/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2021, Nefarius Software Solutions e.U.                      *
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
	WDFCOLLECTION ClientConnections;

	//
	// Lock for ClientConnections collection
	// 
	WDFSPINLOCK ClientConnectionsLock;

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

typedef struct _BTHPS3_REMOTE_CONNECT_CONTEXT
{
	PBTHPS3_SERVER_CONTEXT ServerContext;

	INDICATION_PARAMETERS IndicationParameters;

} BTHPS3_REMOTE_CONNECT_CONTEXT, * PBTHPS3_REMOTE_CONNECT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_REMOTE_CONNECT_CONTEXT, GetRemoteConnectContext)

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
FORCEINLINE
BTHPS3_GET_DEVICE_NAME(
	WDFIOTARGET IoTarget,
	BTH_ADDR RemoteAddress,
	PCHAR Name
)
{
	NTSTATUS status = STATUS_INVALID_BUFFER_SIZE;
	ULONG index = 0;
	WDF_MEMORY_DESCRIPTOR MemoryDescriptor;
	WDFMEMORY MemoryHandle = NULL;
	PBTH_DEVICE_INFO_LIST pDeviceInfoList = NULL;
	ULONG maxDevices = BTH_DEVICE_INFO_MAX_COUNT;
	ULONG retryCount = 0;

	//
	// Retry increasing the buffer a few times if _a lot_ of devices
	// are cached and the allocated memory can't store them all.
	// 
	for (retryCount = 0; (retryCount <= BTH_DEVICE_INFO_MAX_RETRIES
		&& status == STATUS_INVALID_BUFFER_SIZE); retryCount++)
	{
		if (MemoryHandle != NULL) {
			WdfObjectDelete(MemoryHandle);
		}

		status = WdfMemoryCreate(NULL,
			NonPagedPoolNx,
			POOLTAG_BTHPS3,
			sizeof(BTH_DEVICE_INFO_LIST) + (sizeof(BTH_DEVICE_INFO) * maxDevices),
			&MemoryHandle,
			NULL);

		if (!NT_SUCCESS(status)) {
			return status;
		}

		WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(
			&MemoryDescriptor,
			MemoryHandle,
			NULL
		);

		status = WdfIoTargetSendIoctlSynchronously(
			IoTarget,
			NULL,
			IOCTL_BTH_GET_DEVICE_INFO,
			&MemoryDescriptor,
			&MemoryDescriptor,
			NULL,
			NULL
		);

		//
		// Increase memory to allocate
		// 
		maxDevices += BTH_DEVICE_INFO_MAX_COUNT;
	}

	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(MemoryHandle);
		return status;
	}

	pDeviceInfoList = WdfMemoryGetBuffer(MemoryHandle, NULL);
	status = STATUS_NOT_FOUND;

	for (index = 0; index < pDeviceInfoList->numOfDevices; index++)
	{
		PBTH_DEVICE_INFO pDeviceInfo = &pDeviceInfoList->deviceList[index];

		if (pDeviceInfo->address == RemoteAddress)
		{
			if (strlen(pDeviceInfo->name) == 0)
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			strcpy_s(Name, BTH_MAX_NAME_SIZE, pDeviceInfo->name);
			status = STATUS_SUCCESS;
			break;
		}
	}

	WdfObjectDelete(MemoryHandle);
	return status;
}

//
// Request remote device friendly name from radio
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FORCEINLINE
BTHPS3_GET_HCI_VERSION(
	_In_ WDFIOTARGET IoTarget,
	_Out_ PUCHAR HciVersion,
	_Out_opt_ PUSHORT HciRevision
)
{
	NTSTATUS status;
	WDF_MEMORY_DESCRIPTOR MemoryDescriptor;
	WDFMEMORY MemoryHandle = NULL;
	PBTH_LOCAL_RADIO_INFO pLocalInfo = NULL;
#ifdef _M_IX86
	ULONG bytesReturned;
#else
	ULONGLONG bytesReturned;
#endif
	
	status = WdfMemoryCreate(NULL,
		NonPagedPoolNx,
		POOLTAG_BTHPS3,
		sizeof(BTH_LOCAL_RADIO_INFO),
		&MemoryHandle,
		NULL);

	if (!NT_SUCCESS(status)) {
		return status;
	}

	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(
		&MemoryDescriptor,
		MemoryHandle,
		NULL
	);

	status = WdfIoTargetSendIoctlSynchronously(
		IoTarget,
		NULL,
		IOCTL_BTH_GET_LOCAL_INFO,
		&MemoryDescriptor,
		&MemoryDescriptor,
		NULL,
		&bytesReturned
	);

	if (!NT_SUCCESS(status) || bytesReturned < sizeof(BTH_LOCAL_RADIO_INFO)) {
		WdfObjectDelete(MemoryHandle);
		return status;
	}

	pLocalInfo = WdfMemoryGetBuffer(MemoryHandle, NULL);

	if (HciVersion)
		*HciVersion = pLocalInfo->hciVersion;
		
	if (HciRevision)
		*HciRevision = pLocalInfo->hciRevision;

	WdfObjectDelete(MemoryHandle);
	return status;
}
