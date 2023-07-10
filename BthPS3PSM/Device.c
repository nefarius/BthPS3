/**********************************************************************************
 *                                                                                *
 * BthPS3PSM - Windows kernel-mode BTHUSB lower filter driver                     *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2023, Nefarius Software Solutions e.U.                      *
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
#include <BthPS3PSMETW.h>
#include <devpkey.h>

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
    WDF_OBJECT_ATTRIBUTES stringAttributes;
    BOOLEAN isUsb = FALSE;
    BOOLEAN ret = FALSE;
    WDFMEMORY instanceId = NULL;

    DECLARE_CONST_UNICODE_STRING(patchPSMRegValue, G_PatchPSMRegValue);
    DECLARE_CONST_UNICODE_STRING(linkNameRegValue, G_SymbolicLinkName);


    FuncEntry(TRACE_DEVICE);

    PAGED_CODE();

    if (NT_SUCCESS(BthPS3PSM_IsBthUsbDevice(DeviceInit, &ret)) && ret)
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
    if (!isUsb)
    {
        FuncExitNoReturn(TRACE_DEVICE);
        return STATUS_SUCCESS;
    }

    do
    {
        if (!NT_SUCCESS(status = BthPS3PSM_GetPropertyInstanceId(
            DeviceInit,
            &instanceId
        )))
        {
            TraceError(
                TRACE_DEVICE,
                "BthPS3PSM_GetPropertyInstanceId failed with status %!STATUS!",
                status
            );
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"BthPS3PSM_GetPropertyInstanceId", status);
            break;
        }

        WdfFdoInitSetFilter(DeviceInit);

        //
        // PNP/Power callbacks
        // 
        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
        pnpPowerCallbacks.EvtDevicePrepareHardware = BthPS3PSM_EvtDevicePrepareHardware;

        WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

        //
        // Device object attributes
        // 
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
        deviceAttributes.EvtCleanupCallback = BthPS3PSM_EvtDeviceContextCleanup;

        if (!NT_SUCCESS(status = WdfDeviceCreate(
            &DeviceInit,
            &deviceAttributes,
            &device
        )))
        {
            TraceError(
                TRACE_DEVICE,
                "WdfDeviceCreate failed with status %!STATUS!",
                status
            );
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfDeviceCreate", status);
            break;
        }

        PDEVICE_CONTEXT deviceContext = DeviceGetContext(device);

        deviceContext->InstanceId = instanceId;

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
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfWaitLockAcquire", status);
            break;
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
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfCollectionAdd", status);
        }
        WdfWaitLockRelease(FilterDeviceCollectionLock);

        if (!NT_SUCCESS(status))
        {
            break;
        }

#endif

#pragma endregion

        /*
         * Expands to e.g.:
         *
         * "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\USB\VID_XXXX&PID_XXXX\XXXXXXXXXXXXX\Device Parameters"
         */
        if (!NT_SUCCESS(status = WdfDeviceOpenRegistryKey(
            device,
            PLUGPLAY_REGKEY_DEVICE,
            KEY_READ,
            WDF_NO_OBJECT_ATTRIBUTES,
            &deviceContext->RegKeyDeviceNode
        )))
        {
            TraceError(
                TRACE_DEVICE,
                "WdfDeviceOpenRegistryKey failed with status %!STATUS!",
                status
            );
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfDeviceOpenRegistryKey", status);
            break;
        }

        // 
        // Query for BthPS3PSMPatchEnabled value
        // 
        if (!NT_SUCCESS(status = WdfRegistryQueryULong(
            deviceContext->RegKeyDeviceNode,
            &patchPSMRegValue,
            &deviceContext->IsPsmPatchingEnabled
        )))
        {
            TraceError(
                TRACE_DEVICE,
                "WdfRegistryQueryULong failed with status %!STATUS!",
                status
            );

            //
            // Do not log this case to ETW as it is normal on first launch
            // 
            if (status != STATUS_OBJECT_NAME_NOT_FOUND)
            {
                EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfRegistryQueryULong", status);
            }
        }
        else
        {
            TraceVerbose(
                TRACE_DEVICE,
                "BthPS3PSMPatchEnabled value retrieved"
            );

            const PWSTR instanceIdString = (const PWSTR)WdfMemoryGetBuffer(instanceId, NULL);

            EventWriteGetPatchStatusForDeviceInstance(
                NULL,
                deviceContext->IsPsmPatchingEnabled,
                instanceIdString
            );
        }

        WDF_OBJECT_ATTRIBUTES_INIT(&stringAttributes);
        stringAttributes.ParentObject = device;

        // 
        // Create string to hold symbolic link
        // 
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
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfStringCreate", status);
            break;
        }

        // 
        // Grab symbolic link so device can be associated with radio in user-mode
        // 
        if (!NT_SUCCESS(status = WdfRegistryQueryString(
            deviceContext->RegKeyDeviceNode,
            &linkNameRegValue,
            deviceContext->SymbolicLinkName
        )))
        {
            TraceError(
                TRACE_DEVICE,
                "WdfRegistryQueryString failed with status %!STATUS!",
                status
            );
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfRegistryQueryString", status);
            break;
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
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"BthPS3PSM_CreateControlDevice", status);
            break;
        }

#pragma endregion

#endif

        //
        // Initialize the I/O Package and any Queues
        //
        status = BthPS3PSM_QueueInitialize(device);

        } while (FALSE);

        if (instanceId && !NT_SUCCESS(status))
        {
            WdfObjectDelete(instanceId);
        }

        FuncExit(TRACE_DEVICE, "status=%!STATUS!", status);

        return status;
    }

_Success_(return == STATUS_SUCCESS)
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3PSM_IsBthUsbDevice(
    _In_ PWDFDEVICE_INIT DeviceInit,
    _Inout_opt_ PBOOLEAN Result
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

_Success_(return == STATUS_SUCCESS)
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3PSM_GetPropertyInstanceId(
    _In_ PWDFDEVICE_INIT DeviceInit,
    _Inout_ WDFMEMORY * Memory
)
{
    DEVPROPTYPE type;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    WDF_DEVICE_PROPERTY_DATA property;
    WDF_DEVICE_PROPERTY_DATA_INIT(&property, &DEVPKEY_Device_InstanceId);

    //
    // Query DEVPKEY_Device_InstanceId
    // 
    return WdfFdoInitAllocAndQueryPropertyEx(DeviceInit,
        &property,
        NonPagedPoolNx,
        &attributes,
        Memory,
        &type
    );
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
            status
        );
        EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfUsbTargetDeviceCreateWithParameters", status);
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

    const PDEVICE_CONTEXT pDevCtx = DeviceGetContext(Device);

#ifdef BTHPS3PSM_WITH_CONTROL_DEVICE

    NTSTATUS status;
    WDFDEVICE devIter = NULL;

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
        EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfWaitLockAcquire", status);
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

    //
    // This object has no parent so we need to delete it manually
    // 
    WdfObjectDelete(pDevCtx->InstanceId);

#else
    UNREFERENCED_PARAMETER(Device);
#endif

    FuncExitNoReturn(TRACE_DEVICE);
}
#pragma warning(pop) // enable 28118 again
