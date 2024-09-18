/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2024, Nefarius Software Solutions e.U.                      *
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


#include "Driver.h"
#include "BusLogic.IO.tmh"


 //
 // Handles IOCTL_BTHPS3_HID_CONTROL_READ
 // 
NTSTATUS
BthPS3_PDO_HandleHidControlRead(
	_In_ DMFMODULE DmfModule,
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ ULONG IoctlCode,
	_In_reads_(InputBufferSize) VOID* InputBuffer,
	_In_ size_t InputBufferSize,
	_Out_writes_(OutputBufferSize) VOID* OutputBuffer,
	_In_ size_t OutputBufferSize,
	_Out_ size_t* BytesReturned
)
{
	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(IoctlCode);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferSize);
	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(OutputBuffer);

	FuncEntry(TRACE_BUSLOGIC);

	NTSTATUS status;
	const WDFDEVICE device = DMF_ParentDeviceGet(DmfModule);
	const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(device);

    *BytesReturned = 0;

	if (!NT_SUCCESS(status = WdfRequestForwardToIoQueue(
		Request,
		pPdoCtx->Queues.HidControlReadRequests
	)))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"WdfRequestForwardToIoQueue failed with status %!STATUS!",
			status
		);
	}
	else status = STATUS_PENDING;

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}

//
// Handles IOCTL_BTHPS3_HID_CONTROL_WRITE
// 
NTSTATUS
BthPS3_PDO_HandleHidControlWrite(
	_In_ DMFMODULE DmfModule,
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ ULONG IoctlCode,
	_In_reads_(InputBufferSize) VOID* InputBuffer,
	_In_ size_t InputBufferSize,
	_Out_writes_(OutputBufferSize) VOID* OutputBuffer,
	_In_ size_t OutputBufferSize,
	_Out_ size_t* BytesReturned
)
{
	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(IoctlCode);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferSize);

	FuncEntry(TRACE_BUSLOGIC);

	NTSTATUS status;
	const WDFDEVICE device = DMF_ParentDeviceGet(DmfModule);
	const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(device);

    *BytesReturned = 0;

	if (!NT_SUCCESS(status = WdfRequestForwardToIoQueue(
		Request,
		pPdoCtx->Queues.HidControlWriteRequests
	)))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"WdfRequestForwardToIoQueue failed with status %!STATUS!",
			status
		);
	}
	else status = STATUS_PENDING;

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}

//
// Handles IOCTL_BTHPS3_HID_INTERRUPT_READ
// 
NTSTATUS
BthPS3_PDO_HandleHidInterruptRead(
	_In_ DMFMODULE DmfModule,
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ ULONG IoctlCode,
	_In_reads_(InputBufferSize) VOID* InputBuffer,
	_In_ size_t InputBufferSize,
	_Out_writes_(OutputBufferSize) VOID* OutputBuffer,
	_In_ size_t OutputBufferSize,
	_Out_ size_t* BytesReturned
)
{
	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(IoctlCode);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferSize);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferSize);

	FuncEntry(TRACE_BUSLOGIC);

    *BytesReturned = 0;

	NTSTATUS status;
	const WDFDEVICE device = DMF_ParentDeviceGet(DmfModule);
	const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(device);

	if (!NT_SUCCESS(status = WdfRequestForwardToIoQueue(
		Request,
		pPdoCtx->Queues.HidInterruptReadRequests
	)))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"WdfRequestForwardToIoQueue failed with status %!STATUS!",
			status
		);
	}
	else status = STATUS_PENDING;

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}

//
// Handles IOCTL_BTHPS3_HID_INTERRUPT_WRITE
// 
NTSTATUS
BthPS3_PDO_HandleHidInterruptWrite(
	_In_ DMFMODULE DmfModule,
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ ULONG IoctlCode,
	_In_reads_(InputBufferSize) VOID* InputBuffer,
	_In_ size_t InputBufferSize,
	_Out_writes_(OutputBufferSize) VOID* OutputBuffer,
	_In_ size_t OutputBufferSize,
	_Out_ size_t* BytesReturned
)
{
	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(IoctlCode);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferSize);

	FuncEntry(TRACE_BUSLOGIC);

	NTSTATUS status;
	const WDFDEVICE device = DMF_ParentDeviceGet(DmfModule);
	const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(device);

    *BytesReturned = 0;

	if (!NT_SUCCESS(status = WdfRequestForwardToIoQueue(
		Request,
		pPdoCtx->Queues.HidInterruptWriteRequests
	)))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"WdfRequestForwardToIoQueue failed with status %!STATUS!",
			status
		);
	}
	else status = STATUS_PENDING;

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}

//
// Handles IOCTL_BTH_DISCONNECT_DEVICE requests
// 
NTSTATUS
BthPS3_PDO_HandleBthDisconnect(
	_In_ DMFMODULE DmfModule,
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ ULONG IoctlCode,
	_In_reads_(InputBufferSize) VOID* InputBuffer,
	_In_ size_t InputBufferSize,
	_Out_writes_(OutputBufferSize) VOID* OutputBuffer,
	_In_ size_t OutputBufferSize,
	_Out_ size_t* BytesReturned
)
{
	FuncEntry(TRACE_BUSLOGIC);

	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(Request);
	UNREFERENCED_PARAMETER(IoctlCode);
	UNREFERENCED_PARAMETER(InputBufferSize);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(OutputBuffer);

	NTSTATUS status;
	const WDFDEVICE device = DMF_ParentDeviceGet(DmfModule);
	const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(device);
	const WDFIOTARGET parentTarget = pPdoCtx->DevCtxHdr->IoTarget;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDFMEMORY payload;
	WDFREQUEST dcRequest = NULL;

    *BytesReturned = 0;

	do
	{
		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = device;

		if (!NT_SUCCESS(status = WdfRequestCreate(
			&attributes,
			parentTarget,
			&dcRequest
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestCreate failed with status %!STATUS!",
				status
			);
			break;
		}

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = dcRequest;

		if (!NT_SUCCESS(status = WdfMemoryCreatePreallocated(
			&attributes,
			/*
			 * ignore the address the upper driver may provide, we only
			 * allow disconnecting the child that received the request.
			 */
			&pPdoCtx->RemoteAddress,
			sizeof(BTH_ADDR),
			&payload
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfMemoryCreatePreallocated failed with status %!STATUS!",
				status
			);
			break;
		}

		if (!NT_SUCCESS(status = WdfIoTargetFormatRequestForIoctl(
			parentTarget,
			dcRequest,
			IOCTL_BTH_DISCONNECT_DEVICE,
			payload,
			NULL,
			NULL,
			NULL
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfIoTargetFormatRequestForIoctl failed with status %!STATUS!",
				status
			);
			break;
		}

		WdfRequestSetCompletionRoutine(
			dcRequest,
			BthPS3_PDO_DisconnectRequestCompleted,
			pPdoCtx
		);

		//
		// Request parent to abandon us :'(
		// 
		if (WdfRequestSend(
			dcRequest,
			parentTarget,
			WDF_NO_SEND_OPTIONS
		) == FALSE)
		{
			status = WdfRequestGetStatus(dcRequest);

			if (status != STATUS_DEVICE_NOT_CONNECTED && !NT_SUCCESS(status))
			{
				TraceError(
					TRACE_BUSLOGIC,
					"WdfRequestSend failed with status %!STATUS!",
					status
				);
			}
			else
			{
				//
				// Override
				// 
				status = STATUS_SUCCESS;
			}
		}


	} while (FALSE);

	if (dcRequest && !NT_SUCCESS(status))
	{
		WdfObjectDelete(dcRequest);
	}

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}


//
// Sends pending HID Control Read Requests through L2CAP channel to remote device
// 
VOID
BthPS3_PDO_DispatchHidControlRead(
	_In_ WDFQUEUE Queue,
	_In_ WDFCONTEXT Context
)
{
	FuncEntry(TRACE_BUSLOGIC);

	const PBTHPS3_PDO_CONTEXT pPdoCtx = Context;
	NTSTATUS status;
	WDFREQUEST request = NULL;
	PVOID buffer = NULL;
	size_t length = 0;

	while (NT_SUCCESS(WdfIoQueueRetrieveNextRequest(Queue, &request)))
	{
		if (!NT_SUCCESS(status = WdfRequestRetrieveOutputBuffer(
			request,
			0,
			&buffer,
			&length
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
				status
			);

			WdfRequestComplete(request, status);
			continue;
		}

		if (!NT_SUCCESS(status = L2CAP_PS3_ReadControlTransferAsync(
			pPdoCtx,
			request,
			buffer,
			length,
			L2CAP_PS3_AsyncReadControlTransferCompleted
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"L2CAP_PS3_ReadControlTransferAsync failed with status %!STATUS!",
				status
			);

			WdfRequestComplete(request, status);
			continue;
		}
	}

	FuncExitNoReturn(TRACE_BUSLOGIC);
}

//
// Sends pending HID Control Write Requests through L2CAP channel to remote device
// 
VOID
BthPS3_PDO_DispatchHidControlWrite(
	_In_ WDFQUEUE Queue,
	_In_ WDFCONTEXT Context
)
{
	FuncEntry(TRACE_BUSLOGIC);

	const PBTHPS3_PDO_CONTEXT pPdoCtx = Context;
	NTSTATUS status;
	WDFREQUEST request = NULL;
	PVOID buffer = NULL;
	size_t length = 0;

	while (NT_SUCCESS(WdfIoQueueRetrieveNextRequest(Queue, &request)))
	{
		if (!NT_SUCCESS(status = WdfRequestRetrieveInputBuffer(
			request,
			0,
			&buffer,
			&length
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
				status
			);

			WdfRequestComplete(request, status);
			continue;
		}

		if (!NT_SUCCESS(status = L2CAP_PS3_SendControlTransferAsync(
			pPdoCtx,
			request,
			buffer,
			length,
			L2CAP_PS3_AsyncSendControlTransferCompleted
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"L2CAP_PS3_SendControlTransferAsync failed with status %!STATUS!",
				status
			);

			WdfRequestComplete(request, status);
			continue;
		}
	}

	FuncExitNoReturn(TRACE_BUSLOGIC);
}

//
// Sends pending HID Interrupt Read Requests through L2CAP channel to remote device
// 
VOID
BthPS3_PDO_DispatchHidInterruptRead(
	_In_ WDFQUEUE Queue,
	_In_ WDFCONTEXT Context
)
{
	FuncEntry(TRACE_BUSLOGIC);

	const PBTHPS3_PDO_CONTEXT pPdoCtx = Context;
	NTSTATUS status;
	WDFREQUEST request = NULL;
	PVOID buffer = NULL;
	size_t length = 0;

	while (NT_SUCCESS(WdfIoQueueRetrieveNextRequest(Queue, &request)))
	{
		if (!NT_SUCCESS(status = WdfRequestRetrieveOutputBuffer(
			request,
			0,
			&buffer,
			&length
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
				status
			);

			WdfRequestComplete(request, status);
			continue;
		}

		if (!NT_SUCCESS(status = L2CAP_PS3_ReadInterruptTransferAsync(
			pPdoCtx,
			request,
			buffer,
			length,
			L2CAP_PS3_AsyncReadInterruptTransferCompleted
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"L2CAP_PS3_ReadInterruptTransferAsync failed with status %!STATUS!",
				status
			);

			WdfRequestComplete(request, status);
			continue;
		}
	}

	FuncExitNoReturn(TRACE_BUSLOGIC);
}

//
// Sends pending HID Interrupt Write Requests through L2CAP channel to remote device
// 
VOID
BthPS3_PDO_DispatchHidInterruptWrite(
	_In_ WDFQUEUE Queue,
	_In_ WDFCONTEXT Context
)
{
	FuncEntry(TRACE_BUSLOGIC);

	const PBTHPS3_PDO_CONTEXT pPdoCtx = Context;
	NTSTATUS status;
	WDFREQUEST request = NULL;
	PVOID buffer = NULL;
	size_t length = 0;

	while (NT_SUCCESS(WdfIoQueueRetrieveNextRequest(Queue, &request)))
	{
		if (!NT_SUCCESS(status = WdfRequestRetrieveInputBuffer(
			request,
			0,
			&buffer,
			&length
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
				status
			);

			WdfRequestComplete(request, status);
			continue;
		}

		if (!NT_SUCCESS(status = L2CAP_PS3_SendInterruptTransferAsync(
			pPdoCtx,
			request,
			buffer,
			length,
			L2CAP_PS3_AsyncSendInterruptTransferCompleted
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"L2CAP_PS3_SendInterruptTransferAsync failed with status %!STATUS!",
				status
			);

			WdfRequestComplete(request, status);
			continue;
		}
	}

	FuncExitNoReturn(TRACE_BUSLOGIC);
}
