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

#include <bthdef.h>
#include <bthguid.h>
#include <bthioctl.h>
#include <sdpnode.h>
#include <bthddi.h>
#include <bthsdpddi.h>
#include <bthsdpdef.h>
#include <bthguid.h>

#define POOLTAG_BTHPS3                  '3SPB'
#define BTH_DEVICE_INFO_MAX_COUNT       0x0A
#define BTH_DEVICE_INFO_MAX_RETRIES     0x05
#define BTHPS3_MAX_NUM_DEVICES			0xFF
#define BTHPS3_BTH_ADDR_MAX_CHARS		13 /* 12 characters + NULL terminator */


typedef struct _BTHPS3_DEVICE_CONTEXT_HEADER
{
	//
	// Framework device this context is associated with
	//
	WDFDEVICE Device;

	//
	// Default I/O target
	//
	WDFIOTARGET IoTarget;

	//
	// Profile driver interface which contains profile driver DDI
	//
	BTH_PROFILE_DRIVER_INTERFACE ProfileDrvInterface;

	//
	// Local Bluetooth Address
	//
	BTH_ADDR LocalBthAddr;

	//
	// Preallocated request to be reused during initialization/deinitialization phase
	// Access to this request is not synchronized
	//
	WDFREQUEST HostInitRequest;

	//
	// Collection of state information about 
	// currently established connections
	// 
	WDFCOLLECTION Clients;

	//
	// Lock for ClientConnections collection
	// 
	WDFSPINLOCK ClientsLock;

	//
	// DMF module to handle PDO creation
	// 
	DMFMODULE PdoModule;

	//
	// Free and occupied serial numbers
	// 
	UINT32 Slots[8]; // 256 usable bits

	//
	// Lock protecting Slots access
	// 
	WDFSPINLOCK SlotsSpinLock;

} BTHPS3_DEVICE_CONTEXT_HEADER, * PBTHPS3_DEVICE_CONTEXT_HEADER;

typedef struct _BTHPS3_SERVER_CONTEXT
{
	//
	// Context common to client and server
	//
	BTHPS3_DEVICE_CONTEXT_HEADER Header;

	//
	// Artificial HID Control PSM
	// 
	USHORT PsmHidControl;

	//
	// Artificial HID Interrupt PSM
	// 
	USHORT PsmHidInterrupt;

	//
	// Handle obtained by registering L2CAP server
	//
	L2CAP_SERVER_HANDLE L2CAPServerHandle;

	//
	// BRB used for server and PSM register and unregister
	//
	// Server and PSM register and unregister must be done
	// sequentially since access to this brb is not
	// synchronized.
	//
	struct _BRB RegisterUnregisterBrb;

	struct
	{
		//
		// Remote I/O target (PSM filter driver)
		// 
		WDFIOTARGET IoTarget;

		//
		// Delayed action to re-enable filter patch
		// 
		WDFTIMER AutoResetTimer;

		//
		// Request object used to asynchronously enable the patch
		// 
		WDFREQUEST AsyncRequest;

	} PsmFilter;

	struct
	{
		ULONG AutoEnableFilter;

		ULONG AutoDisableFilter;

		ULONG AutoEnableFilterDelay;

		ULONG IsSIXAXISSupported;

		ULONG IsNAVIGATIONSupported;

		ULONG IsMOTIONSupported;

		ULONG IsWIRELESSSupported;

		WDFCOLLECTION SIXAXISSupportedNames;

		WDFCOLLECTION NAVIGATIONSupportedNames;

		WDFCOLLECTION MOTIONSupportedNames;

		WDFCOLLECTION WIRELESSSupportedNames;

	} Settings;

} BTHPS3_SERVER_CONTEXT, * PBTHPS3_SERVER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_SERVER_CONTEXT, GetServerDeviceContext)

typedef struct _BTHPS3_PDO_CONTEXT * PBTHPS3_PDO_CONTEXT;

//
// Context data for work item (dropping from DISPATCH to PASSIVE level)
// 
typedef struct _BTHPS3_REMOTE_CONNECT_CONTEXT
{
	PBTHPS3_SERVER_CONTEXT ServerContext;

	INDICATION_PARAMETERS IndicationParameters;

} BTHPS3_REMOTE_CONNECT_CONTEXT, * PBTHPS3_REMOTE_CONNECT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_REMOTE_CONNECT_CONTEXT, GetRemoteConnectContext)


//
// Context data for work item (dropping from DISPATCH to PASSIVE level)
// 
typedef struct _BTHPS3_REMOTE_DISCONNECT_CONTEXT
{
	PBTHPS3_PDO_CONTEXT PdoContext;

	INDICATION_PARAMETERS IndicationParameters;

} BTHPS3_REMOTE_DISCONNECT_CONTEXT, * PBTHPS3_REMOTE_DISCONNECT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_REMOTE_DISCONNECT_CONTEXT, GetRemoteDisconnectContext)


EVT_WDF_TIMER BthPS3_EnablePatchEvtWdfTimer;


_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RetrieveLocalInfo(
	_In_ PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr
);

#pragma region PSM Registration

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RegisterPSM(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3_UnregisterPSM(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

#pragma endregion

#pragma region L2CAP Server

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RegisterL2CAPServer(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3_UnregisterL2CAPServer(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

#pragma endregion

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3_DeviceContextHeaderInit(
	PBTHPS3_DEVICE_CONTEXT_HEADER Header,
	WDFDEVICE Device
);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_ServerContextInit(
	PBTHPS3_SERVER_CONTEXT Context,
	WDFDEVICE Device
);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_SettingsContextInit(
	PBTHPS3_SERVER_CONTEXT Context
);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_QueryInterfaces(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_Initialize(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(DISPATCH_LEVEL)
void
BthPS3_IndicationCallback(
	_In_ PVOID Context,
	_In_ INDICATION_CODE Indication,
	_In_ PINDICATION_PARAMETERS Parameters
);

#pragma region BRB Request submission

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_SendBrbSynchronously(
	_In_ WDFIOTARGET IoTarget,
	_In_ WDFREQUEST Request,
	_In_ PBRB Brb,
	_In_ ULONG BrbSize
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3_SendBrbAsync(
	_In_ WDFIOTARGET IoTarget,
	_In_ WDFREQUEST Request,
	_In_ PBRB Brb,
	_In_ size_t BrbSize,
	_In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE ComplRoutine,
	_In_opt_ WDFCONTEXT Context
);

#pragma endregion

//
// Request remote device friendly name from radio
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_GetDeviceName(
	WDFIOTARGET IoTarget,
	BTH_ADDR RemoteAddress,
	PCHAR Name
);

//
// Request HCI version from radio
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_GetHciVersion(
	_In_ WDFIOTARGET IoTarget,
	_Out_ PUCHAR HciMajorVersion,
	_Out_opt_ PUSHORT HciRevision
);
