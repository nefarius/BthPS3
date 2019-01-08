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
// Gets called when Interrupt IN (HCI) data is available
// 
VOID
UrbFunctionInterruptInTransferCompleted(
    IN WDFREQUEST Request,
    IN WDFIOTARGET Target,
    IN PWDF_REQUEST_COMPLETION_PARAMS Params,
    IN WDFCONTEXT Context
)
{
    PIRP                    pIrp;
    PURB                    pUrb;
    PIO_STACK_LOCATION      pStack;
    PUCHAR                  buffer;
    ULONG                   bufferLength;

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

    if (bufferLength < 8)
    {
        WdfRequestComplete(Request, Params->IoStatus.Status);
        return;
    }

    HCI_EVENT event = (HCI_EVENT)buffer[0];

    switch (event)
    {
    case HCI_Connection_Complete_EV:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_FILTER,
            "HCI_Connection_Complete_EV");

        TraceDumpBuffer(buffer, bufferLength);

        break;
    case HCI_Remote_Name_Request_Complete_EV:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_FILTER,
            "HCI_Remote_Name_Request_Complete_EV");

        TraceDumpBuffer(buffer, bufferLength);

        if (buffer[2] == 0x00)
        {
            ULONG length;

            //
            // Scan through rest of buffer until null-terminator is found
            //
            for (length = 1;
                buffer[length + 8] != 0x00
                && (length + 8) < bufferLength;
                length++);

            switch (buffer[9])
            {
            case 'P': // First letter in PLAYSTATION(R)3 Controller ('P')
                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_FILTER,
                    "PLAYSTATION(R)3 Controller"
                );
                break;
            case 'N': // First letter in Navigation Controller ('N')
                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_FILTER,
                    "Navigation Controller"
                );
                break;
            case 'M': // First letter in Motion Controller ('M')
                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_FILTER,
                    "Motion Controller"
                );
                break;
            case 'W': // First letter in Wireless Controller ('W')
                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_FILTER,
                    "Wireless Controller"
                );
                break;
            default:
                break;
            }
        }

        break;
    default:

        //TraceDumpBuffer(buffer, pTransfer->TransferBufferLength);

        break;
    }



    WdfRequestComplete(Request, Params->IoStatus.Status);
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
    PIRP                    pIrp;
    PURB                    pUrb;
    PIO_STACK_LOCATION      pStack;
    PUCHAR                  buffer;
    ULONG                   bufferLength;
    L2CAP_SIGNALLING_COMMAND_CODE   code;

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

    if (bufferLength < 8)
    {
        TraceDumpBuffer(buffer, pTransfer->TransferBufferLength);

        WdfRequestComplete(Request, Params->IoStatus.Status);
        return;
    }

    if (L2CAP_IS_CONTROL_CHANNEL(buffer))
    {
        if (L2CAP_IS_SIGNALLING_COMMAND_CODE(buffer))
        {
            code = L2CAP_GET_SIGNALLING_COMMAND_CODE(buffer);

            if (code == L2CAP_Connection_Request)
            {
                PL2CAP_SIGNALLING_CONNECTION_REQUEST data = (PL2CAP_SIGNALLING_CONNECTION_REQUEST)&buffer[8];

                if (data->PSM == PSM_HID_CONTROL)
                {
                    data->PSM = 0x5053;

                    TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_FILTER,
                        "++ Patching PSM to 0x%04X",
                        data->PSM);
                }

                if (data->PSM == PSM_HID_INTERRUPT)
                {
                    data->PSM = 0x5055;

                    TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_FILTER,
                        "++ Patching PSM to 0x%04X",
                        data->PSM);
                }
            }

            DumpL2CAPSignallingCommandCode(">>", code, &buffer[8]);
        }
    }

    WdfRequestComplete(Request, Params->IoStatus.Status);
}

