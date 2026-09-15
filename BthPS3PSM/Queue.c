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
#include "queue.tmh"
#include <usb.h>
#include <usbioctl.h>
#include <BthPS3PSMETW.h>


#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3PSM_QueueInitialize)
#endif


_Use_decl_annotations_
NTSTATUS
BthPS3PSM_QueueInitialize(
    _In_ WDFDEVICE Device
)
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;

    PAGED_CODE();

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchParallel
    );

    //
    // Required for device removal: cancel forwarded requests so queue can drain
    //
    queueConfig.EvtIoStop = BthPS3PSM_EvtIoStop;

    //
    // USB traffic (URBs) arrives via IRP_MJ_INTERNAL_DEVICE_CONTROL, while a
    // BTHX (Bluetooth Extensibility Transport, e.g. BthMini-bound PCIe/UART
    // radios) transport uses regular IRP_MJ_DEVICE_CONTROL requests
    //
    queueConfig.EvtIoInternalDeviceControl = BthPS3PSMEvtIoInternalDeviceControl;
    queueConfig.EvtIoDeviceControl = BthPS3PSM_EvtIoDeviceControl;

    //
    // BthMini.sys is known to issue BTHX DDI requests (e.g.
    // IOCTL_BTHX_GET_VERSION/IOCTL_BTHX_QUERY_CAPABILITIES) before its
    // device reaches D0; a power-managed queue would only dispatch once D0
    // is reached, which can deadlock device start. Disable power management
    // for this queue; it merely forwards/inspects requests and does not
    // need to be held back for D0.
    //
    queueConfig.PowerManaged = WdfFalse;

    if (!NT_SUCCESS(status = WdfIoQueueCreate(
        Device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue
    )))
    {
        TraceError(
            TRACE_QUEUE,
            "WdfIoQueueCreate failed with %!STATUS!",
            status
        );
        EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfIoQueueCreate", status);

        return status;
    }

    return status;
}

//
// Called when device is suspended or removed; allows queue to drain by
// cancelling forwarded requests that may otherwise hang during teardown
//
_Use_decl_annotations_
VOID
BthPS3PSM_EvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Queue);

    FuncEntry(TRACE_QUEUE);

    if (ActionFlags & WdfRequestStopRequestCancelable)
    {
        status = WdfRequestUnmarkCancelable(Request);
        if (status == STATUS_CANCELLED)
        {
            TraceVerbose(
                TRACE_QUEUE,
                "Request=0x%p already cancelled, returning",
                Request
            );
            FuncExitNoReturn(TRACE_QUEUE);
            return;
        }
    }

    if (ActionFlags & WdfRequestStopActionSuspend)
    {
        TraceVerbose(
            TRACE_QUEUE,
            "StopAcknowledge Request=0x%p (Suspend)",
            Request
        );
        WdfRequestStopAcknowledge(Request, FALSE);
    }
    else if (ActionFlags & WdfRequestStopActionPurge)
    {
        TraceVerbose(
            TRACE_QUEUE,
            "CancelSentRequest Request=0x%p (Purge)",
            Request
        );
        WdfRequestCancelSentRequest(Request);
    }
    else
    {
        TraceVerbose(
            TRACE_QUEUE,
            "CancelSentRequest Request=0x%p (fallback)",
            Request
        );
        WdfRequestCancelSentRequest(Request);
    }

    FuncExitNoReturn(TRACE_QUEUE);
}

//
// Handle IRP_MJ_INTERNAL_DEVICE_CONTROL requests
// 
_Use_decl_annotations_
VOID
BthPS3PSMEvtIoInternalDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS status;
    WDF_REQUEST_SEND_OPTIONS options;
    BOOLEAN ret;


    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    const WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    const PDEVICE_CONTEXT pContext = DeviceGetContext(device);
    const PIRP irp = WdfRequestWdmGetIrp(Request);

    //
    // On the USB transport we expect URB traffic; on BTHX (handled by
    // BthPS3PSM_EvtIoDeviceControl) this major function is unused
    // 
    if (pContext->TransportType == BthPS3PsmTransportUsb
        && IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        const PURB urb = (PURB)URB_FROM_IRP(irp);

        switch (urb->UrbHeader.Function)
        {
#pragma region URB_FUNCTION_SELECT_CONFIGURATION

        case URB_FUNCTION_SELECT_CONFIGURATION:

            TraceVerbose(
                TRACE_QUEUE,
                "<< URB_FUNCTION_SELECT_CONFIGURATION");

            WdfRequestFormatRequestUsingCurrentType(Request);

            WdfRequestSetCompletionRoutine(
                Request,
                UrbSelectConfigurationCompleted,
                device
            );

            ret = WdfRequestSend(
                Request,
                WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)),
                WDF_NO_SEND_OPTIONS
            );

            if (ret == FALSE)
            {
                status = WdfRequestGetStatus(Request);
                TraceError(
                    TRACE_QUEUE,
                    "WdfRequestSend failed with status %!STATUS!",
                    status
                );
                WdfRequestComplete(Request, status);
            }

            return;

#pragma endregion

#pragma region URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER

        case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:

            //
            // This URB targets the bulk IN pipe so we attach a completion
            // routine to it so we can grab the incoming data once coming
            // back from the lower driver.
            // 
            if (urb->UrbBulkOrInterruptTransfer.PipeHandle == pContext->BulkReadPipe)
            {
                TraceVerbose(
                    TRACE_QUEUE,
                    ">> Bulk IN transfer (PipeHandle: %p)",
                    urb->UrbBulkOrInterruptTransfer.PipeHandle
                );

                WdfRequestFormatRequestUsingCurrentType(Request);

                WdfRequestSetCompletionRoutine(
                    Request,
                    UrbFunctionBulkInTransferCompleted,
                    device
                );

                ret = WdfRequestSend(
                    Request,
                    WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)),
                    WDF_NO_SEND_OPTIONS
                );

                if (ret == FALSE)
                {
                    status = WdfRequestGetStatus(Request);
                    TraceError(
                        TRACE_QUEUE,
                        "WdfRequestSend failed with status %!STATUS!",
                        status
                    );
                    WdfRequestComplete(Request, status);
                }

                return;
            }

            break;

#pragma endregion

        default:
            break;
        }
    }

    //
    // Request not for us, forward
    // 
    WdfRequestFormatRequestUsingCurrentType(Request);

    WDF_REQUEST_SEND_OPTIONS_INIT(
        &options,
        WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET
    );

    ret = WdfRequestSend(
        Request,
        WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)),
        &options
    );

    if (ret == FALSE)
    {
        status = WdfRequestGetStatus(Request);
        TraceError(
            TRACE_QUEUE,
            "WdfRequestSend failed with status %!STATUS!",
            status
        );
        EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfRequestGetStatus", status);
        WdfRequestComplete(Request, status);
    }
}

//
// Handle IRP_MJ_DEVICE_CONTROL requests
//
// BTHX (Bluetooth Extensibility Transport) radios deliver their HCI traffic
// via regular device control requests instead of USB URBs; we hook
// IOCTL_BTHX_READ_HCI here to inspect/patch inbound ACL Data reads.
// 
_Use_decl_annotations_
VOID
BthPS3PSM_EvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS status;
    WDF_REQUEST_SEND_OPTIONS options;
    BOOLEAN ret;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    const WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    const PDEVICE_CONTEXT pContext = DeviceGetContext(device);

    if (pContext->TransportType == BthPS3PsmTransportBthx
        && IoControlCode == IOCTL_BTHX_READ_HCI)
    {
        WDFMEMORY outputMemory = NULL;
        PVOID outputBuffer = NULL;
        size_t outputBufferLength = 0;

        TraceVerbose(
            TRACE_QUEUE,
            "<< IOCTL_BTHX_READ_HCI");

        //
        // This IOCTL is always forwarded to us by BthMini.sys, i.e. it
        // originates from another kernel-mode driver in this device stack.
        // Per WdfRequestRetrieveOutputMemory's documented contract, that
        // means it also supports METHOD_NEITHER requests (unlike a plain
        // user-mode METHOD_NEITHER request, which would require an
        // EvtIoInCallerContext callback and
        // WdfRequestRetrieveUnsafeUserOutputBuffer instead). The resulting
        // WDFMEMORY/buffer stays valid until the request completes, so the
        // completion routine can safely inspect it, including once the
        // request comes back up from the BTHX transport stack at a raised
        // IRQL.
        // 
        status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);

        if (NT_SUCCESS(status))
        {
            outputBuffer = WdfMemoryGetBuffer(outputMemory, &outputBufferLength);
        }

        if (!NT_SUCCESS(status)
            || outputBuffer == NULL
            || outputBufferLength < FIELD_OFFSET(BTHX_HCI_READ_WRITE_CONTEXT, Data))
        {
            TraceError(
                TRACE_QUEUE,
                "IOCTL_BTHX_READ_HCI output buffer unavailable or too small (status %!STATUS!, length %Iu)",
                status,
                outputBufferLength
            );

            WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
            return;
        }

        WDF_OBJECT_ATTRIBUTES attributes;
        PBTHX_READ_REQUEST_CONTEXT requestContext = NULL;

        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, BTHX_READ_REQUEST_CONTEXT);

        if (NT_SUCCESS(WdfObjectAllocateContext(
            Request,
            &attributes,
            (PVOID*)&requestContext
        )))
        {
            requestContext->OutputBuffer = outputBuffer;
            requestContext->OutputBufferLength = outputBufferLength;

            WdfRequestFormatRequestUsingCurrentType(Request);

            WdfRequestSetCompletionRoutine(
                Request,
                BthxReadHciCompleted,
                device
            );

            ret = WdfRequestSend(
                Request,
                WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)),
                WDF_NO_SEND_OPTIONS
            );

            if (ret == FALSE)
            {
                status = WdfRequestGetStatus(Request);
                TraceError(
                    TRACE_QUEUE,
                    "WdfRequestSend failed with status %!STATUS!",
                    status
                );
                WdfRequestComplete(Request, status);
            }

            return;
        }

        TraceVerbose(
            TRACE_QUEUE,
            "-- Couldn't allocate request context for IOCTL_BTHX_READ_HCI, forwarding unmodified"
        );
    }

    //
    // Request not for us (or we couldn't hook it), forward
    // 
    WdfRequestFormatRequestUsingCurrentType(Request);

    WDF_REQUEST_SEND_OPTIONS_INIT(
        &options,
        WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET
    );

    ret = WdfRequestSend(
        Request,
        WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)),
        &options
    );

    if (ret == FALSE)
    {
        status = WdfRequestGetStatus(Request);
        TraceError(
            TRACE_QUEUE,
            "WdfRequestSend failed with status %!STATUS!",
            status
        );
        EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfRequestGetStatus", status);
        WdfRequestComplete(Request, status);
    }
}
