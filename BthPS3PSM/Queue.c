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
#include "queue.tmh"
#include <usb.h>
#include <usbioctl.h>
#include <wdfusb.h>
#include <BthPS3PSMETW.h>


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
        EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfDeviceOpenRegistryKey", status);

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
	NTSTATUS status;
	WDF_REQUEST_SEND_OPTIONS options;
	BOOLEAN ret;


	UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

	const WDFDEVICE device = WdfIoQueueGetDevice(Queue);
	const PDEVICE_CONTEXT pContext = DeviceGetContext(device);
	const PIRP irp = WdfRequestWdmGetIrp(Request);

    //
    // As a BTHUSB lower filter driver we expect USB/URB traffic
    // 
    if (IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
	    const PURB urb = (PURB)URB_FROM_IRP(irp);

        switch (urb->UrbHeader.Function)
        {
#pragma region URB_FUNCTION_SELECT_CONFIGURATION

        case URB_FUNCTION_SELECT_CONFIGURATION:

            TraceVerbose(
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
                    WDF_NO_SEND_OPTIONS);

                if (ret == FALSE) {
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

    if (ret == FALSE) {
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
