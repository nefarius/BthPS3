/*
* BthPS3 - Windows kernel-mode Bluetooth profile and bus driver
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


#include "driver.h"
#include "queue.tmh"
#include <stdio.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3QueueInitialize)
#endif

NTSTATUS
BthPS3QueueInitialize(
    _In_ WDFDEVICE Device
)
/*++

Routine Description:

     The I/O dispatch callbacks for the frameworks device object
     are configured in this function.

     A single default I/O Queue is configured for parallel request
     processing, and a driver context memory allocation is created
     to hold our structure QUEUE_CONTEXT.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    VOID

--*/
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;

    PAGED_CODE();

    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchParallel
    );

    queueConfig.EvtIoStop = BthPS3_EvtIoStop;
    queueConfig.EvtIoInternalDeviceControl = BthPS3_EvtWdfIoQueueIoInternalDeviceControl;

    status = WdfIoQueueCreate(
        Device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    return status;
}

VOID
TraceDumpBuffer(
    PVOID Buffer,
    ULONG BufferLength
)
{
    PWSTR   dumpBuffer;
    size_t  dumpBufferLength;
    ULONG   i;

    dumpBufferLength = ((BufferLength * sizeof(WCHAR)) * 3) + 2;
    dumpBuffer = ExAllocatePoolWithTag(
        NonPagedPoolNx,
        dumpBufferLength,
        'egrA'
    );
    RtlZeroMemory(dumpBuffer, dumpBufferLength);

    for (i = 0; i < BufferLength; i++)
    {
        swprintf(&dumpBuffer[i * 3], L"%02X ", ((PUCHAR)Buffer)[i]);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_DEVICE,
        "%ws",
        dumpBuffer);

    ExFreePoolWithTag(dumpBuffer, 'egrA');
}

//
// Handle IRP_MJ_INTERNAL_DEVICE_CONTROL sent to FDO
// 
void BthPS3_EvtWdfIoQueueIoInternalDeviceControl(
    WDFQUEUE Queue,
    WDFREQUEST Request,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode
)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PBTHPS3_FDO_PDO_REQUEST_CONTEXT reqCtx = NULL;
    WDFDEVICE device = NULL;
    PBTHPS3_SERVER_CONTEXT deviceCtx = NULL;
    PBTHPS3_CLIENT_CONNECTION clientConnection = NULL;
    PVOID buffer = NULL;
    size_t bufferLength = 0;


    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_QUEUE, "%!FUNC! Entry");

    reqCtx = GetFdoPdoRequestContext(Request);

    //
    // This isn't supposed to happen, bail out
    // 
    if (reqCtx->ClientConnection == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "Invalid client connection handle"
        );

        WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
        return;
    }

    device = WdfIoQueueGetDevice(Queue);
    deviceCtx = GetServerDeviceContext(device);
    clientConnection = reqCtx->ClientConnection;

    switch (IoControlCode)
    {
#pragma region IOCTL_BTHPS3_HID_CONTROL_READ

    case IOCTL_BTHPS3_HID_CONTROL_READ:

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            ">> IOCTL_BTHPS3_HID_CONTROL_READ"
        );

        status = WdfRequestRetrieveOutputBuffer(
            Request,
            OutputBufferLength,
            &buffer,
            &bufferLength
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
                status
            );
            break;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            "bufferLength: %d",
            (ULONG)bufferLength
        );

        status = L2CAP_PS3_ReadControlTransferAsync(
            clientConnection,
            Request,
            buffer,
            bufferLength,
            L2CAP_PS3_AsyncReadControlTransferCompleted
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "L2CAP_PS3_ReadControlTransferAsync failed with status %!STATUS!",
                status
            );
        }
        else
        {
            status = STATUS_PENDING;
        }

        break;

#pragma endregion

#pragma region IOCTL_BTHPS3_HID_CONTROL_WRITE

    case IOCTL_BTHPS3_HID_CONTROL_WRITE:

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            ">> IOCTL_BTHPS3_HID_CONTROL_WRITE"
        );

        status = WdfRequestRetrieveInputBuffer(
            Request,
            InputBufferLength,
            &buffer,
            &bufferLength
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
                status
            );
            break;
        }

        status = L2CAP_PS3_SendControlTransferAsync(
            clientConnection,
            Request,
            buffer,
            bufferLength,
            L2CAP_PS3_AsyncSendControlTransferCompleted
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "L2CAP_PS3_SendControlTransferAsync failed with status %!STATUS!",
                status
            );
        }
        else
        {
            status = STATUS_PENDING;
        }

        break;

#pragma endregion

#pragma region IOCTL_BTHPS3_HID_INTERRUPT_READ

    case IOCTL_BTHPS3_HID_INTERRUPT_READ:

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            ">> IOCTL_BTHPS3_HID_INTERRUPT_READ"
        );

        status = WdfRequestRetrieveOutputBuffer(
            Request,
            OutputBufferLength,
            &buffer,
            &bufferLength
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
                status
            );
            break;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            "bufferLength: %d",
            (ULONG)bufferLength
        );

        status = L2CAP_PS3_ReadInterruptTransferAsync(
            clientConnection,
            Request,
            buffer,
            bufferLength,
            L2CAP_PS3_AsyncReadInterruptTransferCompleted
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "L2CAP_PS3_ReadInterruptTransferAsync failed with status %!STATUS!",
                status
            );
        }
        else
        {
            status = STATUS_PENDING;
        }

        break;

#pragma endregion

#pragma region IOCTL_BTHPS3_HID_INTERRUPT_WRITE

    case IOCTL_BTHPS3_HID_INTERRUPT_WRITE:

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            ">> IOCTL_BTHPS3_HID_INTERRUPT_WRITE"
        );

        status = WdfRequestRetrieveInputBuffer(
            Request,
            InputBufferLength,
            &buffer,
            &bufferLength
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
                status
            );
            break;
        }

        status = L2CAP_PS3_SendInterruptTransferAsync(
            clientConnection,
            Request,
            buffer,
            bufferLength,
            L2CAP_PS3_AsyncSendInterruptTransferCompleted
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "L2CAP_PS3_SendInterruptTransferAsync failed with status %!STATUS!",
                status
            );
        }
        else
        {
            status = STATUS_PENDING;
        }

        break;

#pragma endregion

    default:
        TraceEvents(TRACE_LEVEL_WARNING,
            TRACE_QUEUE,
            "Unknown IoControlCode received: 0x%X",
            IoControlCode
        );

        status = STATUS_INVALID_PARAMETER;
        break;
    }

    if (status != STATUS_PENDING) {
        WdfRequestComplete(Request, status);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_QUEUE, "%!FUNC! Exit (status: %!STATUS!)", status);
}

//
// Gets invoked when the queue operation is stopped and outstanding requests need to be handled
// 
VOID
BthPS3_EvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
{
    BOOLEAN ret;

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_QUEUE,
        "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d",
        Queue, Request, ActionFlags);

    ret = WdfRequestCancelSentRequest(Request);

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_QUEUE,
        "WdfRequestCancelSentRequest returned %d",
        ret
    );

    return;
}
