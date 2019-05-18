/*
* BthPS3PSM - Windows kernel-mode BTHUSB lower filter driver
* Copyright (C) 2018-2019  Nefarius Software Solutions e.U. and Contributors
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
    WDF_OBJECT_ATTRIBUTES   controlAttributes;
    WDF_IO_QUEUE_CONFIG     ioQueueConfig;
    WDFQUEUE                queue;

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
        &SDDL_DEVOBJ_SYS_ALL_ADM_ALL
    );

    if (pInit == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfControlDeviceInitAllocate failed with %!STATUS!", status);
        goto Error;
    }

    WdfDeviceInitSetExclusive(pInit, FALSE);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&controlAttributes,
        CONTROL_DEVICE_CONTEXT);
    status = WdfDeviceCreate(&pInit,
        &controlAttributes,
        &controlDevice);
    if (!NT_SUCCESS(status)) {
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
    // Expose device interface
    // 
    status = WdfDeviceCreateDeviceInterface(
        Device,
        (LPGUID)&GUID_DEVINTERFACE_BTHPS3PSM,
        NULL
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_SIDEBAND,
            "WdfDeviceCreateDeviceInterface failed with %!STATUS!",
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
    

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_SIDEBAND, "%!FUNC! Exit");
}
#pragma warning(pop) // enable 28118 again

#endif
