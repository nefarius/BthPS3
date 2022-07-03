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


#include "Driver.h"
#include "Bluetooth.tmh"
#include <bthguid.h>
#include "BthPS3.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3_QueryInterfaces)
#pragma alloc_text (PAGE, BthPS3_Initialize)
#endif

 //
 // Requests information about underlying radio
 // 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RetrieveLocalInfo(
	_In_ PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr
)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct _BRB_GET_LOCAL_BD_ADDR* brb = NULL;
	UCHAR hciVersion;

	FuncEntry(TRACE_BTH);

	do
	{
		brb = (struct _BRB_GET_LOCAL_BD_ADDR*)
			DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
				BRB_HCI_GET_LOCAL_BD_ADDR,
				POOLTAG_BTHPS3
			);

		if (brb == NULL)
		{
			status = STATUS_INSUFFICIENT_RESOURCES;

			TraceError(
				TRACE_BTH,
				"Failed to allocate brb BRB_HCI_GET_LOCAL_BD_ADDR with status %!STATUS!",
				status
			);

			break;
		}

		if (!NT_SUCCESS(status = BthPS3_SendBrbSynchronously(
			DevCtxHdr->IoTarget,
			DevCtxHdr->HostInitRequest,
			(PBRB)brb,
			sizeof(*brb)
		)))
		{
			TraceError(
				TRACE_BTH,
				"Retrieving local bth address failed with status %!STATUS!",
				status
			);
		}
		else
		{
			DevCtxHdr->LocalBthAddr = brb->BtAddress;
		}

		DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);

		//
		// Verify HCI major version is high enough
		// 
		if (!NT_SUCCESS(status = BthPS3_GetHciVersion(
			DevCtxHdr->IoTarget,
			&hciVersion,
			NULL
		)))
		{
			TraceEvents(TRACE_LEVEL_WARNING, TRACE_BTH,
				"Retrieving HCI major version failed, status %!STATUS!",
				status
			);
			//
			// Can still operate without this information
			// 
			status = STATUS_SUCCESS;
		}
		else
		{
			TraceInformation(
				TRACE_BTH,
				"Host radio HCI major version %d",
				hciVersion
			);

			if (hciVersion < BTHPS3_MIN_SUPPORTED_HCI_MAJOR_VERSION)
			{
				TraceError(
					TRACE_BTH,
					"Host radio HCI major version %d too low, can't continue",
					hciVersion
				);

				//
				// Fail initialization
				// 
				status = STATUS_NOT_SUPPORTED;
			}
		}
	} while (FALSE);

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3_DeviceContextHeaderInit(
	PBTHPS3_DEVICE_CONTEXT_HEADER Header,
	WDFDEVICE Device
)
{
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES attributes;

	FuncEntry(TRACE_BTH);

	Header->Device = Device;

	Header->IoTarget = WdfDeviceGetIoTarget(Device);

	//
	// Initialize request object
	//

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = Device;

	if (!NT_SUCCESS(status = WdfRequestCreate(
		&attributes,
		Header->IoTarget,
		&Header->HostInitRequest
	)))
	{
		TraceError(
			TRACE_BTH,
			"Failed to pre-allocate request in device context with status %!STATUS!",
			status
		);
	}

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}

//
// Initialize all members of the server device context
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_ServerContextInit(
	PBTHPS3_SERVER_CONTEXT Context,
	WDFDEVICE Device
)
{
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_TIMER_CONFIG timerCfg;

	FuncEntry(TRACE_BTH);

	do 
	{
		//
		// Initialize crucial header struct first
		// 
		if (!NT_SUCCESS(status = BthPS3_DeviceContextHeaderInit(&Context->Header, Device)))
		{
			break;
		}

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = Device;

		if (!NT_SUCCESS(status = WdfSpinLockCreate(
			&attributes,
			&Context->Header.ClientConnectionsLock
		)))
		{
			break;
		}

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = Device;

		if (!NT_SUCCESS(status = WdfCollectionCreate(
			&attributes,
			&Context->Header.ClientConnections
		)))
		{
			break;
		}

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = Device;

		WDF_TIMER_CONFIG_INIT(&timerCfg, BthPS3_EnablePatchEvtWdfTimer);

		if (!NT_SUCCESS(status = WdfTimerCreate(
			&timerCfg,
			&attributes,
			&Context->PsmFilter.AutoResetTimer
		)))
		{
			break;
		}

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = Device;

		if (!NT_SUCCESS(status = WdfSpinLockCreate(
			&attributes,
			&Context->Header.SlotsSpinLock
		)))
		{
			break;
		}

		if (!NT_SUCCESS(status = WdfCollectionCreate(
			&attributes,
			&Context->Settings.SIXAXISSupportedNames
		)))
		{
			break;
		}

		if (!NT_SUCCESS(status = WdfCollectionCreate(
			&attributes,
			&Context->Settings.NAVIGATIONSupportedNames
		)))
		{
			break;
		}

		if (!NT_SUCCESS(status = WdfCollectionCreate(
			&attributes,
			&Context->Settings.MOTIONSupportedNames
		)))
		{
			break;
		}

		if (!NT_SUCCESS(status = WdfCollectionCreate(
			&attributes,
			&Context->Settings.WIRELESSSupportedNames
		)))
		{
			break;
		}
		
		//
		// Query registry for dynamic values
		// 
		status = BthPS3_SettingsContextInit(Context);

	} while (FALSE);

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}

//
// Read runtime properties from registry
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_SettingsContextInit(
	PBTHPS3_SERVER_CONTEXT Context
)
{
	NTSTATUS                status;
	WDFKEY                  hKey = NULL;
	WDF_OBJECT_ATTRIBUTES   attribs;

	DECLARE_CONST_UNICODE_STRING(autoEnableFilter, BTHPS3_REG_VALUE_AUTO_ENABLE_FILTER);
	DECLARE_CONST_UNICODE_STRING(autoDisableFilter, BTHPS3_REG_VALUE_AUTO_DISABLE_FILTER);
	DECLARE_CONST_UNICODE_STRING(autoEnableFilterDelay, BTHPS3_REG_VALUE_AUTO_ENABLE_FILTER_DELAY);

	DECLARE_CONST_UNICODE_STRING(isSIXAXISSupported, BTHPS3_REG_VALUE_IS_SIXAXIS_SUPPORTED);
	DECLARE_CONST_UNICODE_STRING(isNAVIGATIONSupported, BTHPS3_REG_VALUE_IS_NAVIGATION_SUPPORTED);
	DECLARE_CONST_UNICODE_STRING(isMOTIONSupported, BTHPS3_REG_VALUE_IS_MOTION_SUPPORTED);
	DECLARE_CONST_UNICODE_STRING(isWIRELESSSupported, BTHPS3_REG_VALUE_IS_WIRELESS_SUPPORTED);

	DECLARE_CONST_UNICODE_STRING(SIXAXISSupportedNames, BTHPS3_REG_VALUE_SIXAXIS_SUPPORTED_NAMES);
	DECLARE_CONST_UNICODE_STRING(NAVIGATIONSupportedNames, BTHPS3_REG_VALUE_NAVIGATION_SUPPORTED_NAMES);
	DECLARE_CONST_UNICODE_STRING(MOTIONSupportedNames, BTHPS3_REG_VALUE_MOTION_SUPPORTED_NAMES);
	DECLARE_CONST_UNICODE_STRING(WIRELESSSupportedNames, BTHPS3_REG_VALUE_WIRELESS_SUPPORTED_NAMES);

	//
	// Set default values
	//
	Context->Settings.AutoEnableFilter = TRUE;
	Context->Settings.AutoDisableFilter = TRUE;
	Context->Settings.AutoEnableFilterDelay = 10; // Seconds

	Context->Settings.IsSIXAXISSupported = TRUE;
	Context->Settings.IsNAVIGATIONSupported = TRUE;
	Context->Settings.IsMOTIONSupported = TRUE;
	Context->Settings.IsWIRELESSSupported = TRUE;

	//
	// Open
	//   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\BthPS3\Parameters
	// key
	// 
	status = WdfDriverOpenParametersRegistryKey(
		WdfGetDriver(),
		STANDARD_RIGHTS_ALL,
		WDF_NO_OBJECT_ATTRIBUTES,
		&hKey
	);

	//
	// On success, read configuration values
	// 
	if (NT_SUCCESS(status))
	{
		//
		// Don't care, if it fails, keep default value
		// 
		(void)WdfRegistryQueryULong(
			hKey,
			&autoEnableFilter,
			&Context->Settings.AutoEnableFilter
		);

		(void)WdfRegistryQueryULong(
			hKey,
			&autoDisableFilter,
			&Context->Settings.AutoDisableFilter
		);

		(void)WdfRegistryQueryULong(
			hKey,
			&autoEnableFilterDelay,
			&Context->Settings.AutoEnableFilterDelay
		);

		(void)WdfRegistryQueryULong(
			hKey,
			&isSIXAXISSupported,
			&Context->Settings.IsSIXAXISSupported
		);

		(void)WdfRegistryQueryULong(
			hKey,
			&isNAVIGATIONSupported,
			&Context->Settings.IsNAVIGATIONSupported
		);

		(void)WdfRegistryQueryULong(
			hKey,
			&isMOTIONSupported,
			&Context->Settings.IsMOTIONSupported
		);

		(void)WdfRegistryQueryULong(
			hKey,
			&isWIRELESSSupported,
			&Context->Settings.IsWIRELESSSupported
		);

		WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
		attribs.ParentObject = Context->Settings.SIXAXISSupportedNames;
		(void)WdfRegistryQueryMultiString(
			hKey,
			&SIXAXISSupportedNames,
			&attribs,
			Context->Settings.SIXAXISSupportedNames
		);

		WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
		attribs.ParentObject = Context->Settings.NAVIGATIONSupportedNames;
		(void)WdfRegistryQueryMultiString(
			hKey,
			&NAVIGATIONSupportedNames,
			&attribs,
			Context->Settings.NAVIGATIONSupportedNames
		);

		WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
		attribs.ParentObject = Context->Settings.MOTIONSupportedNames;
		(void)WdfRegistryQueryMultiString(
			hKey,
			&MOTIONSupportedNames,
			&attribs,
			Context->Settings.MOTIONSupportedNames
		);

		WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
		attribs.ParentObject = Context->Settings.WIRELESSSupportedNames;
		(void)WdfRegistryQueryMultiString(
			hKey,
			&WIRELESSSupportedNames,
			&attribs,
			Context->Settings.WIRELESSSupportedNames
		);

		WdfRegistryClose(hKey);
	}

	return status;
}

//
// Grabs driver-to-driver interface
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_QueryInterfaces(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
	NTSTATUS status;

	PAGED_CODE();

	if (!NT_SUCCESS(status = WdfFdoQueryForInterface(
		DevCtx->Header.Device,
		&GUID_BTHDDI_PROFILE_DRIVER_INTERFACE,
		(PINTERFACE)(&DevCtx->Header.ProfileDrvInterface),
		sizeof(DevCtx->Header.ProfileDrvInterface),
		BTHDDI_PROFILE_DRIVER_INTERFACE_VERSION_FOR_QI,
		NULL
	)))
	{
		TraceError(
			TRACE_BTH,
			"QueryInterface failed for Interface profile driver interface, version %d, Status code %!STATUS!",
			BTHDDI_PROFILE_DRIVER_INTERFACE_VERSION_FOR_QI,
			status
		);
	}

	return status;
}

//
// Initialize Bluetooth driver-to-driver interface
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_Initialize(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
	PAGED_CODE();

	return BthPS3_QueryInterfaces(DevCtx);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_GetDeviceName(
	WDFIOTARGET IoTarget,
	BTH_ADDR RemoteAddress,
	PCHAR Name
)
{
	FuncEntry(TRACE_BTH);

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
		if (MemoryHandle != NULL)
		{
			WdfObjectDelete(MemoryHandle);
		}

		if (!NT_SUCCESS(status = WdfMemoryCreate(NULL,
			NonPagedPoolNx,
			POOLTAG_BTHPS3,
			sizeof(BTH_DEVICE_INFO_LIST) + (sizeof(BTH_DEVICE_INFO) * maxDevices),
			&MemoryHandle,
			NULL)))
		{
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

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_GetHciVersion(
	_In_ WDFIOTARGET IoTarget,
	_Out_ PUCHAR HciVersion,
	_Out_opt_ PUSHORT HciRevision
)
{
	FuncEntry(TRACE_BTH);

	NTSTATUS status;
	WDF_MEMORY_DESCRIPTOR MemoryDescriptor;
	WDFMEMORY MemoryHandle = NULL;
	PBTH_LOCAL_RADIO_INFO pLocalInfo = NULL;
#ifdef _M_IX86
	ULONG bytesReturned;
#else
	ULONGLONG bytesReturned;
#endif
	
	if (!NT_SUCCESS(status = WdfMemoryCreate(NULL,
		NonPagedPoolNx,
		POOLTAG_BTHPS3,
		sizeof(BTH_LOCAL_RADIO_INFO),
		&MemoryHandle,
		NULL))) 
	{
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

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}
