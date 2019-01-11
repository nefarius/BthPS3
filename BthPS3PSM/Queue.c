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

    queueConfig.EvtIoDeviceControl = BthPS3PSMEvtIoDeviceControl;
    queueConfig.EvtIoStop = BthPS3PSMEvtIoStop;
    queueConfig.EvtIoInternalDeviceControl = BthPS3PSMEvtIoInternalDeviceControl;

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
BthPS3PSMEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
/*++

Routine Description:

    This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    OutputBufferLength - Size of the output buffer in bytes

    InputBufferLength - Size of the input buffer in bytes

    IoControlCode - I/O control code.

Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_QUEUE,
        "%!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %d",
        Queue, Request, (int)OutputBufferLength, (int)InputBufferLength, IoControlCode);

    WdfRequestComplete(Request, STATUS_SUCCESS);

    return;
}

VOID
BthPS3PSMEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
/*++

Routine Description:

    This event is invoked for a power-managed queue before the device leaves the working state (D0).

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    ActionFlags - A bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS-typed flags
                  that identify the reason that the callback function is being called
                  and whether the request is cancelable.

Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_QUEUE,
        "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d",
        Queue, Request, ActionFlags);

    //
    // In most cases, the EvtIoStop callback function completes, cancels, or postpones
    // further processing of the I/O request.
    //
    // Typically, the driver uses the following rules:
    //
    // - If the driver owns the I/O request, it calls WdfRequestUnmarkCancelable
    //   (if the request is cancelable) and either calls WdfRequestStopAcknowledge
    //   with a Requeue value of TRUE, or it calls WdfRequestComplete with a
    //   completion status value of STATUS_SUCCESS or STATUS_CANCELLED.
    //
    //   Before it can call these methods safely, the driver must make sure that
    //   its implementation of EvtIoStop has exclusive access to the request.
    //
    //   In order to do that, the driver must synchronize access to the request
    //   to prevent other threads from manipulating the request concurrently.
    //   The synchronization method you choose will depend on your driver's design.
    //
    //   For example, if the request is held in a shared context, the EvtIoStop callback
    //   might acquire an internal driver lock, take the request from the shared context,
    //   and then release the lock. At this point, the EvtIoStop callback owns the request
    //   and can safely complete or requeue the request.
    //
    // - If the driver has forwarded the I/O request to an I/O target, it either calls
    //   WdfRequestCancelSentRequest to attempt to cancel the request, or it postpones
    //   further processing of the request and calls WdfRequestStopAcknowledge with
    //   a Requeue value of FALSE.
    //
    // A driver might choose to take no action in EvtIoStop for requests that are
    // guaranteed to complete in a small amount of time.
    //
    // In this case, the framework waits until the specified request is complete
    // before moving the device (or system) to a lower power state or removing the device.
    // Potentially, this inaction can prevent a system from entering its hibernation state
    // or another low system power state. In extreme cases, it can cause the system
    // to crash with bugcheck code 9F.
    //

    return;
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
