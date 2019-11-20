/*
* BthPS3PSM - Windows kernel-mode BTHUSB lower filter driver
*
* MIT License
*
* Copyright (C) 2018-2019  Nefarius Software Solutions e.U. and Contributors
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/


#include "Driver.h"
#include "Sideband.tmh"
#include <wdmsec.h>


#ifdef BTHPS3PSM_WITH_CONTROL_DEVICE

WDFCOLLECTION   FilterDeviceCollection;
WDFWAITLOCK     FilterDeviceCollectionLock;
WDFDEVICE       ControlDevice = NULL;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3PSM_CreateControlDevice)
#pragma alloc_text (PAGE, BthPS3PSM_DeleteControlDevice)
#endif

//
// Creates the control device for sideband communication.
// 
_Use_decl_annotations_
NTSTATUS
BthPS3PSM_CreateControlDevice(
    WDFDEVICE Device
)
{
    NTSTATUS                status = STATUS_SUCCESS;
    BOOLEAN                 bCreate = FALSE;
    PWDFDEVICE_INIT         pInit = NULL;
    WDFDEVICE               controlDevice = NULL;
    WDF_IO_QUEUE_CONFIG     ioQueueConfig;
    WDFQUEUE                queue;

    DECLARE_CONST_UNICODE_STRING(ntDeviceName, BTHPS3PSM_NTDEVICE_NAME_STRING);
    DECLARE_CONST_UNICODE_STRING(symbolicLinkName, BTHPS3PSM_SYMBOLIC_NAME_STRING);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Entry");


    PAGED_CODE();

    //
    // First find out whether any ControlDevice has been created. If the
    // collection has more than one device then we know somebody has already
    // created or in the process of creating the device.
    //
    WdfWaitLockAcquire(FilterDeviceCollectionLock, NULL);

    if (WdfCollectionGetCount(FilterDeviceCollection) == 1) {
        bCreate = TRUE;
    }

    WdfWaitLockRelease(FilterDeviceCollectionLock);

    if (!bCreate) {
        //
        // Control device is already created. So return success.
        //
        return STATUS_SUCCESS;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_SIDEBAND,
        "Creating Control Device");

    pInit = WdfControlDeviceInitAllocate(
        WdfDeviceGetDriver(Device),
        /* only system services and elevated users may access us */
        &SDDL_DEVOBJ_SYS_ALL_ADM_ALL
    );

    if (pInit == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfControlDeviceInitAllocate failed with %!STATUS!", status);
        goto Error;
    }

    //
    // Exclusive access isn't required
    // 
    WdfDeviceInitSetExclusive(pInit, FALSE);

    //
    // Assign name to expose
    // 
    status = WdfDeviceInitAssignName(pInit, &ntDeviceName);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfDeviceInitAssignName failed with %!STATUS!", status);
        goto Error;
    }

    status = WdfDeviceCreate(&pInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &controlDevice);
    if (!NT_SUCCESS(status)) {
        goto Error;
    }

    //
    // Create a symbolic link for the control object so that user-mode can open
    // the device.
    //

    status = WdfDeviceCreateSymbolicLink(controlDevice,
        &symbolicLinkName);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfDeviceCreateSymbolicLink failed with %!STATUS!", status);
        goto Error;
    }

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
        WdfIoQueueDispatchSequential);

    ioQueueConfig.EvtIoDeviceControl = BthPS3PSM_SidebandIoDeviceControl;

    //
    // Framework by default creates non-power managed queues for
    // filter drivers.
    //
    status = WdfIoQueueCreate(controlDevice,
        &ioQueueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue // pointer to default queue
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfIoQueueCreate failed with %!STATUS!",
            status
        );
        goto Error;
    }

    //
    // Control devices must notify WDF when they are done initializing.   I/O is
    // rejected until this call is made.
    //
    WdfControlFinishInitializing(controlDevice);

    ControlDevice = controlDevice;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Exit");

    return status;

Error:

    if (pInit != NULL) {
        WdfDeviceInitFree(pInit);
    }

    if (controlDevice != NULL) {
        //
        // Release the reference on the newly created object, since
        // we couldn't initialize it.
        //
        WdfObjectDelete(controlDevice);
        controlDevice = NULL;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Exit (%!STATUS!)", status);

    return status;
}

//
// Tear down control device
// 
_Use_decl_annotations_
VOID
BthPS3PSM_DeleteControlDevice(
    WDFDEVICE Device
)
{
    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_SIDEBAND,
        "Deleting Control Device"
    );

    if (ControlDevice) {
        WdfObjectDelete(ControlDevice);
        ControlDevice = NULL;
    }
}

#pragma warning(push)
#pragma warning(disable:28118) // this callback will run at IRQL=PASSIVE_LEVEL
_Use_decl_annotations_
VOID BthPS3PSM_SidebandIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
)
{
    NTSTATUS                            status = STATUS_UNSUCCESSFUL;
    size_t                              length = 0;
    WDFDEVICE                           device;
    PDEVICE_CONTEXT                     pDevCtx = NULL;
    PBTHPS3PSM_ENABLE_PSM_PATCHING      pEnable = NULL;
    PBTHPS3PSM_DISABLE_PSM_PATCHING     pDisable = NULL;
    PBTHPS3PSM_GET_PSM_PATCHING         pGet = NULL;
    UNICODE_STRING                      linkName;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);


    switch (IoControlCode)
    {
#pragma region IOCTL_BTHPS3PSM_ENABLE_PSM_PATCHING

    case IOCTL_BTHPS3PSM_ENABLE_PSM_PATCHING:

        status = WdfRequestRetrieveInputBuffer(
            Request,
            sizeof(BTHPS3PSM_ENABLE_PSM_PATCHING),
            (void*)&pEnable,
            &length
        );

        if (!NT_SUCCESS(status) || length != sizeof(BTHPS3PSM_ENABLE_PSM_PATCHING))
        {
            TraceEvents(
                TRACE_LEVEL_ERROR,
                TRACE_SIDEBAND,
                "WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
                status
            );

            break;
        }

        WdfWaitLockAcquire(FilterDeviceCollectionLock, NULL);

        device = WdfCollectionGetItem(FilterDeviceCollection, pEnable->DeviceIndex);

        if (device == NULL)
        {
            status = STATUS_NO_SUCH_DEVICE;
        }
        else
        {
            pDevCtx = DeviceGetContext(device);
            pDevCtx->IsPsmPatchingEnabled = TRUE;

            TraceEvents(
                TRACE_LEVEL_VERBOSE,
                TRACE_SIDEBAND,
                "PSM patch enabled for device %d",
                pEnable->DeviceIndex
            );
        }

        WdfWaitLockRelease(FilterDeviceCollectionLock);

        break;

#pragma endregion

#pragma region IOCTL_BTHPS3PSM_DISABLE_PSM_PATCHING

    case IOCTL_BTHPS3PSM_DISABLE_PSM_PATCHING:

        status = WdfRequestRetrieveInputBuffer(
            Request,
            sizeof(BTHPS3PSM_DISABLE_PSM_PATCHING),
            (void*)&pDisable,
            &length
        );

        if (!NT_SUCCESS(status) || length != sizeof(BTHPS3PSM_DISABLE_PSM_PATCHING))
        {
            TraceEvents(
                TRACE_LEVEL_ERROR,
                TRACE_SIDEBAND,
                "WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
                status
            );

            break;
        }

        WdfWaitLockAcquire(FilterDeviceCollectionLock, NULL);

        device = WdfCollectionGetItem(FilterDeviceCollection, pDisable->DeviceIndex);

        if (device == NULL)
        {
            status = STATUS_NO_SUCH_DEVICE;
        }
        else
        {
            pDevCtx = DeviceGetContext(device);
            pDevCtx->IsPsmPatchingEnabled = FALSE;

            TraceEvents(
                TRACE_LEVEL_VERBOSE,
                TRACE_SIDEBAND,
                "PSM patch disabled for device %d",
                pDisable->DeviceIndex
            );
        }

        WdfWaitLockRelease(FilterDeviceCollectionLock);

        break;

#pragma endregion

#pragma region IOCTL_BTHPS3PSM_GET_PSM_PATCHING

    case IOCTL_BTHPS3PSM_GET_PSM_PATCHING:

        status = WdfRequestRetrieveInputBuffer(
            Request,
            sizeof(BTHPS3PSM_GET_PSM_PATCHING),
            (void*)&pGet,
            &length
        );

        if (!NT_SUCCESS(status) || length != sizeof(BTHPS3PSM_GET_PSM_PATCHING))
        {
            TraceEvents(
                TRACE_LEVEL_ERROR,
                TRACE_SIDEBAND,
                "WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
                status
            );

            break;
        }

        WdfWaitLockAcquire(FilterDeviceCollectionLock, NULL);

        device = WdfCollectionGetItem(FilterDeviceCollection, pGet->DeviceIndex);

        if (device == NULL)
        {
            status = STATUS_NO_SUCH_DEVICE;
        }
        else
        {
            pDevCtx = DeviceGetContext(device);

            status = WdfRequestRetrieveOutputBuffer(
                Request,
                sizeof(BTHPS3PSM_GET_PSM_PATCHING),
                (void*)&pGet,
                &length
            );

            if (!NT_SUCCESS(status) || length != sizeof(BTHPS3PSM_GET_PSM_PATCHING))
            {
                TraceEvents(
                    TRACE_LEVEL_ERROR,
                    TRACE_SIDEBAND,
                    "WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
                    status
                );
            }
            else
            {
                pGet->IsEnabled = (pDevCtx->IsPsmPatchingEnabled > 0);

                WdfStringGetUnicodeString(pDevCtx->SymbolicLinkName, &linkName);

                TraceEvents(
                    TRACE_LEVEL_VERBOSE,
                    TRACE_SIDEBAND,
                    "!! SymbolicLinkName: %wZ (length: %d)",
                    &linkName,
                    linkName.Length
                );

                // Source isn't NULL-terminated, so take that into account
                if (linkName.Length <= (BTHPS3_MAX_DEVICE_ID_LEN - 1))
                {
                    RtlCopyMemory(pGet->SymbolicLinkName, linkName.Buffer, linkName.Length);
                    pGet->SymbolicLinkName[linkName.Length] = '\0'; // NULL-terminate
                }

                WdfRequestSetInformation(Request, sizeof(BTHPS3PSM_GET_PSM_PATCHING));
            }
        }

        WdfWaitLockRelease(FilterDeviceCollectionLock);

        break;

#pragma endregion

    default:
        TraceEvents(TRACE_LEVEL_WARNING,
            TRACE_SIDEBAND,
            "Unknown I/O Control Code submitted: %X",
            IoControlCode
        );
        break;
    }

    WdfRequestComplete(Request, status);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Exit");
}
#pragma warning(pop) // enable 28118 again

#endif
