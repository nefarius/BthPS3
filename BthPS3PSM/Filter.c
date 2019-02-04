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
#include "filter.tmh"
#include <wdfusb.h>
#include <bthdef.h>
#include <ntintsafe.h>

NTSTATUS
ProxyUrbSelectConfiguration(
    PURB Urb,
    PDEVICE_CONTEXT Context
)
{
    NTSTATUS                                status;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS     selectCfgParams;
    PUSBD_INTERFACE_INFORMATION             pIface;
    UCHAR                                   numberConfiguredPipes;
    UCHAR                                   index;
    WDFUSBPIPE                              pipe;
    WDF_USB_PIPE_INFORMATION                pipeInfo;

    pIface = &Urb->UrbSelectConfiguration.Interface;

    //
    // "Hijack" this URB from the upper function driver and
    // use the framework functions to enumerate the endpoints
    // so we can later conveniently distinguish the pipes for
    // interrupt (HCI) and bulk (L2CAP) traffic coming through.
    // 
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_URB(
        &selectCfgParams,
        Urb
    );

    // 
    // This call sends our own request down the stack
    // but with the desired configuration mirrored from
    // the upper request we're currently keeping on hold.
    // 
    status = WdfUsbTargetDeviceSelectConfig(
        Context->UsbDevice,
        WDF_NO_OBJECT_ATTRIBUTES,
        &selectCfgParams
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_FILTER,
            "WdfUsbTargetDeviceSelectConfig failed with status %!STATUS!",
            status
        );

        return status;
    }

    //
    // There could be multiple interfaces although every 
    // tested and compatible host device uses the first
    // interface so we can get away with a bit of lazyness ;)
    // 
    Context->UsbInterface = WdfUsbTargetDeviceGetInterface(
        Context->UsbDevice,
        pIface->InterfaceNumber);

    if (NULL == Context->UsbInterface) {
        status = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_FILTER,
            "WdfUsbTargetDeviceGetInterface for interface %d failed with status %!STATUS!",
            pIface->InterfaceNumber,
            status
        );

        return status;
    }

    numberConfiguredPipes = WdfUsbInterfaceGetNumConfiguredPipes(Context->UsbInterface);

    for (index = 0; index < numberConfiguredPipes; index++) {

        WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

        pipe = WdfUsbInterfaceGetConfiguredPipe(
            Context->UsbInterface,
            index, //PipeIndex,
            &pipeInfo
        );

        if (WdfUsbPipeTypeInterrupt == pipeInfo.PipeType) {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_FILTER,
                "Interrupt Pipe is 0x%p",
                WdfUsbTargetPipeWdmGetPipeHandle(pipe)
            );

            Context->InterruptPipe = pipe;
        }

        if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
            WdfUsbTargetPipeIsInEndpoint(pipe)) {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_FILTER,
                "BulkInput Pipe is 0x%p",
                WdfUsbTargetPipeWdmGetPipeHandle(pipe)
            );

            Context->BulkReadPipe = pipe;
        }

        if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
            WdfUsbTargetPipeIsOutEndpoint(pipe)) {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_FILTER,
                "BulkOutput Pipe is 0x%p",
                WdfUsbTargetPipeWdmGetPipeHandle(pipe)
            );

            Context->BulkWritePipe = pipe;
        }
    }

    //
    // If we didn't find all 3 pipes, fail to start.
    //
    if (!(Context->BulkWritePipe
        && Context->BulkReadPipe && Context->InterruptPipe)) {
        status = STATUS_INVALID_DEVICE_STATE;

        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_FILTER,
            "Device is not configured properly %!STATUS!",
            status
        );

        return status;
    }

    return STATUS_SUCCESS;
}

//
// Gets called when Bulk IN (L2CAP) data is available
// 
VOID
UrbFunctionBulkInTransferCompleted(
    IN WDFREQUEST Request,
    IN WDFIOTARGET Target,
    IN PWDF_REQUEST_COMPLETION_PARAMS Params,
    IN WDFCONTEXT Context
)
{
    PIRP                                    pIrp;
    PURB                                    pUrb;
    PIO_STACK_LOCATION                      pStack;
    PUCHAR                                  buffer;
    ULONG                                   bufferLength;
    L2CAP_SIGNALLING_COMMAND_CODE           code;
    PL2CAP_SIGNALLING_CONNECTION_REQUEST    pConReq;

    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_FILTER, "%!FUNC! Entry");

    pIrp = WdfRequestWdmGetIrp(Request);
    pStack = IoGetCurrentIrpStackLocation(pIrp);
    pUrb = (PURB)URB_FROM_IRP(pIrp);

    struct _URB_BULK_OR_INTERRUPT_TRANSFER *pTransfer = &pUrb->UrbBulkOrInterruptTransfer;

    bufferLength = pTransfer->TransferBufferLength;
    buffer = (PUCHAR)USBPcapURBGetBufferPointer(
        pTransfer->TransferBufferLength,
        pTransfer->TransferBuffer,
        pTransfer->TransferBufferMDL
    );

    if (bufferLength >= L2CAP_MIN_BUFFER_LEN && L2CAP_IS_CONTROL_CHANNEL(buffer))
    {
        if (L2CAP_IS_SIGNALLING_COMMAND_CODE(buffer))
        {
            code = L2CAP_GET_SIGNALLING_COMMAND_CODE(buffer);

            if (code == L2CAP_Connection_Request)
            {
                pConReq = (PL2CAP_SIGNALLING_CONNECTION_REQUEST)&buffer[8];

                if (pConReq->PSM == PSM_HID_CONTROL)
                {
                    pConReq->PSM = PSM_DS3_HID_CONTROL;

                    TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_FILTER,
                        "++ Patching PSM to 0x%04X",
                        pConReq->PSM);
                }

                if (pConReq->PSM == PSM_HID_INTERRUPT)
                {
                    pConReq->PSM = PSM_DS3_HID_INTERRUPT;

                    TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_FILTER,
                        "++ Patching PSM to 0x%04X",
                        pConReq->PSM);
                }
            }
        }
    }

    WdfRequestComplete(Request, Params->IoStatus.Status);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_FILTER, "%!FUNC! Exit");
}
