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

#define MAX_DEVICE_ID_LEN   200
#define BTH_ADDR_HEX_LEN    12

#pragma region REMOVE
//
// Identification information for dynamically enumerated bus children (PDOs)
// 
typedef struct _PDO_IDENTIFICATION_DESCRIPTION
{
    //
    // Mandatory header
    // 
    WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER Header;

    //
    // Client connection context
    // 
    PBTHPS3_CLIENT_CONNECTION ClientConnection;

} PDO_IDENTIFICATION_DESCRIPTION, *PPDO_IDENTIFICATION_DESCRIPTION;

//
// Context data of child device (PDO)
// 
typedef struct _BTHPS3_PDO_DEVICE_CONTEXT
{
    //
    // Client connection context
    // 
    PBTHPS3_CLIENT_CONNECTION ClientConnection;

} BTHPS3_PDO_DEVICE_CONTEXT, *PBTHPS3_PDO_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_PDO_DEVICE_CONTEXT, GetPdoDeviceContext)
#pragma endregion

#pragma region REMOVE
EVT_WDF_CHILD_LIST_CREATE_DEVICE BthPS3_EvtWdfChildListCreateDevice;
#pragma endregion

EVT_WDF_DEVICE_CONTEXT_CLEANUP BthPS3_PDO_EvtDeviceContextCleanup;

EVT_WDF_DEVICE_D0_EXIT BthPS3_PDO_EvtWdfDeviceD0Exit;

EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT BthPS3_PDO_EvtWdfDeviceSelfManagedIoInit;

EVT_WDF_REQUEST_COMPLETION_ROUTINE BthPS3_PDO_DisconnectRequestCompleted;

#if KMDF_VERSION_MAJOR == 1 && KMDF_VERSION_MINOR >= 13
#if (NTDDI_VERSION < NTDDI_WIN10)
DEFINE_DEVPROPKEY(DEVPKEY_Bluetooth_LastConnectedTime, 
    0x2bd67d8b, 0x8beb, 0x48d5, 0x87, 0xe0, 0x6c, 0xda, 0x34, 0x28, 0x04, 0x0a, 11);    // DEVPROP_TYPE_FILETIME
#endif
#endif

NTSTATUS
BthPS3_AssignDeviceProperty(
    WDFDEVICE Device,
    const DEVPROPKEY* PropertyKey,
    DEVPROPTYPE Type,
    ULONG Size,
    PVOID Data
);

//
// The new fun
// 

//
// TODO: this deprecates struct _BTHPS3_CLIENT_CONNECTION
// 
typedef struct _BTHPS3_PDO_CONTEXT
{
    PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr;

    BTH_ADDR RemoteAddress;

    UNICODE_STRING RemoteName;

    DS_DEVICE_TYPE DeviceType;

    BTHPS3_CLIENT_L2CAP_CHANNEL HidControlChannel;

    BTHPS3_CLIENT_L2CAP_CHANNEL HidInterruptChannel;

    DMFMODULE DmfModuleIoctlHandler;

    ULONG SerialNumber;

    WDFMEMORY HardwareId;

} BTHPS3_PDO_CONTEXT, *PBTHPS3_PDO_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_PDO_CONTEXT, GetPdoContext)


_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_PDO_Create(
    _In_ PBTHPS3_SERVER_CONTEXT Context,
    _In_ BTH_ADDR RemoteAddress,
    _In_ DS_DEVICE_TYPE DeviceType,
    _In_ PFN_WDF_OBJECT_CONTEXT_CLEANUP CleanupCallback,
    _Out_ PBTHPS3_PDO_CONTEXT *PdoContext
);

NTSTATUS
BthPS3_PDO_RetrieveByBthAddr(
    _In_ PBTHPS3_SERVER_CONTEXT Context,
    _In_ BTH_ADDR RemoteAddress,
    _Out_ PBTHPS3_PDO_CONTEXT *PdoContext
);

VOID
BthPS3_PDO_Destroy(
    _In_ PBTHPS3_DEVICE_CONTEXT_HEADER Context,
    _In_ PBTHPS3_PDO_CONTEXT ClientConnection
);

EVT_DMF_DEVICE_MODULES_ADD BthPS3_PDO_EvtDmfModulesAdd;

EVT_DMF_Pdo_PreCreate BthPS3_PDO_EvtPreCreate;

EVT_DMF_Pdo_PostCreate BthPS3_PDO_EvtPostCreate;

EVT_DMF_IoctlHandler_Callback BthPS3_PDO_HandleHidControlRead;

EVT_DMF_IoctlHandler_Callback BthPS3_PDO_HandleHidControlWrite;

EVT_DMF_IoctlHandler_Callback BthPS3_PDO_HandleHidInterruptRead;

EVT_DMF_IoctlHandler_Callback BthPS3_PDO_HandleHidInterruptWrite;
