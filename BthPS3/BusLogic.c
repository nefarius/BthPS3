/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2026, Nefarius Software Solutions e.U.                      *
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
#include "BusLogic.tmh"
#include "BthPS3ETW.h"


 //
 // IOCTLs handled by each PDO
 // 
IoctlHandler_IoctlRecord G_PDO_IoctlSpecification[] =
{
	/* HID channels traffic */
	{IOCTL_BTHPS3_HID_CONTROL_READ, 0, 1, BthPS3_PDO_HandleHidControlRead},
	{IOCTL_BTHPS3_HID_CONTROL_WRITE, 1, 0, BthPS3_PDO_HandleHidControlWrite},
	{IOCTL_BTHPS3_HID_INTERRUPT_READ, 0, 1, BthPS3_PDO_HandleHidInterruptRead},
	{IOCTL_BTHPS3_HID_INTERRUPT_WRITE, 1, 0, BthPS3_PDO_HandleHidInterruptWrite},
	/* Disconnect instruction (e.g. from DsHidMini) */
	{IOCTL_BTH_DISCONNECT_DEVICE, sizeof(BTH_ADDR), 0, BthPS3_PDO_HandleBthDisconnect},
};


//
// Called when initializing DMF modules for the PDO
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3_PDO_EvtDmfModulesAdd(
	_In_ WDFDEVICE Device,
	_In_ PDMFMODULE_INIT DmfModuleInit
)
{
	FuncEntry(TRACE_BUSLOGIC);

	DMF_MODULE_ATTRIBUTES moduleAttributes;
	DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;

	const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(Device);

	//
	// IOCTL Handler Module
	// 

	DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(
		&moduleConfigIoctlHandler,
		&moduleAttributes
	);

	moduleConfigIoctlHandler.DeviceInterfaceGuid = GUID_DEVINTERFACE_BTHPS3;
	moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeDefault;
	moduleConfigIoctlHandler.EvtIoctlHandlerAccessModeFilter = NULL;
	moduleConfigIoctlHandler.IoctlRecordCount = ARRAYSIZE(G_PDO_IoctlSpecification);
	moduleConfigIoctlHandler.IoctlRecords = G_PDO_IoctlSpecification;
	moduleConfigIoctlHandler.ForwardUnhandledRequests = FALSE;
	moduleConfigIoctlHandler.ManualMode = TRUE;

	DMF_DmfModuleAdd(
		DmfModuleInit,
		&moduleAttributes,
		WDF_NO_OBJECT_ATTRIBUTES,
		&pPdoCtx->DmfModuleIoctlHandler
	);

	//
	// Per-PDO rundown: callbacks and BRB completions acquire this before
	// touching connection state. Teardown ends it before the PDO is unplugged.
	//
	DMF_Rundown_ATTRIBUTES_INIT(&moduleAttributes);
	DMF_DmfModuleAdd(
		DmfModuleInit,
		&moduleAttributes,
		WDF_NO_OBJECT_ATTRIBUTES,
		&pPdoCtx->DmfModuleRundown
	);

	FuncExitNoReturn(TRACE_BUSLOGIC);
}

//
// Creates a new PDO and connection context for a given remote address
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
_Success_(return == STATUS_SUCCESS)
NTSTATUS
BthPS3_PDO_Create(
	_In_ PBTHPS3_SERVER_CONTEXT Context,
	_In_ BTH_ADDR RemoteAddress,
	_In_ DS_DEVICE_TYPE DeviceType,
	_In_ PSTR RemoteName,
	_Outptr_result_maybenull_ BTHPS3_PDO_CONTEXT** PdoContext
)
{
	FuncEntry(TRACE_BUSLOGIC);

	NTSTATUS status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES attributes;
	PDO_RECORD record;
	WDFDEVICE device = NULL;
	BOOLEAN pdoPlugged = FALSE;
	UNICODE_STRING guidString = { 0 };
	WCHAR devAddr[BTHPS3_BTH_ADDR_MAX_CHARS]; // MAC address in hex format including NULL terminator
	PWSTR manufacturer = L"Nefarius Software Solutions e.U.";
	LARGE_INTEGER lastConnectionTime;
	WDFKEY hKey = NULL;
	ULONG rawPdo = 0;

    *PdoContext = NULL;

	DECLARE_UNICODE_STRING_SIZE(hardwareId, MAX_DEVICE_ID_LEN);
	DECLARE_UNICODE_STRING_SIZE(remotenameWide, BTH_MAX_NAME_SIZE);
	DECLARE_CONST_UNICODE_STRING(rawPdoValue, BTHPS3_REG_VALUE_RAW_PDO);

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, BTHPS3_PDO_CONTEXT);

	attributes.EvtCleanupCallback = BthPS3_PDO_EvtContextCleanup;

	KeQuerySystemTimePrecise(&lastConnectionTime);

	RtlZeroMemory(&record, sizeof(PDO_RECORD));

	record.CustomClientContext = &attributes;
	//
	// The PDO itself uses some DMF modules during operation
	// 
	record.EnableDmf = TRUE;
	record.EvtDmfDeviceModulesAdd = BthPS3_PDO_EvtDmfModulesAdd;


	//
	// Open
	//   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\BthPS3\Parameters
	// key
	// 
	status = WdfDriverOpenParametersRegistryKey(
		WdfGetDriver(),
		STANDARD_RIGHTS_READ,
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
			&rawPdoValue,
			&rawPdo
		);
	}

	do
	{
		//
		// Get unique serial
		// 
		if (!NT_SUCCESS(status = BthPS3_PDO_QuerySlot(
			&Context->Header,
			RemoteAddress,
			&record.SerialNumber
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"BthPS3_PDO_QuerySlot failed with status %!STATUS!",
				status
			);
			break;
		}

		//
		// Convert remote name from narrow to wide
		// 
		if (!NT_SUCCESS(status = RtlUnicodeStringPrintf(
			&remotenameWide,
			L"%hs",
			RemoteName
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"RtlUnicodeStringPrintf failed with status %!STATUS!",
				status
			);
			break;
		}

		//
		// Prepare properties
		// 

		if (!NT_SUCCESS(RtlStringCbPrintfW(
			devAddr,
			ARRAYSIZE(devAddr) * sizeof(WCHAR),
			L"%012llX",
			RemoteAddress
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"RtlStringCbPrintfW failed with status %!STATUS!",
				status
			);
			break;
		}

		//
		// Set these device properties for the new PDO
		// TODO: convert from stack to heap allocated!
		// 
		Pdo_DevicePropertyEntry entries[] =
		{
			{
				{sizeof(WDF_DEVICE_PROPERTY_DATA), &DEVPKEY_Bluetooth_DeviceVID, LOCALE_NEUTRAL, PLUGPLAY_PROPERTY_PERSISTENT},
				DEVPROP_TYPE_UINT16,
				NULL,
				sizeof(USHORT),
				FALSE,
				NULL
			},
			{
				{sizeof(WDF_DEVICE_PROPERTY_DATA), &DEVPKEY_Bluetooth_DevicePID, LOCALE_NEUTRAL, PLUGPLAY_PROPERTY_PERSISTENT},
				DEVPROP_TYPE_UINT16,
				NULL,
				sizeof(USHORT),
				FALSE,
				NULL
			},
			{
				{sizeof(WDF_DEVICE_PROPERTY_DATA), &DEVPKEY_Bluetooth_DeviceAddress, LOCALE_NEUTRAL, PLUGPLAY_PROPERTY_PERSISTENT},
				DEVPROP_TYPE_STRING,
				devAddr,
				ARRAYSIZE(devAddr) * sizeof(WCHAR),
				FALSE,
				NULL
			},
			{
				{sizeof(WDF_DEVICE_PROPERTY_DATA), &DEVPKEY_BluetoothRadio_Address, LOCALE_NEUTRAL, PLUGPLAY_PROPERTY_PERSISTENT},
				DEVPROP_TYPE_UINT64,
				&Context->Header.LocalBthAddr,
				sizeof(UINT64),
				FALSE,
				NULL
			},
			{
				{sizeof(WDF_DEVICE_PROPERTY_DATA), &DEVPKEY_Device_FriendlyName, LOCALE_NEUTRAL, PLUGPLAY_PROPERTY_PERSISTENT},
				DEVPROP_TYPE_STRING,
				remotenameWide.Buffer,
				remotenameWide.Length + sizeof(L'\0'),
				FALSE,
				NULL
			},
			{
				{sizeof(WDF_DEVICE_PROPERTY_DATA), &DEVPKEY_Bluetooth_DeviceManufacturer, LOCALE_NEUTRAL, PLUGPLAY_PROPERTY_PERSISTENT},
				DEVPROP_TYPE_STRING,
				manufacturer,
				(ULONG)(wcslen(manufacturer) * sizeof(WCHAR)) + sizeof(L'\0'),
				FALSE,
				NULL
			},
			{
				{sizeof(WDF_DEVICE_PROPERTY_DATA), &DEVPKEY_Bluetooth_LastConnectedTime, LOCALE_NEUTRAL, PLUGPLAY_PROPERTY_PERSISTENT},
				DEVPROP_TYPE_FILETIME,
				&lastConnectionTime,
				sizeof(LARGE_INTEGER),
				FALSE,
				NULL
			},
		};

		//
		// Set VID/PID according to device type
		// 
		switch (DeviceType)
		{
		case DS_DEVICE_TYPE_SIXAXIS:
			entries[0].ValueData = &BTHPS3_SIXAXIS_VID;
			entries[1].ValueData = &BTHPS3_SIXAXIS_PID;
			break;
		case DS_DEVICE_TYPE_NAVIGATION:
			entries[0].ValueData = &BTHPS3_NAVIGATION_VID;
			entries[1].ValueData = &BTHPS3_NAVIGATION_PID;
			break;
		case DS_DEVICE_TYPE_MOTION:
			entries[0].ValueData = &BTHPS3_MOTION_VID;
			entries[1].ValueData = &BTHPS3_MOTION_PID;
			break;
		case DS_DEVICE_TYPE_WIRELESS:
			entries[0].ValueData = &BTHPS3_WIRELESS_VID;
			entries[1].ValueData = &BTHPS3_WIRELESS_PID;
			break;
		case DS_DEVICE_TYPE_UNKNOWN:
		default:  // NOLINT(clang-diagnostic-covered-switch-default)
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		Pdo_DeviceProperty_Table properties;

		properties.ItemCount = ARRAYSIZE(entries);
		properties.TableEntries = entries;

		record.DeviceProperties = &properties;

		//
		// Prepare Hardware ID GUID segment and description based on device type
		// 
		switch (DeviceType)
		{
		case DS_DEVICE_TYPE_SIXAXIS:
			record.Description = L"PLAYSTATION(R)3 Controller";
			status = RtlStringFromGUID(&GUID_BUSENUM_BTHPS3_SIXAXIS,
				&guidString
			);
			break;
		case DS_DEVICE_TYPE_NAVIGATION:
			record.Description = L"Navigation Controller";
			status = RtlStringFromGUID(&GUID_BUSENUM_BTHPS3_NAVIGATION,
				&guidString
			);
			break;
		case DS_DEVICE_TYPE_MOTION:
			record.Description = L"Motion Controller";
			status = RtlStringFromGUID(&GUID_BUSENUM_BTHPS3_MOTION,
				&guidString
			);
			break;
		case DS_DEVICE_TYPE_WIRELESS:
			record.Description = L"Wireless Controller";
			status = RtlStringFromGUID(&GUID_BUSENUM_BTHPS3_WIRELESS,
				&guidString
			);
			break;
		case DS_DEVICE_TYPE_UNKNOWN:
		default:  // NOLINT(clang-diagnostic-covered-switch-default)
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		if (!NT_SUCCESS(status))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"RtlStringFromGUID failed with status %!STATUS!",
				status
			);
			break;
		}

        //
        // Format string to build hardware ID with
        // 
        PCWSTR hardwareIdFormat = L"%ws\\%wZ&Dev&VID_%04X&PID_%04X";

		//
		// Build Hardware ID depending on device type
		// 
		switch (DeviceType)
		{
		case DS_DEVICE_TYPE_SIXAXIS:
			status = RtlUnicodeStringPrintf(
				&hardwareId,
				hardwareIdFormat,
				BthPS3BusEnumeratorName,
				&guidString,
				BTHPS3_SIXAXIS_VID,
				BTHPS3_SIXAXIS_PID
			);
			break;
		case DS_DEVICE_TYPE_NAVIGATION:
			status = RtlUnicodeStringPrintf(
				&hardwareId,
				hardwareIdFormat,
				BthPS3BusEnumeratorName,
				&guidString,
				BTHPS3_NAVIGATION_VID,
				BTHPS3_NAVIGATION_PID
			);
			break;
		case DS_DEVICE_TYPE_MOTION:
			status = RtlUnicodeStringPrintf(
				&hardwareId,
				hardwareIdFormat,
				BthPS3BusEnumeratorName,
				&guidString,
				BTHPS3_MOTION_VID,
				BTHPS3_MOTION_PID
			);
			break;
		case DS_DEVICE_TYPE_WIRELESS:
			status = RtlUnicodeStringPrintf(
				&hardwareId,
				hardwareIdFormat,
				BthPS3BusEnumeratorName,
				&guidString,
				BTHPS3_WIRELESS_VID,
				BTHPS3_WIRELESS_PID
			);
			break;
		case DS_DEVICE_TYPE_UNKNOWN:
		default:  // NOLINT(clang-diagnostic-covered-switch-default)
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"RtlUnicodeStringPrintf failed for hardwareId with status %!STATUS!",
				status
			);
			break;
		}

		//
		// Set Hardware ID previously built
		// 
		record.HardwareIds[0] = hardwareId.Buffer;
		record.HardwareIdsCount = 1;

		//
		// Expose as RAW device if told
		// 
		if (rawPdo)
		{
			record.RawDevice = TRUE;

			switch (DeviceType)
			{
			case DS_DEVICE_TYPE_SIXAXIS:
				record.RawDeviceClassGuid = &GUID_DEVCLASS_BTHPS3_SIXAXIS;
				break;
			case DS_DEVICE_TYPE_NAVIGATION:
				record.RawDeviceClassGuid = &GUID_DEVCLASS_BTHPS3_NAVIGATION;
				break;
			case DS_DEVICE_TYPE_MOTION:
				record.RawDeviceClassGuid = &GUID_DEVCLASS_BTHPS3_MOTION;
				break;
			case DS_DEVICE_TYPE_WIRELESS:
				record.RawDeviceClassGuid = &GUID_DEVCLASS_BTHPS3_WIRELESS;
				break;
			case DS_DEVICE_TYPE_UNKNOWN:
			default:  // NOLINT(clang-diagnostic-covered-switch-default)
				status = STATUS_INVALID_PARAMETER;
				break;
			}
		}

		//
		// Create PDO, DMF modules and allocate PDO context
		// 
		if (!NT_SUCCESS(status = DMF_Pdo_DevicePlugEx(
			Context->Header.PdoModule,
			&record,
			&device
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"DMF_Pdo_DevicePlugEx failed with status %!STATUS!",
				status
			);
			break;
		}

		pdoPlugged = TRUE;

		{
			const PBTHPS3_PDO_CONTEXT pPdoCtxEarly = GetPdoContext(device);

			//
			// Initialize both events right after the plug succeeds so any
			// rollback below can safely wait on them (KeWaitForSingleObject
			// requires an initialized KEVENT).
			// 
			KeInitializeEvent(&pPdoCtxEarly->HidControlChannel.DisconnectEvent,
				NotificationEvent,
				TRUE
			);
			KeInitializeEvent(&pPdoCtxEarly->HidInterruptChannel.DisconnectEvent,
				NotificationEvent,
				TRUE
			);
		}

		//
		// Insert PDO in connection collection
		// 
		if (!NT_SUCCESS(status = WdfCollectionAdd(
			Context->Header.Clients,
			device
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfCollectionAdd for PDO device object failed with status %!STATUS!",
				status
			);
			break;
		}

		//
		// Persist slot information to avoid duplicates
		// 
		if (!NT_SUCCESS(status = BthPS3_PDO_AssignSlot(
			&Context->Header,
			RemoteAddress,
			record.SerialNumber
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"BthPS3_PDO_AssignSlot failed with status %!STATUS!",
				status
			);
			break;
		}

			const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(device);

			pPdoCtx->RemoteAddress = RemoteAddress;
			pPdoCtx->DevCtxHdr = &Context->Header;
			pPdoCtx->DeviceType = DeviceType;
			pPdoCtx->SerialNumber = record.SerialNumber;
			pPdoCtx->TeardownWorkItem = NULL;
			pPdoCtx->Lifecycle = BthPS3PdoLifecycleDraining;

			WDF_WORKITEM_CONFIG workItemConfig;
			WDF_WORKITEM_CONFIG_INIT(&workItemConfig, BthPS3_PDO_EvtTeardownWorkItem);

			WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
			attributes.ParentObject = device;

			if (!NT_SUCCESS(status = WdfWorkItemCreate(
				&workItemConfig,
				&attributes,
				&pPdoCtx->TeardownWorkItem
			)))
			{
				TraceError(
					TRACE_BUSLOGIC,
					"WdfWorkItemCreate for PDO teardown failed with status %!STATUS!",
					status
				);
				break;
			}

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = device;

		PUCHAR pBuffer = NULL;
		const size_t hwIdByteCount = (wcslen(hardwareId.Buffer) * sizeof(WCHAR)) + sizeof(L'\0');

		//
		// Save Hardware ID for later unplug
		// 
		if (!NT_SUCCESS(status = WdfMemoryCreate(
			&attributes,
			NonPagedPoolNx,
			POOLTAG_BTHPS3,
			hwIdByteCount,
			&pPdoCtx->HardwareId,
			(PVOID*)&pBuffer
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfMemoryCreate failed with status %!STATUS!",
				status
			);
			break;
		}

		RtlCopyMemory(pBuffer, hardwareId.Buffer, hwIdByteCount);

		//
		// Initialize HidControlChannel properties
		// 

		if (!NT_SUCCESS(status = WdfRequestCreate(
			&attributes,
			pPdoCtx->DevCtxHdr->IoTarget,
			&pPdoCtx->HidControlChannel.ConnectDisconnectRequest
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestCreate for HidControlChannel failed with status %!STATUS!",
				status
			);
			break;
		}

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = device;

		if (!NT_SUCCESS(status = WdfSpinLockCreate(
			&attributes,
			&pPdoCtx->HidControlChannel.ConnectionStateLock
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfSpinLockCreate for HidControlChannel failed with status %!STATUS!",
				status
			);
			break;
		}

			pPdoCtx->HidControlChannel.ConnectionState = ConnectionStateInitialized;
			pPdoCtx->HidControlChannel.PdoContext = pPdoCtx;

		//
		// Initialize HidInterruptChannel properties
		// 

		if (!NT_SUCCESS(status = WdfRequestCreate(
			&attributes,
			pPdoCtx->DevCtxHdr->IoTarget,
			&pPdoCtx->HidInterruptChannel.ConnectDisconnectRequest
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestCreate for HidInterruptChannel failed with status %!STATUS!",
				status
			);
			break;
		}

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = device;

		if (!NT_SUCCESS(status = WdfSpinLockCreate(
			&attributes,
			&pPdoCtx->HidInterruptChannel.ConnectionStateLock
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfSpinLockCreate for HidInterruptChannel failed with status %!STATUS!",
				status
			);
			break;
		}

			pPdoCtx->HidInterruptChannel.ConnectionState = ConnectionStateInitialized;
			pPdoCtx->HidInterruptChannel.PdoContext = pPdoCtx;

			//
			// Allow callbacks and BRB completions to acquire the PDO
			//
			DMF_Rundown_Start(pPdoCtx->DmfModuleRundown);

			//
			// Acquire a rundown reference on behalf of the caller. Ownership
			// transfers with *PdoContext; the caller releases it via
			// BthPS3_PDO_RundownRelease, same contract as
			// BthPS3_PDO_RetrieveByBthAddr.
			// 
			if (!NT_SUCCESS(status = BthPS3_PDO_RundownAcquire(pPdoCtx)))
			{
				TraceError(
					TRACE_BUSLOGIC,
					"BthPS3_PDO_RundownAcquire failed with status %!STATUS!",
					status
				);
				break;
			}

			//
			// Flip to Active before exposing the IOCTL interface so a
			// disconnect request arriving immediately after can transition
			// this PDO into Draining.
			// 
			InterlockedExchange(&pPdoCtx->Lifecycle, BthPS3PdoLifecycleActive);

			//
			// We're ready, expose interface
			// 
			DMF_IoctlHandler_IoctlStateSet(pPdoCtx->DmfModuleIoctlHandler, TRUE);

			*PdoContext = pPdoCtx;

	} while (FALSE);

	if (hKey)
	{
		WdfRegistryClose(hKey);
	}

	if (!NT_SUCCESS(status) && pdoPlugged)
	{
		*PdoContext = NULL;

		WdfWaitLockAcquire(Context->Header.ClientsLock, NULL);

		const ULONG itemCount = WdfCollectionGetCount(Context->Header.Clients);

		for (ULONG index = 0; index < itemCount; index++)
		{
			if (WdfCollectionGetItem(Context->Header.Clients, index) == device)
			{
				WdfCollectionRemoveItem(Context->Header.Clients, index);
				break;
			}
		}

		WdfWaitLockRelease(Context->Header.ClientsLock);

		(void)DMF_Pdo_DeviceUnplug(Context->Header.PdoModule, device);
		BthPS3_PDO_ReleaseSlot(&Context->Header, RemoteAddress, record.SerialNumber);
	}

	if (NT_SUCCESS(status))
	{
		EventWriteChildDeviceCreationSuccessful(
			NULL,
			RemoteAddress,
			RemoteName,
			record.SerialNumber,
			status
		);
	}
	else
	{
		EventWriteChildDeviceCreationFailed(
			NULL,
			RemoteAddress,
			RemoteName,
			record.SerialNumber,
			status
		);
	}

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}

//
// Retrieves an existing Active PDO for BTH_ADDR. Success returns a rundown
// reference that the caller must release with BthPS3_PDO_RundownRelease.
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
_Success_(return == STATUS_SUCCESS)
NTSTATUS
BthPS3_PDO_RetrieveByBthAddr(
	_In_ PBTHPS3_SERVER_CONTEXT Context,
	_In_ BTH_ADDR RemoteAddress,
	_Outptr_result_maybenull_ PBTHPS3_PDO_CONTEXT* PdoContext
)
{
	NTSTATUS status = STATUS_NOT_FOUND;

	FuncEntryArguments(
		TRACE_BUSLOGIC,
		"RemoteAddress=%012llX",
		RemoteAddress
	);

    *PdoContext = NULL;

	WdfWaitLockAcquire(Context->Header.ClientsLock, NULL);

	const ULONG itemCount = WdfCollectionGetCount(Context->Header.Clients);

	for (ULONG index = 0; index < itemCount; index++)
	{
		const WDFDEVICE currentPdo = WdfCollectionGetItem(Context->Header.Clients, index);
		const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(currentPdo);

		if (pPdoCtx->RemoteAddress == RemoteAddress &&
			pPdoCtx->Lifecycle == BthPS3PdoLifecycleActive)
		{
			if (!NT_SUCCESS(BthPS3_PDO_RundownAcquire(pPdoCtx)))
			{
				TraceVerbose(
					TRACE_BUSLOGIC,
					"Skipping PDO 0x%p: rundown acquire failed",
					pPdoCtx
				);
				continue;
			}

			TraceVerbose(
				TRACE_BUSLOGIC,
				"Found desired connection item in connection list"
			);

			status = STATUS_SUCCESS;
			*PdoContext = pPdoCtx;
			break;
		}
	}

	WdfWaitLockRelease(Context->Header.ClientsLock);

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
BthPS3_PDO_RundownAcquire(
	_In_ PBTHPS3_PDO_CONTEXT PdoContext
)
{
	if (PdoContext == NULL || PdoContext->DmfModuleRundown == NULL)
	{
		return STATUS_INVALID_DEVICE_STATE;
	}

	return DMF_Rundown_Reference(PdoContext->DmfModuleRundown);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
BthPS3_PDO_RundownRelease(
	_In_ PBTHPS3_PDO_CONTEXT PdoContext
)
{
	if (PdoContext == NULL || PdoContext->DmfModuleRundown == NULL)
	{
		return;
	}

	DMF_Rundown_Dereference(PdoContext->DmfModuleRundown);
}

static
VOID
BthPS3_PDO_WaitDisconnectEventDiagnostic(
	_In_ PKEVENT Event,
	_In_ PCSTR ChannelName
)
{
	LARGE_INTEGER timeout;
	NTSTATUS status;

	timeout.QuadPart = WDF_REL_TIMEOUT_IN_SEC(5);
	status = KeWaitForSingleObject(
		Event,
		Executive,
		KernelMode,
		FALSE,
		&timeout
	);

	if (status == STATUS_WAIT_0)
	{
		TraceVerbose(
			TRACE_BUSLOGIC,
			"%s channel event signalled",
			ChannelName
		);
	}
	else
	{
		TraceError(
			TRACE_BUSLOGIC,
			"%s channel wait completed with status %!STATUS! (timeout is not success)",
			ChannelName,
			status
		);
	}
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
BthPS3_PDO_UnplugNow(
	_In_ PBTHPS3_DEVICE_CONTEXT_HEADER Context,
	_In_ PBTHPS3_PDO_CONTEXT PdoContext
)
{
	const WDFDEVICE device = WdfObjectContextGetObject(PdoContext);
	const ULONG serial = PdoContext->SerialNumber;
	WCHAR hardwareId[BTHPS3_MAX_DEVICE_ID_LEN] = { 0 };

	if (PdoContext->HardwareId != NULL)
	{
		wcscpy_s(
			hardwareId,
			sizeof(hardwareId) / sizeof(WCHAR),
			(PWSTR)WdfMemoryGetBuffer(PdoContext->HardwareId, NULL)
		);
	}

	NTSTATUS status = DMF_Pdo_DeviceUnplug(Context->PdoModule, device);

	if (!NT_SUCCESS(status))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"DMF_Pdo_DeviceUnplug failed with status %!STATUS!",
			status
		);

		EventWriteChildDeviceDestructionFailed(
			NULL,
			serial,
			hardwareId,
			status
		);

		if (PdoContext->TeardownWorkItem != NULL)
		{
			WdfWorkItemEnqueue(PdoContext->TeardownWorkItem);
		}

		return;
	}

	WdfWaitLockAcquire(Context->ClientsLock, NULL);

	const ULONG itemCount = WdfCollectionGetCount(Context->Clients);

	for (ULONG index = 0; index < itemCount; index++)
	{
		if (WdfCollectionGetItem(Context->Clients, index) == device)
		{
			WdfCollectionRemoveItem(Context->Clients, index);
			break;
		}
	}

	InterlockedExchange(&PdoContext->Lifecycle, BthPS3PdoLifecycleUnplugged);

	WdfWaitLockRelease(Context->ClientsLock);

	EventWriteChildDeviceDestructionSuccessful(
		NULL,
		serial,
		hardwareId,
		status
	);
}

//
// Requests two-phase PDO teardown. Safe at DISPATCH_LEVEL: only CAS to
// Draining and enqueue the passive coordinator. Idempotent.
//
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
BthPS3_PDO_Destroy(
	_In_ PBTHPS3_DEVICE_CONTEXT_HEADER Context,
	_In_ PBTHPS3_PDO_CONTEXT PdoContext
)
{
	UNREFERENCED_PARAMETER(Context);

	FuncEntryArguments(
		TRACE_BUSLOGIC,
		"PdoContext=0x%p",
		PdoContext
	);

	const LONG previous = InterlockedCompareExchange(
		&PdoContext->Lifecycle,
		BthPS3PdoLifecycleDraining,
		BthPS3PdoLifecycleActive
	);

	if (previous != BthPS3PdoLifecycleActive)
	{
		TraceVerbose(
			TRACE_BUSLOGIC,
			"PDO 0x%p already leaving Active (lifecycle=%d), ignoring destroy",
			PdoContext,
			previous
		);
		FuncExitNoReturn(TRACE_BUSLOGIC);
		return;
	}

	TraceInformation(
		TRACE_BUSLOGIC,
		"PDO 0x%p transitioning Active -> Draining",
		PdoContext
	);

	if (PdoContext->TeardownWorkItem != NULL)
	{
		WdfWorkItemEnqueue(PdoContext->TeardownWorkItem);
	}
	else if (KeGetCurrentIrql() <= PASSIVE_LEVEL)
	{
		if (PdoContext->DmfModuleRundown != NULL)
		{
			DMF_Rundown_EndAndWait(PdoContext->DmfModuleRundown);
		}

		BthPS3_PDO_UnplugNow(PdoContext->DevCtxHdr, PdoContext);
	}
	else
	{
		TraceError(
			TRACE_BUSLOGIC,
			"PDO 0x%p has no teardown work item at DISPATCH_LEVEL",
			PdoContext
		);
	}

	FuncExitNoReturn(TRACE_BUSLOGIC);
}

VOID
BthPS3_PDO_EvtTeardownWorkItem(
	_In_ WDFWORKITEM WorkItem
)
{
	const WDFDEVICE device = (WDFDEVICE)WdfWorkItemGetParentObject(WorkItem);
	const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(device);

	FuncEntryArguments(TRACE_BUSLOGIC, "PdoContext=0x%p", pPdoCtx);

	NT_ASSERT(pPdoCtx->Lifecycle == BthPS3PdoLifecycleDraining);

	L2CAP_PS3_RemoteDisconnect(
		pPdoCtx->DevCtxHdr,
		pPdoCtx->RemoteAddress,
		&pPdoCtx->HidControlChannel
	);
	L2CAP_PS3_RemoteDisconnect(
		pPdoCtx->DevCtxHdr,
		pPdoCtx->RemoteAddress,
		&pPdoCtx->HidInterruptChannel
	);

	//
	// Let in-flight OPEN completions send CLOSE before rundown ends.
	// Timeout is logged; remaining BRBs are still waited for by rundown.
	//
	BthPS3_PDO_WaitDisconnectEventDiagnostic(
		&pPdoCtx->HidControlChannel.DisconnectEvent,
		"HID Control"
	);
	BthPS3_PDO_WaitDisconnectEventDiagnostic(
		&pPdoCtx->HidInterruptChannel.DisconnectEvent,
		"HID Interrupt"
	);

	DMF_Rundown_EndAndWait(pPdoCtx->DmfModuleRundown);

	BthPS3_PDO_UnplugNow(pPdoCtx->DevCtxHdr, pPdoCtx);

	FuncExitNoReturn(TRACE_BUSLOGIC);
}

//
// Diagnostic only. Rundown + the teardown coordinator are the lifetime barrier.
// STATUS_TIMEOUT must not be treated as a signaled event.
//
VOID
BthPS3_PDO_EvtContextCleanup(
	_In_
	WDFOBJECT Object
)
{
	FuncEntry(TRACE_BUSLOGIC);

	PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(Object);

	BthPS3_PDO_WaitDisconnectEventDiagnostic(
		&pPdoCtx->HidControlChannel.DisconnectEvent,
		"HID Control"
	);
	BthPS3_PDO_WaitDisconnectEventDiagnostic(
		&pPdoCtx->HidInterruptChannel.DisconnectEvent,
		"HID Interrupt"
	);

	TraceInformation(
		TRACE_BUSLOGIC,
		"Cleaning up context 0x%p of device object 0x%p (lifecycle=%d)",
		pPdoCtx,
		Object,
		pPdoCtx->Lifecycle
	);

	FuncExitNoReturn(TRACE_BUSLOGIC);
}
