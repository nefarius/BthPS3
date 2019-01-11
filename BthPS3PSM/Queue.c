/*++

Module Name:

    queue.c

Abstract:

    This file contains the queue entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "queue.tmh"
#include <usb.h>
#include <usbioctl.h>
#include <wdfusb.h>


#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3PSMQueueInitialize)
#endif

NTSTATUS
BthPS3PSMQueueInitialize(
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
    PIO_STACK_LOCATION          irpStack;
    WDFDEVICE                   device;
    PDEVICE_CONTEXT             pContext;


    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    device = WdfIoQueueGetDevice(Queue);
    pContext = DeviceGetContext(device);

    irp = WdfRequestWdmGetIrp(Request);
    irpStack = IoGetCurrentIrpStackLocation(irp);

    switch (IoControlCode)
    {
    case IOCTL_INTERNAL_USB_SUBMIT_URB:

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            ">> IOCTL_INTERNAL_USB_SUBMIT_URB");

        urb = (PURB)URB_FROM_IRP(irp);

        switch (urb->UrbHeader.Function)
        {
#pragma region URB_FUNCTION_SELECT_CONFIGURATION

        case URB_FUNCTION_SELECT_CONFIGURATION:

            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_QUEUE,
                ">> >> URB_FUNCTION_SELECT_CONFIGURATION");

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

        case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:

            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_QUEUE,
                ">> >> URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER (PipeHandle: %p)",
                urb->UrbBulkOrInterruptTransfer.PipeHandle);

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
                    "Bulk IN transfer"
                );

                WdfRequestFormatRequestUsingCurrentType(Request);

                WdfRequestSetCompletionRoutine(
                    Request,
                    UrbFunctionBulkInTransferCompleted,
                    NULL // TODO: what makes sense to pass here?
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

        default:
            break;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            "<<");

        break;
    default:
        break;
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
