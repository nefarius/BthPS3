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

    FuncEntry(TRACE_SIDEBAND);


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

    TraceInformation(
        TRACE_SIDEBAND,
        "Creating Control Device"
    );

    do
    {
        pInit = WdfControlDeviceInitAllocate(
            WdfDeviceGetDriver(Device),
            /* only system services and elevated users may access us */
            &SDDL_DEVOBJ_SYS_ALL_ADM_ALL
        );

        if (pInit == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            TraceError(
                TRACE_SIDEBAND,
                "WdfControlDeviceInitAllocate failed with %!STATUS!",
                status
            );
            break;
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
            TraceError(
                TRACE_SIDEBAND,
                "WdfDeviceInitAssignName failed with %!STATUS!",
                status
            );
            break;
        }

        status = WdfDeviceCreate(&pInit,
            WDF_NO_OBJECT_ATTRIBUTES,
            &controlDevice);
        if (!NT_SUCCESS(status)) {
            break;
        }

        //
        // Create a symbolic link for the control object so that user-mode can open
        // the device.
        //

        status = WdfDeviceCreateSymbolicLink(controlDevice,
            &symbolicLinkName);

        if (!NT_SUCCESS(status)) {
            TraceError(
                TRACE_SIDEBAND,
                "WdfDeviceCreateSymbolicLink failed with %!STATUS!",
                status
            );
            break;
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
            TraceError(
                TRACE_SIDEBAND,
                "WdfIoQueueCreate failed with %!STATUS!",
                status
            );
            break;
        }

        //
        // Control devices must notify WDF when they are done initializing.   I/O is
        // rejected until this call is made.
        //
        WdfControlFinishInitializing(controlDevice);

        ControlDevice = controlDevice;

    } while (FALSE);

    if (pInit != NULL)
    {
	    WdfDeviceInitFree(pInit);
    }

    if (!NT_SUCCESS(status) && controlDevice != NULL)
    {
	    //
	    // Release the reference on the newly created object, since
	    // we couldn't initialize it.
	    //
	    WdfObjectDelete(controlDevice);
	    controlDevice = NULL;
    }

    FuncExit(TRACE_SIDEBAND, "status=%!STATUS!", status);

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

    TraceInformation(
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

    TraceInformation( TRACE_SIDEBAND, "%!FUNC! Entry");

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

    TraceInformation( TRACE_SIDEBAND, "%!FUNC! Exit");
}
#pragma warning(pop) // enable 28118 again

#endif
