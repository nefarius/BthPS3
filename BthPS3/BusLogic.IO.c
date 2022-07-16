/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2022, Nefarius Software Solutions e.U.                      *
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
_IRQL_requires_max_(PASSIVE_LEVEL)
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
	UNREFERENCED_PARAMETER(BytesReturned);
	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(OutputBuffer);

	NTSTATUS status;
	const WDFDEVICE device = DMF_ParentDeviceGet(DmfModule);
	const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(device);

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

	/*
	if (!NT_SUCCESS(status = L2CAP_PS3_ReadControlTransferAsync(
		pPdoCtx,
		Request,
		OutputBuffer,
		OutputBufferSize,
		L2CAP_PS3_AsyncReadControlTransferCompleted
	)))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"L2CAP_PS3_ReadControlTransferAsync failed with status %!STATUS!",
			status
		);
	}
	else
	{
		status = STATUS_PENDING;
	}
	*/

	return status;
}

//
// Handles IOCTL_BTHPS3_HID_CONTROL_WRITE
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
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
	UNREFERENCED_PARAMETER(BytesReturned);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferSize);

	NTSTATUS status;
	const WDFDEVICE device = DMF_ParentDeviceGet(DmfModule);
	const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(device);

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

	/*if (!NT_SUCCESS(status = L2CAP_PS3_SendControlTransferAsync(
		pPdoCtx,
		Request,
		InputBuffer,
		InputBufferSize,
		L2CAP_PS3_AsyncReadControlTransferCompleted
	)))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"L2CAP_PS3_SendControlTransferAsync failed with status %!STATUS!",
			status
		);
	}
	else
	{
		status = STATUS_PENDING;
	}*/

	return status;
}

//
// Handles IOCTL_BTHPS3_HID_INTERRUPT_READ
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
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
	UNREFERENCED_PARAMETER(BytesReturned);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferSize);

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

	/*if (!NT_SUCCESS(status = L2CAP_PS3_ReadInterruptTransferAsync(
		pPdoCtx,
		Request,
		OutputBuffer,
		OutputBufferSize,
		L2CAP_PS3_AsyncReadControlTransferCompleted
	)))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"L2CAP_PS3_ReadInterruptTransferAsync failed with status %!STATUS!",
			status
		);
	}
	else
	{
		status = STATUS_PENDING;
	}*/

	return status;
}

//
// Handles IOCTL_BTHPS3_HID_INTERRUPT_WRITE
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
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
	UNREFERENCED_PARAMETER(BytesReturned);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferSize);

	NTSTATUS status;
	const WDFDEVICE device = DMF_ParentDeviceGet(DmfModule);
	const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(device);

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

	/*if (!NT_SUCCESS(status = L2CAP_PS3_SendInterruptTransferAsync(
		pPdoCtx,
		Request,
		InputBuffer,
		InputBufferSize,
		L2CAP_PS3_AsyncReadControlTransferCompleted
	)))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"L2CAP_PS3_SendInterruptTransferAsync failed with status %!STATUS!",
			status
		);
	}
	else
	{
		status = STATUS_PENDING;
	}*/

	return status;
}

//
// Handles all other requests
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_PDO_HandleOther(
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
	UNREFERENCED_PARAMETER(Request);
	UNREFERENCED_PARAMETER(IoctlCode);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferSize);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(BytesReturned);

	NTSTATUS status;
	const WDFDEVICE device = DMF_ParentDeviceGet(DmfModule);
	const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(device);

	UNREFERENCED_PARAMETER(pPdoCtx);

	TraceVerbose(
		TRACE_BUSLOGIC,
		"Received Request with code: 0x%X",
		IoctlCode
	);

	status = STATUS_UNSUCCESSFUL;

	return status;
}

