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

#define BTHPS3PSM_POOL_TAG                      'MSP3'
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
    WDF_OBJECT_ATTRIBUTES           deviceAttributes;
    WDFDEVICE                       device;
    NTSTATUS                        status;
    WDF_PNPPOWER_EVENT_CALLBACKS    pnpPowerCallbacks;
    PDEVICE_CONTEXT                 deviceContext;
    WDFKEY                          key;
    WDF_OBJECT_ATTRIBUTES           stringAttribs;
    BOOLEAN                         isUsb = FALSE, isSystem = FALSE;
    WDF_DEVICE_STATE                deviceState;
	BOOLEAN							ret;

    DECLARE_CONST_UNICODE_STRING(patchPSMRegValue, G_PatchPSMRegValue);
    DECLARE_CONST_UNICODE_STRING(linkNameRegValue, G_SymbolicLinkName);


    PAGED_CODE();

    if (NT_SUCCESS(BthPS3PSM_IsVirtualRootDevice(DeviceInit, &ret)) && ret)
    {
    	TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_DEVICE,
            "Device is virtual root device"
        );
	    isSystem = TRUE;
    }
    else if (NT_SUCCESS(BthPS3PSM_IsBthUsbDevice(DeviceInit, &ret)) && ret)
    {
    	TraceEvents(TRACE_LEVEL_VERBOSE,
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
    if (!isUsb && !isSystem) {
        return STATUS_SUCCESS;
    }

    if (!isSystem)
	    WdfFdoInitSetFilter(DeviceInit);

    //
    // PNP/Power callbacks
    // 
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    if (!isSystem)
	    pnpPowerCallbacks.EvtDevicePrepareHardware = BthPS3PSM_EvtDevicePrepareHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    //
    // Device object attributes
    // 
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
    if (!isSystem)
	    deviceAttributes.EvtCleanupCallback = BthPS3PSM_EvtDeviceContextCleanup;

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status))
    {
    	deviceContext = DeviceGetContext(device);
    	
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
        status = WdfWaitLockAcquire(FilterDeviceCollectionLock, NULL);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_DEVICE,
                "WdfWaitLockAcquire failed with status %!STATUS!",
                status
            );
        }

        //
        // WdfCollectionAdd takes a reference on the item object and removes
        // it when you call WdfCollectionRemove.
        //
        status = WdfCollectionAdd(FilterDeviceCollection, device);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_DEVICE,
                "WdfCollectionAdd failed with status %!STATUS!",
                status
            );
        }
        WdfWaitLockRelease(FilterDeviceCollectionLock);

        if (!NT_SUCCESS(status)) {
            return status;
        }

#endif

#pragma endregion

        status = WdfDeviceOpenRegistryKey(
            device,
            PLUGPLAY_REGKEY_DEVICE,
            KEY_READ,
            WDF_NO_OBJECT_ATTRIBUTES,
            &key
        );

        if (NT_SUCCESS(status))
        {
            status = WdfRegistryQueryULong(
                key,
                &patchPSMRegValue,
                &deviceContext->IsPsmPatchingEnabled
            );

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "WdfRegistryQueryULong failed with status %!STATUS!",
                    status
                );
            }
            else
            {
                TraceEvents(TRACE_LEVEL_VERBOSE,
                    TRACE_DEVICE,
                    "BthPS3PSMPatchEnabled value retrieved"
                );
            }

            WDF_OBJECT_ATTRIBUTES_INIT(&stringAttribs);
            stringAttribs.ParentObject = device;

            status = WdfStringCreate(
                NULL,
                &stringAttribs,
                &deviceContext->SymbolicLinkName
            );

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "WdfStringCreate failed with status %!STATUS!",
                    status
                );
            }
            else
            {
                status = WdfRegistryQueryString(
                    key,
                    &linkNameRegValue,
                    deviceContext->SymbolicLinkName
                );

                if (!NT_SUCCESS(status))
                {
                    TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "WdfRegistryQueryString failed with status %!STATUS!",
                        status
                    );
                }
                else
                {
                    TraceEvents(TRACE_LEVEL_VERBOSE,
                        TRACE_DEVICE,
                        "SymbolicLinkName value retrieved"
                    );
                }
            }

            WdfRegistryClose(key);
        }
        else
        {
            TraceEvents(TRACE_LEVEL_ERROR,
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
        status = BthPS3PSM_CreateControlDevice(device);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
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

NTSTATUS BthPS3PSM_IsVirtualRootDevice(PWDFDEVICE_INIT DeviceInit, PBOOLEAN Result)
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
	
	status = WdfFdoInitQueryProperty(
		DeviceInit,
		DevicePropertyEnumeratorName,
		sizeof(enumeratorName),
		enumeratorName,
		&returnSize
	);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = WdfFdoInitQueryProperty(
		DeviceInit,
		DevicePropertyHardwareID,
		sizeof(hardwareID),
		hardwareID,
		&returnSize
	);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = WdfFdoInitQueryProperty(
		DeviceInit,
		DevicePropertyClassName,
		sizeof(className),
		className,
		&returnSize
	);
	if (!NT_SUCCESS(status))
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

NTSTATUS BthPS3PSM_IsBthUsbDevice(PWDFDEVICE_INIT DeviceInit, PBOOLEAN Result)
{
    NTSTATUS status;
	WCHAR enumeratorName[MAX_DEVICE_ID_LEN];
	WCHAR className[MAX_DEVICE_ID_LEN];
	ULONG returnSize;
	UNICODE_STRING lhsEnumeratorName, lhsClassName;
	UNICODE_STRING rhsEnumeratorName, rhsClassName;

	RtlInitUnicodeString(&rhsEnumeratorName, L"USB");
	RtlInitUnicodeString(&rhsClassName, L"Bluetooth");
	
	status = WdfFdoInitQueryProperty(
		DeviceInit,
		DevicePropertyEnumeratorName,
		sizeof(enumeratorName),
		enumeratorName,
		&returnSize
	);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = WdfFdoInitQueryProperty(
		DeviceInit,
		DevicePropertyClassName,
		sizeof(className),
		className,
		&returnSize
	);
	if (!NT_SUCCESS(status))
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
    NTSTATUS                        status = STATUS_SUCCESS;
    WDF_USB_DEVICE_CREATE_CONFIG    usbConfig;
    PDEVICE_CONTEXT                 deviceContext;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Entry");

    deviceContext = DeviceGetContext(Device);

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
    status = WdfUsbTargetDeviceCreateWithParameters(
        Device,
        &usbConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &deviceContext->UsbDevice
    );

    if (!NT_SUCCESS(status)) {

        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "WdfUsbTargetDeviceCreateWithParameters failed with status %!STATUS!",
            status);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Exit");

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

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Entry");

	PDEVICE_CONTEXT pDevCtx = DeviceGetContext(Device);
	
#ifdef BTHPS3PSM_WITH_CONTROL_DEVICE

    ULONG               count, i;
    WDFKEY              key;
    NTSTATUS            status;
    WDFDEVICE           devIter = NULL;

    DECLARE_CONST_UNICODE_STRING(patchPSMRegValue, G_PatchPSMRegValue);
    
    status = WdfWaitLockAcquire(FilterDeviceCollectionLock, NULL);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "WdfWaitLockAcquire failed with status %!STATUS!",
            status
        );
    }

    count = WdfCollectionGetCount(FilterDeviceCollection);

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
    for (i = 0; i < count; i++)
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

    status = WdfDeviceOpenRegistryKey(
        Device,
        PLUGPLAY_REGKEY_DEVICE,
        KEY_WRITE,
        WDF_NO_OBJECT_ATTRIBUTES,
        &key
    );

    if (NT_SUCCESS(status))
    {
        status = WdfRegistryAssignULong(
            key,
            &patchPSMRegValue,
            pDevCtx->IsPsmPatchingEnabled
        );

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_DEVICE,
                "WdfRegistryAssignULong failed with status %!STATUS!",
                status
            );
        }
        else
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_DEVICE,
                "Settings stored"
            );
        }

        WdfRegistryClose(key);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_DEVICE,
            "WdfDeviceOpenRegistryKey failed with status %!STATUS!",
            status
        );
    }

#pragma endregion

#else
    UNREFERENCED_PARAMETER(Device);
#endif

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Exit");
}
#pragma warning(pop) // enable 28118 again
