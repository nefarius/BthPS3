/**********************************************************************************
 *                                                                                *
 * BthPS3PSM - Windows kernel-mode Bluetooth lower filter driver                  *
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


#include "driver.h"
#include "device.tmh"
#include <usbdi.h>
#include <BthPS3PSMETW.h>
#include <devpkey.h>

#ifdef BTHPS3PSM_WITH_CONTROL_DEVICE
extern WDFCOLLECTION FilterDeviceCollection;
extern WDFWAITLOCK FilterDeviceCollectionLock;
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3PSM_CreateDevice)
#pragma alloc_text (PAGE, BthPS3PSM_EvtDeviceContextCleanup)
#endif

#define BTHPS3PSM_DEVICE_PROPERTY_LENGTH        0xFF
#define BTHPS3PSM_USB_ENUMERATOR_NAME           L"USB"
#define BTHPS3PSM_BLUETOOTH_CLASS_NAME          L"Bluetooth"

//
// Documented compatible ID for non-USB Bluetooth radios bound to the
// Bluetooth Extensibility Transport DDI (bthxddi.h), see
// https://learn.microsoft.com/windows-hardware/drivers/bluetooth/bluetooth-host-radio-support
// 
#define BTHPS3PSM_BTHX_COMPATIBLE_ID             L"MS_BTHX_BTHMINI"

//
// Service name of Microsoft's inbox Bluetooth Extensibility Transport
// function driver; used as a fallback signal if the compatible ID above
// is not (or no longer) reported by a given transport stack.
// 
#define BTHPS3PSM_BTHMINI_SERVICE_NAME           L"BthMini"


//
// Called upon device creation
// 
_Use_decl_annotations_
NTSTATUS
BthPS3PSM_CreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDFDEVICE device;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES stringAttributes;
    BTHPS3PSM_TRANSPORT_TYPE transportType = BthPS3PsmTransportUnsupported;
    WDFMEMORY instanceId = NULL;

    DECLARE_CONST_UNICODE_STRING(patchPSMRegValue, G_PatchPSMRegValue);
    DECLARE_CONST_UNICODE_STRING(linkNameRegValue, G_SymbolicLinkName);


    FuncEntry(TRACE_DEVICE);

    PAGED_CODE();

    if (!NT_SUCCESS(BthPS3PSM_QueryTransportType(DeviceInit, &transportType)))
    {
        transportType = BthPS3PsmTransportUnsupported;
    }

    switch (transportType)
    {
    case BthPS3PsmTransportUsb:
        TraceVerbose(
            TRACE_DEVICE,
            "Device is a USB Bluetooth host radio"
        );
        EventWriteTransportTypeDetected(NULL, (ULONG)transportType);
        break;
    case BthPS3PsmTransportBthx:
        TraceVerbose(
            TRACE_DEVICE,
            "Device is a BTHX (Bluetooth Extensibility Transport) Bluetooth host radio"
        );
        EventWriteTransportTypeDetected(NULL, (ULONG)transportType);
        break;
    default:
        TraceEvents(TRACE_LEVEL_WARNING,
                    TRACE_DEVICE,
                    "Unsupported device type, aborting initialization"
        );
        EventWriteUnsupportedTransportType(NULL);
        break;
    }

    //
    // Don't create a device object and return
    // 
    if (transportType == BthPS3PsmTransportUnsupported)
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
        deviceContext->TransportType = transportType;

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
         * or, for a BTHX/BthMini-bound radio:
         * "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\IBTPCIBUS\HCIH4\XXXXXXXXXXXXX\Device Parameters"
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
            //
            // Do not log this case to ETW as it is normal on first launch
            // 
            if (status != STATUS_OBJECT_NAME_NOT_FOUND)
            {
                TraceError(
                    TRACE_DEVICE,
                    "WdfRegistryQueryULong failed with status %!STATUS!",
                    status
                );
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
        // Grab symbolic link so device can be associated with radio in user-mode.
        // 
        // This value is written by bthport.sys for its device interface and is
        // not guaranteed to exist for every transport stack (in particular some
        // BTHX/BthMini-based radios); treat its absence as non-fatal so the
        // filter still loads and patches PSMs, it just can't be positively
        // associated with a specific radio symbolic link by user-mode callers.
        // 
        if (!NT_SUCCESS(status = WdfRegistryQueryString(
            deviceContext->RegKeyDeviceNode,
            &linkNameRegValue,
            deviceContext->SymbolicLinkName
        )))
        {
            TraceEvents(TRACE_LEVEL_WARNING,
                TRACE_DEVICE,
                "WdfRegistryQueryString failed with status %!STATUS!, continuing without a symbolic link association",
                status
            );

            // reset to success; this is not a fatal condition
            status = STATUS_SUCCESS;
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
    }
    while (FALSE);

    if (instanceId && !NT_SUCCESS(status))
    {
        WdfObjectDelete(instanceId);
    }

    FuncExit(TRACE_DEVICE, "status=%!STATUS!", status);

    return status;
}

//
// Determines whether the device we're about to attach to is a supported
// Bluetooth host radio and, if so, which transport it runs on (USB or the
// Bluetooth Extensibility Transport, a.k.a. BTHX/BthMini).
// 
_Use_decl_annotations_
NTSTATUS
BthPS3PSM_QueryTransportType(
    _In_ PWDFDEVICE_INIT DeviceInit,
    _Inout_ BTHPS3PSM_TRANSPORT_TYPE* TransportType
)
{
    NTSTATUS status;
    WCHAR enumeratorName[MAX_DEVICE_ID_LEN];
    WCHAR className[MAX_DEVICE_ID_LEN];
    ULONG returnSize;
    UNICODE_STRING lhsEnumeratorName, lhsClassName;
    UNICODE_STRING rhsEnumeratorName, rhsClassName;

    *TransportType = BthPS3PsmTransportUnsupported;

    RtlInitUnicodeString(&rhsClassName, BTHPS3PSM_BLUETOOTH_CLASS_NAME);

    //
    // Regardless of transport, we only ever attach to Bluetooth-class devices
    // 
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

    RtlInitUnicodeString(&lhsClassName, className);

    if (RtlCompareUnicodeString(&lhsClassName, &rhsClassName, TRUE) != 0)
    {
        return STATUS_SUCCESS;
    }

    //
    // USB-attached radio (BTHUSB.SYS or a vendor equivalent)
    // 
    RtlInitUnicodeString(&rhsEnumeratorName, BTHPS3PSM_USB_ENUMERATOR_NAME);

    if (NT_SUCCESS(WdfFdoInitQueryProperty(
        DeviceInit,
        DevicePropertyEnumeratorName,
        sizeof(enumeratorName),
        enumeratorName,
        &returnSize
    )))
    {
        RtlInitUnicodeString(&lhsEnumeratorName, enumeratorName);

        if (RtlCompareUnicodeString(&lhsEnumeratorName, &rhsEnumeratorName, TRUE) == 0)
        {
            *TransportType = BthPS3PsmTransportUsb;
            return STATUS_SUCCESS;
        }
    }

    //
    // Not USB; check for a BTHX (Bluetooth Extensibility Transport) radio,
    // e.g. a PCIe or UART-attached controller bound to Microsoft's inbox
    // BthMini.sys transport function driver
    // 
    if (BthPS3PSM_IsBthxTransportDevice(DeviceInit))
    {
        *TransportType = BthPS3PsmTransportBthx;
    }

    return STATUS_SUCCESS;
}

//
// Checks the documented MS_BTHX_BTHMINI compatible ID and, as a fallback,
// the bound service name to determine whether the device we're about to
// filter is a BTHX (Bluetooth Extensibility Transport) radio.
// 
_Use_decl_annotations_
BOOLEAN
BthPS3PSM_IsBthxTransportDevice(
    _In_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    DEVPROPTYPE type;
    WDF_DEVICE_PROPERTY_DATA property;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFMEMORY memory;
    PWCHAR buffer;
    size_t bufferSize;
    BOOLEAN found = FALSE;
    UNICODE_STRING rhsCompatibleId;

    RtlInitUnicodeString(&rhsCompatibleId, BTHPS3PSM_BTHX_COMPATIBLE_ID);

    //
    // Primary signal: DEVPKEY_Device_CompatibleIds is a REG_MULTI_SZ-style
    // string list; per Microsoft's Bluetooth host radio support
    // documentation, non-USB radios expose MS_BTHX_BTHMINI in this list
    // 
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    WDF_DEVICE_PROPERTY_DATA_INIT(&property, &DEVPKEY_Device_CompatibleIds);

    memory = NULL;

    if (NT_SUCCESS(status = WdfFdoInitAllocAndQueryPropertyEx(
        DeviceInit,
        &property,
        NonPagedPoolNx,
        &attributes,
        &memory,
        &type
    )))
    {
        if (type == DEVPROP_TYPE_STRING_LIST)
        {
            buffer = (PWCHAR)WdfMemoryGetBuffer(memory, &bufferSize);

            for (PWCHAR cursor = buffer;
                 cursor != NULL
                     && ((ULONG_PTR)cursor - (ULONG_PTR)buffer) < bufferSize
                     && *cursor != UNICODE_NULL;
                 cursor += (wcslen(cursor) + 1))
            {
                UNICODE_STRING candidate;

                RtlInitUnicodeString(&candidate, cursor);

                if (RtlCompareUnicodeString(&candidate, &rhsCompatibleId, TRUE) == 0)
                {
                    found = TRUE;
                    break;
                }
            }
        }

        WdfObjectDelete(memory);
    }

    if (found)
    {
        return TRUE;
    }

    //
    // Fallback signal: match the well-known service name of Microsoft's
    // inbox BTHX transport function driver in case a given stack does not
    // (or no longer) reports the compatible ID above
    // 
    UNICODE_STRING rhsServiceName;
    RtlInitUnicodeString(&rhsServiceName, BTHPS3PSM_BTHMINI_SERVICE_NAME);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    WDF_DEVICE_PROPERTY_DATA_INIT(&property, &DEVPKEY_Device_Service);

    memory = NULL;

    if (NT_SUCCESS(status = WdfFdoInitAllocAndQueryPropertyEx(
        DeviceInit,
        &property,
        NonPagedPoolNx,
        &attributes,
        &memory,
        &type
    )))
    {
        if (type == DEVPROP_TYPE_STRING)
        {
            UNICODE_STRING serviceName;

            buffer = (PWCHAR)WdfMemoryGetBuffer(memory, &bufferSize);
            RtlInitUnicodeString(&serviceName, buffer);

            if (RtlCompareUnicodeString(&serviceName, &rhsServiceName, TRUE) == 0)
            {
                found = TRUE;
            }
        }

        WdfObjectDelete(memory);
    }

    return found;
}

_Use_decl_annotations_
NTSTATUS
BthPS3PSM_GetPropertyInstanceId(
    _In_ PWDFDEVICE_INIT DeviceInit,
    _Inout_ WDFMEMORY* Memory
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
        WDFDEVICE devIter = WdfCollectionGetItem(FilterDeviceCollection, i);

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
