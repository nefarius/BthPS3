/**********************************************************************************
 *                                                                                *
 * BthPS3PSM - Windows kernel-mode BTHUSB lower filter driver                     *
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


#include "driver.h"
#include "device.tmh"
#include <usb.h>
#include <usbdi.h>
#include <usbdlib.h>
#include <wdfusb.h>

#ifdef BTHPS3PSM_WITH_CONTROL_DEVICE
extern WDFCOLLECTION   FilterDeviceCollection;
extern WDFWAITLOCK     FilterDeviceCollectionLock;
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3PSM_CreateDevice)
#pragma alloc_text (PAGE, BthPS3PSM_EvtDevicePrepareHardware)
#pragma alloc_text (PAGE, BthPS3PSM_EvtDeviceContextCleanup)
#endif

#define BTHPS3PSM_DEVICE_PROPERTY_LENGTH        0xFF
#define BTHPS3PSM_USB_ENUMERATOR_NAME           L"USB"
#define BTHPS3PSM_SYSTEM_ENUMERATOR_NAME        L"ROOT"


//
// Called upon device creation
// 
NTSTATUS
BthPS3PSM_CreateDevice(
	_Inout_ PWDFDEVICE_INIT DeviceInit
)
{
	WDF_OBJECT_ATTRIBUTES deviceAttributes;
	WDFDEVICE device;
	NTSTATUS status;
	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
	WDFKEY key;
	WDF_OBJECT_ATTRIBUTES stringAttributes;
	BOOLEAN isUsb = FALSE, isSystem = FALSE;
	WDF_DEVICE_STATE deviceState;
	BOOLEAN ret;

	DECLARE_CONST_UNICODE_STRING(patchPSMRegValue, G_PatchPSMRegValue);
	DECLARE_CONST_UNICODE_STRING(linkNameRegValue, G_SymbolicLinkName);


	PAGED_CODE();

	// TODO: this is obsolete and can be removed
	if (NT_SUCCESS(BthPS3PSM_IsVirtualRootDevice(DeviceInit, &ret)) && ret)
	{
		TraceVerbose(
			TRACE_DEVICE,
			"Device is virtual root device"
		);
		isSystem = TRUE;
	}
	else if (NT_SUCCESS(BthPS3PSM_IsBthUsbDevice(DeviceInit, &ret)) && ret)
	{
		TraceVerbose(
			TRACE_DEVICE,
			"Device is USB Bluetooth device"
		);
		isUsb = TRUE;
	}
	else
	{
		TraceEvents(TRACE_LEVEL_WARNING,
			TRACE_DEVICE,
			"Unsupported device type, aborting initialization"
		);
	}

	//
	// Don't create a device object and return
	// 
	if (!isUsb && !isSystem)
	{
		return STATUS_SUCCESS;
	}

	if (!isSystem)
	{
		WdfFdoInitSetFilter(DeviceInit);
	}

	//
	// PNP/Power callbacks
	// 
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

	if (!isSystem)
	{
		pnpPowerCallbacks.EvtDevicePrepareHardware = BthPS3PSM_EvtDevicePrepareHardware;
	}

	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

	//
	// Device object attributes
	// 
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
	if (!isSystem)
	{
		deviceAttributes.EvtCleanupCallback = BthPS3PSM_EvtDeviceContextCleanup;
	}

	if (NT_SUCCESS(status = WdfDeviceCreate(
		&DeviceInit,
		&deviceAttributes,
		&device
	)))
	{
		PDEVICE_CONTEXT deviceContext = DeviceGetContext(device);

		if (isSystem)
		{
			//
			// Hide from UI
			// 
			WDF_DEVICE_STATE_INIT(&deviceState);
			deviceState.DontDisplayInUI = WdfTrue;
			WdfDeviceSetDeviceState(device, &deviceState);

			//
			// Done, no need for further initialization
			// 
			return status;
		}

#pragma region Add this device to global collection

#ifdef BTHPS3PSM_WITH_CONTROL_DEVICE

		//
		// Add this device to the FilterDevice collection.
		//
		if (!NT_SUCCESS(status = WdfWaitLockAcquire(
			FilterDeviceCollectionLock,
			NULL
		)))
		{
			TraceError(
				TRACE_DEVICE,
				"WdfWaitLockAcquire failed with status %!STATUS!",
				status
			);
		}

		//
		// WdfCollectionAdd takes a reference on the item object and removes
		// it when you call WdfCollectionRemove.
		//
		if (!NT_SUCCESS(status = WdfCollectionAdd(
			FilterDeviceCollection,
			device
		)))
		{
			TraceError(
				TRACE_DEVICE,
				"WdfCollectionAdd failed with status %!STATUS!",
				status
			);
		}
		WdfWaitLockRelease(FilterDeviceCollectionLock);

		if (!NT_SUCCESS(status))
		{
			return status;
		}

#endif

#pragma endregion

		if (NT_SUCCESS(status = WdfDeviceOpenRegistryKey(
			device,
			/*
			 * Expands to e.g.:
			 *
			 * "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\USB\VID_XXXX&PID_XXXX\a&c9c4e92&0&4\Device Parameters"
			 *
			 */
			PLUGPLAY_REGKEY_DEVICE,
			KEY_READ,
			WDF_NO_OBJECT_ATTRIBUTES,
			&key
		)))
		{
			if (!NT_SUCCESS(status = WdfRegistryQueryULong(
				key,
				&patchPSMRegValue,
				&deviceContext->IsPsmPatchingEnabled
			)))
			{
				TraceError(
					TRACE_DEVICE,
					"WdfRegistryQueryULong failed with status %!STATUS!",
					status
				);
			}
			else
			{
				TraceVerbose(
					TRACE_DEVICE,
					"BthPS3PSMPatchEnabled value retrieved"
				);
			}

			WDF_OBJECT_ATTRIBUTES_INIT(&stringAttributes);
			stringAttributes.ParentObject = device;

			if (!NT_SUCCESS(status = WdfStringCreate(
				NULL,
				&stringAttributes,
				&deviceContext->SymbolicLinkName
			)))
			{
				TraceError(
					TRACE_DEVICE,
					"WdfStringCreate failed with status %!STATUS!",
					status
				);
			}
			else
			{
				if (!NT_SUCCESS(status = WdfRegistryQueryString(
					key,
					&linkNameRegValue,
					deviceContext->SymbolicLinkName
				)))
				{
					TraceError(
						TRACE_DEVICE,
						"WdfRegistryQueryString failed with status %!STATUS!",
						status
					);
				}
				else
				{
					TraceVerbose(
						TRACE_DEVICE,
						"SymbolicLinkName value retrieved"
					);
				}
			}

			WdfRegistryClose(key);
		}
		else
		{
			TraceError(
				TRACE_DEVICE,
				"WdfDeviceOpenRegistryKey failed with status %!STATUS!",
				status
			);
		}


#ifndef BTHPS3PSM_WITH_CONTROL_DEVICE
		deviceContext->IsPsmPatchingEnabled = TRUE;
#else

#pragma region Create control device

		//
		// Create a control device
		//
		if (!NT_SUCCESS(status = BthPS3PSM_CreateControlDevice(
			device
		)))
		{
			TraceError(
				TRACE_DEVICE,
				"BthPS3PSM_CreateControlDevice failed with status %!STATUS!",
				status
			);

			return status;
		}

#pragma endregion

#endif

		//
		// Initialize the I/O Package and any Queues
		//
		status = BthPS3PSM_QueueInitialize(device);
	}

	return status;
}

NTSTATUS
BthPS3PSM_IsVirtualRootDevice(
	PWDFDEVICE_INIT DeviceInit,
	PBOOLEAN Result
)
{
	NTSTATUS status;
	WCHAR enumeratorName[MAX_DEVICE_ID_LEN];
	WCHAR hardwareID[MAX_DEVICE_ID_LEN];
	WCHAR className[MAX_DEVICE_ID_LEN];
	ULONG returnSize;
	UNICODE_STRING lhsEnumeratorName, lhsHardwareID, lhsClassName;
	UNICODE_STRING rhsEnumeratorName, rhsHardwareID, rhsClassName;

	RtlInitUnicodeString(&rhsEnumeratorName, L"ROOT");
	RtlInitUnicodeString(&rhsHardwareID, BTHPS3PSM_FILTER_HARDWARE_ID);
	RtlInitUnicodeString(&rhsClassName, L"System");

	if (!NT_SUCCESS(status = WdfFdoInitQueryProperty(
		DeviceInit,
		DevicePropertyEnumeratorName,
		sizeof(enumeratorName),
		enumeratorName,
		&returnSize
	)))
	{
		return status;
	}

	if (!NT_SUCCESS(status = WdfFdoInitQueryProperty(
		DeviceInit,
		DevicePropertyHardwareID,
		sizeof(hardwareID),
		hardwareID,
		&returnSize
	)))
	{
		return status;
	}

	if (!NT_SUCCESS(status = WdfFdoInitQueryProperty(
		DeviceInit,
		DevicePropertyClassName,
		sizeof(className),
		className,
		&returnSize
	)))
	{
		return status;
	}

	RtlInitUnicodeString(&lhsEnumeratorName, enumeratorName);
	RtlInitUnicodeString(&lhsHardwareID, hardwareID);
	RtlInitUnicodeString(&lhsClassName, className);

	if (Result)
		*Result = ((RtlCompareUnicodeString(&lhsEnumeratorName, &rhsEnumeratorName, TRUE) == 0)
			&& (RtlCompareUnicodeString(&lhsHardwareID, &rhsHardwareID, TRUE) == 0)
			&& (RtlCompareUnicodeString(&lhsClassName, &rhsClassName, TRUE) == 0));

	return status;
}

NTSTATUS
BthPS3PSM_IsBthUsbDevice(
	PWDFDEVICE_INIT DeviceInit,
	PBOOLEAN Result
)
{
	NTSTATUS status;
	WCHAR enumeratorName[MAX_DEVICE_ID_LEN];
	WCHAR className[MAX_DEVICE_ID_LEN];
	ULONG returnSize;
	UNICODE_STRING lhsEnumeratorName, lhsClassName;
	UNICODE_STRING rhsEnumeratorName, rhsClassName;

	RtlInitUnicodeString(&rhsEnumeratorName, L"USB");
	RtlInitUnicodeString(&rhsClassName, L"Bluetooth");

	if (!NT_SUCCESS(status = WdfFdoInitQueryProperty(
		DeviceInit,
		DevicePropertyEnumeratorName,
		sizeof(enumeratorName),
		enumeratorName,
		&returnSize
	)))
	{
		return status;
	}

	if (!NT_SUCCESS(status = WdfFdoInitQueryProperty(
		DeviceInit,
		DevicePropertyClassName,
		sizeof(className),
		className,
		&returnSize
	)))
	{
		return status;
	}

	RtlInitUnicodeString(&lhsEnumeratorName, enumeratorName);
	RtlInitUnicodeString(&lhsClassName, className);

	if (Result)
		*Result = ((RtlCompareUnicodeString(&lhsEnumeratorName, &rhsEnumeratorName, TRUE) == 0)
			&& (RtlCompareUnicodeString(&lhsClassName, &rhsClassName, TRUE) == 0));

	return status;
}

//
// Called upon powering up
// 
NTSTATUS
BthPS3PSM_EvtDevicePrepareHardware(
	WDFDEVICE Device,
	WDFCMRESLIST ResourcesRaw,
	WDFCMRESLIST ResourcesTranslated
)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_USB_DEVICE_CREATE_CONFIG usbConfig;

	UNREFERENCED_PARAMETER(ResourcesRaw);
	UNREFERENCED_PARAMETER(ResourcesTranslated);

	PAGED_CODE();

	FuncEntry(TRACE_DEVICE);

	const PDEVICE_CONTEXT deviceContext = DeviceGetContext(Device);

	//
	// Initialize the USB config context.
	//
	WDF_USB_DEVICE_CREATE_CONFIG_INIT(
		&usbConfig,
		USBD_CLIENT_CONTRACT_VERSION_602
	);

	//
	// Allocate framework USB device object
	// 
	// Since we're a filter we _must not_ blindly
	// call WdfUsb* functions but "abuse" the URBs
	// coming from the upper function driver.
	// 
	if (!NT_SUCCESS(status = WdfUsbTargetDeviceCreateWithParameters(
		Device,
		&usbConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&deviceContext->UsbDevice
	)))
	{
		TraceError(
			TRACE_QUEUE,
			"WdfUsbTargetDeviceCreateWithParameters failed with status %!STATUS!",
			status);
	}

	FuncExit(TRACE_DEVICE, "status=%!STATUS!", status);

	return status;
}

//
// Called upon device context clean-up
// 
#pragma warning(push)
#pragma warning(disable:28118) // this callback will run at IRQL=PASSIVE_LEVEL
_Use_decl_annotations_
VOID
BthPS3PSM_EvtDeviceContextCleanup(
	WDFOBJECT Device
)
{
	PAGED_CODE();

	FuncEntry(TRACE_DEVICE);

	PDEVICE_CONTEXT pDevCtx = DeviceGetContext(Device);

#ifdef BTHPS3PSM_WITH_CONTROL_DEVICE

	WDFKEY key;
	NTSTATUS status;
	WDFDEVICE devIter = NULL;

	DECLARE_CONST_UNICODE_STRING(patchPSMRegValue, G_PatchPSMRegValue);

	if (!NT_SUCCESS(status = WdfWaitLockAcquire(
		FilterDeviceCollectionLock,
		NULL
	)))
	{
		TraceError(
			TRACE_QUEUE,
			"WdfWaitLockAcquire failed with status %!STATUS!",
			status
		);
	}

	const ULONG count = WdfCollectionGetCount(FilterDeviceCollection);

	if (count == 1)
	{
		//
		// We are the last instance. So let us delete the control-device
		// so that driver can unload when the FilterDevice is deleted.
		// We absolutely have to do the deletion of control device with
		// the collection lock acquired because we implicitly use this
		// lock to protect ControlDevice global variable. We need to make
		// sure another thread doesn't attempt to create while we are
		// deleting the device.
		//
		BthPS3PSM_DeleteControlDevice((WDFDEVICE)Device);
	}

	//
	// Collection might be empty due to device creation failure
	// Loop though and compare items before removal attempt
	// 
	for (ULONG i = 0; i < count; i++)
	{
		devIter = WdfCollectionGetItem(FilterDeviceCollection, i);

		if (devIter == Device)
		{
			WdfCollectionRemoveItem(FilterDeviceCollection, i);
			break;
		}
	}

	WdfWaitLockRelease(FilterDeviceCollectionLock);

#pragma region Store settings in Registry Hardware Key

	pDevCtx = DeviceGetContext(Device);

	if (NT_SUCCESS(status = WdfDeviceOpenRegistryKey(
		Device,
		/*
		 * Expands to e.g.:
		 *
		 * "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\USB\VID_XXXX&PID_XXXX\a&c9c4e92&0&4\Device Parameters"
		 *
		 */
		PLUGPLAY_REGKEY_DEVICE,
		KEY_WRITE,
		WDF_NO_OBJECT_ATTRIBUTES,
		&key
	)))
	{
		if (!NT_SUCCESS(status = WdfRegistryAssignULong(
			key,
			&patchPSMRegValue,
			pDevCtx->IsPsmPatchingEnabled
		)))
		{
			TraceError(
				TRACE_DEVICE,
				"WdfRegistryAssignULong failed with status %!STATUS!",
				status
			);
		}
		else
		{
			TraceVerbose(
				TRACE_DEVICE,
				"Settings stored"
			);
		}

		WdfRegistryClose(key);
	}
	else
	{
		TraceError(
			TRACE_DEVICE,
			"WdfDeviceOpenRegistryKey failed with status %!STATUS!",
			status
		);
	}

#pragma endregion

#else
	UNREFERENCED_PARAMETER(Device);
#endif

	FuncExitNoReturn(TRACE_DEVICE);
}
#pragma warning(pop) // enable 28118 again
