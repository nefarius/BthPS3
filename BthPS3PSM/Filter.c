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
#include "filter.tmh"
#include <wdfusb.h>
#include <bthdef.h>
#include <ntintsafe.h>


 //
 // Gets called when URB_FUNCTION_SELECT_CONFIGURATION is coming our way
 // 
NTSTATUS
ProxyUrbSelectConfiguration(
	PURB Urb,
	PDEVICE_CONTEXT Context
)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS selectCfgParams;
	PUSBD_INTERFACE_INFORMATION pIface;
	WDF_USB_PIPE_INFORMATION pipeInfo;

	FuncEntry(TRACE_FILTER);

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
	if (!NT_SUCCESS(status = WdfUsbTargetDeviceSelectConfig(
		Context->UsbDevice,
		WDF_NO_OBJECT_ATTRIBUTES,
		&selectCfgParams
	)))
	{
		TraceError(
			TRACE_FILTER,
			"WdfUsbTargetDeviceSelectConfig failed with status %!STATUS!",
			status
		);

		goto exit;
	}

	//
	// There could be multiple interfaces although every 
	// tested and compatible host device uses the first
	// interface so we can get away with a bit of laziness ;)
	// 
	Context->UsbInterface = WdfUsbTargetDeviceGetInterface(
		Context->UsbDevice,
		pIface->InterfaceNumber);

	if (NULL == Context->UsbInterface) {
		status = STATUS_UNSUCCESSFUL;
		TraceError(
			TRACE_FILTER,
			"WdfUsbTargetDeviceGetInterface for interface %d failed with status %!STATUS!",
			pIface->InterfaceNumber,
			status
		);

		goto exit;
	}

	const UCHAR numberConfiguredPipes = WdfUsbInterfaceGetNumConfiguredPipes(Context->UsbInterface);

	for (UCHAR index = 0; index < numberConfiguredPipes; index++) {

		WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

		const WDFUSBPIPE pipe = WdfUsbInterfaceGetConfiguredPipe(
			Context->UsbInterface,
			index, //PipeIndex,
			&pipeInfo
		);

		if (WdfUsbPipeTypeInterrupt == pipeInfo.PipeType) {
			TraceInformation(
				TRACE_FILTER,
				"Interrupt Pipe is 0x%p",
				WdfUsbTargetPipeWdmGetPipeHandle(pipe)
			);

			Context->InterruptPipe = pipe;
		}

		if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
			WdfUsbTargetPipeIsInEndpoint(pipe)) {
			TraceInformation(
				TRACE_FILTER,
				"BulkInput Pipe is 0x%p",
				WdfUsbTargetPipeWdmGetPipeHandle(pipe)
			);

			Context->BulkReadPipe = pipe;
		}

		if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
			WdfUsbTargetPipeIsOutEndpoint(pipe)) {
			TraceInformation(
				TRACE_FILTER,
				"BulkOutput Pipe is 0x%p",
				WdfUsbTargetPipeWdmGetPipeHandle(pipe)
			);

			Context->BulkWritePipe = pipe;
		}
	}

	//
	// If we didn't find all 3 pipes, fail the request
	//
	if (!(Context->BulkWritePipe
		&& Context->BulkReadPipe && Context->InterruptPipe)) {
		status = STATUS_INVALID_DEVICE_STATE;

		TraceError(
			TRACE_FILTER,
			"Device is not configured properly %!STATUS!",
			status
		);

		goto exit;
	}

exit:
	FuncExit(TRACE_FILTER, "status=%!STATUS!", status);

	return status;
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
	PUCHAR buffer;
	UNREFERENCED_PARAMETER(Target);

	FuncEntry(TRACE_FILTER);

	const WDFDEVICE device = (WDFDEVICE)Context;
	const PDEVICE_CONTEXT pDevCtx = DeviceGetContext(device);
	const PIRP pIrp = WdfRequestWdmGetIrp(Request);
	const PURB pUrb = (PURB)URB_FROM_IRP(pIrp);

	const struct _URB_BULK_OR_INTERRUPT_TRANSFER* pTransfer = &pUrb->UrbBulkOrInterruptTransfer;

	const ULONG bufferLength = pTransfer->TransferBufferLength;
	buffer = (PUCHAR)USBPcapURBGetBufferPointer(
		pTransfer->TransferBufferLength,
		pTransfer->TransferBuffer,
		pTransfer->TransferBufferMDL
	);

	if (
		bufferLength >= L2CAP_MIN_BUFFER_LEN
		&& L2CAP_IS_CONTROL_CHANNEL(buffer)
		&& L2CAP_IS_SIGNALLING_COMMAND_CODE(buffer)
		)
	{
		const L2CAP_SIGNALLING_COMMAND_CODE code = L2CAP_GET_SIGNALLING_COMMAND_CODE(buffer);

		if (code == L2CAP_Connection_Request)
		{
			const PL2CAP_SIGNALLING_CONNECTION_REQUEST pConReq = (PL2CAP_SIGNALLING_CONNECTION_REQUEST)&buffer[8];

			if (pConReq->PSM == PSM_HID_CONTROL)
			{
				TraceVerbose(
					TRACE_FILTER,
					">> Connection request for HID Control PSM 0x%04X arrived",
					pConReq->PSM);

				if (pDevCtx->IsPsmPatchingEnabled)
				{
					pConReq->PSM = PSM_DS3_HID_CONTROL;

					TraceInformation(
						TRACE_FILTER,
						"++ Patching HID Control PSM to 0x%04X",
						pConReq->PSM);
				}
				else
				{
					TraceVerbose(
						TRACE_FILTER,
						"-- NOT Patching HID Control PSM"
					);
				}
			}

			if (pConReq->PSM == PSM_HID_INTERRUPT)
			{
				TraceVerbose(
					TRACE_FILTER,
					">> Connection request for HID Interrupt PSM 0x%04X arrived",
					pConReq->PSM);

				if (pDevCtx->IsPsmPatchingEnabled)
				{
					pConReq->PSM = PSM_DS3_HID_INTERRUPT;

					TraceInformation(
						TRACE_FILTER,
						"++ Patching HID Interrupt PSM to 0x%04X",
						pConReq->PSM);
				}
				else
				{
					TraceVerbose(
						TRACE_FILTER,
						"-- NOT Patching HID Interrupt PSM"
					);
				}
			}
		}
	}

	WdfRequestComplete(Request, Params->IoStatus.Status);

	FuncExitNoReturn(TRACE_FILTER);
}
