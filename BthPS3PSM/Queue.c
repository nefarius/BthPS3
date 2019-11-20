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


#include "driver.h"
#include "queue.tmh"
#include <usb.h>
#include <usbioctl.h>
#include <wdfusb.h>


#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3PSM_QueueInitialize)
#endif


NTSTATUS
BthPS3PSM_QueueInitialize(
    _In_ WDFDEVICE Device
)
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;

    PAGED_CODE();

    //
    // Configure a default queue so that requests that are not
    // configure-forwarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchParallel
    );

    //
    // This is the only callback we're intercepting
    // 
    queueConfig.EvtIoInternalDeviceControl = BthPS3PSMEvtIoInternalDeviceControl;

    status = WdfIoQueueCreate(
        Device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "WdfIoQueueCreate failed with %!STATUS!",
            status
        );
        return status;
    }

    return status;
}

//
// Handle IRP_MJ_INTERNAL_DEVICE_CONTROL requests
// 
VOID
BthPS3PSMEvtIoInternalDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS                    status;
    WDF_REQUEST_SEND_OPTIONS    options;
    BOOLEAN                     ret;
    PIRP                        irp;
    PURB                        urb;
    WDFDEVICE                   device;
    PDEVICE_CONTEXT             pContext;


    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    device = WdfIoQueueGetDevice(Queue);
    pContext = DeviceGetContext(device);
    irp = WdfRequestWdmGetIrp(Request);

    //
    // As a BTHUSB lower filter driver we expect USB/URB traffic
    // 
    if (IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        urb = (PURB)URB_FROM_IRP(irp);

        switch (urb->UrbHeader.Function)
        {
#pragma region URB_FUNCTION_SELECT_CONFIGURATION

        case URB_FUNCTION_SELECT_CONFIGURATION:

            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_QUEUE,
                "<< URB_FUNCTION_SELECT_CONFIGURATION");

            //
            // "Hijack" this URB from the upper function driver and
            // use the framework functions to enumerate the endpoints
            // so we can later conveniently distinguish the pipes for
            // interrupt (HCI) and bulk (L2CAP) traffic coming through.
            // 
            status = ProxyUrbSelectConfiguration(urb, pContext);

            //
            // We configured the device in by proxy for the upper
            // function driver, so complete instead of forward.
            // 
            WdfRequestComplete(Request, status);
            return;

#pragma endregion

#pragma region URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER

        case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:

            //
            // This URB targets the bulk IN pipe so we attach a completion
            // routine to it so we can grab the incoming data once coming
            // back from the lower driver.
            // 
            if (urb->UrbBulkOrInterruptTransfer.PipeHandle ==
                WdfUsbTargetPipeWdmGetPipeHandle(pContext->BulkReadPipe))
            {
                TraceEvents(TRACE_LEVEL_VERBOSE,
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
                    WDF_NO_SEND_OPTIONS);

                if (ret == FALSE) {
                    status = WdfRequestGetStatus(Request);
                    TraceEvents(TRACE_LEVEL_ERROR,
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

    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
        WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)), &options);

    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "WdfRequestSend failed: %!STATUS!", status);
        WdfRequestComplete(Request, status);
    }

    return;
}
