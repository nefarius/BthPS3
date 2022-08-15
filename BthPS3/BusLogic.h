/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2022, Nefarius Software Solutions e.U.                      *
 * All rights reserved.                                                           *
 *                                                                                *
 * Redistribution and use in source and binary forms, with or without             *
 * modification, are permitted provided that the following conditions are met:    *
 *                                                                                *
 * 1. Redistributions of source code must retain the above copyright notice, this *
 *    list of conditions and the following disclaimer.                            *
 *                                                                                *
 * 2. Redistributions in binary form must reproduce the above copyright notice,   *
 *    this list of conditions and the following disclaimer in the documentation   *
 *    and/or other materials provided with the distribution.                      *
 *                                                                                *
 * 3. Neither the name of the copyright holder nor the names of its               *
 *    contributors may be used to endorse or promote products derived from        *
 *    this software without specific prior written permission.                    *
 *                                                                                *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"    *
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE      *
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE *
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE   *
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL     *
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR     *
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER     *
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  *
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  *
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.           *
 *                                                                                *
 **********************************************************************************/


#pragma once

#define MAX_DEVICE_ID_LEN				200
#define BTH_ADDR_HEX_LEN				12
#define REG_CACHED_DEVICE_KEY_FMT		L"Devices\\%012llX"
#define REG_CACHED_DEVICE_KEY_FMT_LEN	(8 + BTHPS3_BTH_ADDR_MAX_CHARS)


//
// Connection state
//
typedef enum _BTHPS3_CONNECTION_STATE {
    ConnectionStateUninitialized = 0,
    ConnectionStateInitialized,
    ConnectionStateConnecting,
    ConnectionStateConnected,
    ConnectionStateConnectFailed,
    ConnectionStateDisconnecting,
    ConnectionStateDisconnected

} BTHPS3_CONNECTION_STATE, *PBTHPS3_CONNECTION_STATE;

//
// State information for a single L2CAP channel
// 
typedef struct _BTHPS3_CLIENT_L2CAP_CHANNEL
{
    BTHPS3_CONNECTION_STATE ConnectionState;

    WDFSPINLOCK ConnectionStateLock;

    L2CAP_CHANNEL_HANDLE ChannelHandle;

    struct _BRB ConnectDisconnectBrb;

    WDFREQUEST ConnectDisconnectRequest;

    KEVENT DisconnectEvent;

} BTHPS3_CLIENT_L2CAP_CHANNEL, *PBTHPS3_CLIENT_L2CAP_CHANNEL;

//
// PDO context object holding all state information per child device
// 
typedef struct _BTHPS3_PDO_CONTEXT
{
	PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr;

	BTH_ADDR RemoteAddress;

	DS_DEVICE_TYPE DeviceType;

	BTHPS3_CLIENT_L2CAP_CHANNEL HidControlChannel;

	BTHPS3_CLIENT_L2CAP_CHANNEL HidInterruptChannel;

	DMFMODULE DmfModuleIoctlHandler;

	ULONG SerialNumber;

	WDFMEMORY HardwareId;

	struct
	{
		WDFQUEUE HidControlReadRequests;

		WDFQUEUE HidControlWriteRequests;

		WDFQUEUE HidInterruptReadRequests;

		WDFQUEUE HidInterruptWriteRequests;

	} Queues;

} BTHPS3_PDO_CONTEXT, * PBTHPS3_PDO_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_PDO_CONTEXT, GetPdoContext)


VOID
FORCEINLINE
CLIENT_CONNECTION_REQUEST_REUSE(
    _In_ WDFREQUEST Request
)
{
    NTSTATUS statusReuse;
    WDF_REQUEST_REUSE_PARAMS reuseParams;

    WDF_REQUEST_REUSE_PARAMS_INIT(&reuseParams, WDF_REQUEST_REUSE_NO_FLAGS, STATUS_NOT_SUPPORTED);
    statusReuse = WdfRequestReuse(Request, &reuseParams);
    NT_ASSERT(NT_SUCCESS(statusReuse));
    UNREFERENCED_PARAMETER(statusReuse);
}

//
// PDO lifecycle
// 

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_PDO_Create(
	_In_ PBTHPS3_SERVER_CONTEXT Context,
	_In_ BTH_ADDR RemoteAddress,
	_In_ DS_DEVICE_TYPE DeviceType,
	_In_ PSTR RemoteName,
	_Out_ PBTHPS3_PDO_CONTEXT* PdoContext
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3_PDO_RetrieveByBthAddr(
	_In_ PBTHPS3_SERVER_CONTEXT Context,
	_In_ BTH_ADDR RemoteAddress,
	_Out_ PBTHPS3_PDO_CONTEXT* PdoContext
);

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3_PDO_Destroy(
	_In_ PBTHPS3_DEVICE_CONTEXT_HEADER Context,
	_In_ PBTHPS3_PDO_CONTEXT PdoContext
);

//
// Clean-up
// 

EVT_WDF_OBJECT_CONTEXT_CLEANUP BthPS3_PDO_EvtContextCleanup;

//
// DMF
// 

EVT_DMF_DEVICE_MODULES_ADD BthPS3_PDO_EvtDmfModulesAdd;

//
// PDO pre-/post-create callbacks
// 

EVT_DMF_Pdo_PreCreate BthPS3_PDO_EvtPreCreate;

EVT_DMF_Pdo_PostCreate BthPS3_PDO_EvtPostCreate;

//
// Handle requests from upper driver
// 

EVT_DMF_IoctlHandler_Callback BthPS3_PDO_HandleHidControlRead;

EVT_DMF_IoctlHandler_Callback BthPS3_PDO_HandleHidControlWrite;

EVT_DMF_IoctlHandler_Callback BthPS3_PDO_HandleHidInterruptRead;

EVT_DMF_IoctlHandler_Callback BthPS3_PDO_HandleHidInterruptWrite;

EVT_DMF_IoctlHandler_Callback BthPS3_PDO_HandleBthDisconnect;

//
// Process requests once queued
// 

EVT_WDF_IO_QUEUE_STATE BthPS3_PDO_DispatchHidControlRead;

EVT_WDF_IO_QUEUE_STATE BthPS3_PDO_DispatchHidControlWrite;

EVT_WDF_IO_QUEUE_STATE BthPS3_PDO_DispatchHidInterruptRead;

EVT_WDF_IO_QUEUE_STATE BthPS3_PDO_DispatchHidInterruptWrite;

//
// PNP/Power
// 

EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT BthPS3_PDO_SelfManagedIoInit;

//
// I/O completion
// 

EVT_WDF_REQUEST_COMPLETION_ROUTINE BthPS3_PDO_DisconnectRequestCompleted;

//
// Registry operations
// 

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_PDO_QuerySlot(
	PBTHPS3_DEVICE_CONTEXT_HEADER Header,
	BTH_ADDR RemoteAddress,
	PULONG Slot
);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_PDO_AssignSlot(
	PBTHPS3_DEVICE_CONTEXT_HEADER Header,
	BTH_ADDR RemoteAddress,
	ULONG Slot
);
