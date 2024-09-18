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
#include "Bluetooth.Request.tmh"


_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_SendBrbSynchronously(
	_In_ WDFIOTARGET IoTarget,
	_In_ WDFREQUEST Request,
	_In_ PBRB Brb,
	_In_ ULONG BrbSize
)
{
	NTSTATUS status;
	WDF_REQUEST_REUSE_PARAMS reuseParams;
	WDF_MEMORY_DESCRIPTOR OtherArg1Desc;

	FuncEntry(TRACE_BTH);

	WDF_REQUEST_REUSE_PARAMS_INIT(
		&reuseParams,
		WDF_REQUEST_REUSE_NO_FLAGS,
		STATUS_NOT_SUPPORTED
	);

	if (!NT_SUCCESS(status = WdfRequestReuse(Request, &reuseParams)))
	{
		return status;
	}

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
		&OtherArg1Desc,
		Brb,
		BrbSize
	);

	status = WdfIoTargetSendInternalIoctlOthersSynchronously(
		IoTarget,
		Request,
		IOCTL_INTERNAL_BTH_SUBMIT_BRB,
		&OtherArg1Desc,
		NULL, //OtherArg2
		NULL, //OtherArg4
		NULL, //RequestOptions
		NULL  //BytesReturned
	);

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3_SendBrbAsync(
	_In_ WDFIOTARGET IoTarget,
	_In_ WDFREQUEST Request,
	_In_ PBRB Brb,
	_In_ size_t BrbSize,
	_In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE ComplRoutine,
	_In_opt_ WDFCONTEXT Context
)
{
	NTSTATUS status = BTH_ERROR_SUCCESS;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDFMEMORY memoryArg1 = NULL;

	if (BrbSize <= 0)
	{
		TraceError(
			TRACE_BTH,
			"BrbSize has invalid value: %I64d\n",
			BrbSize
		);

		status = STATUS_INVALID_PARAMETER;

		return status;
	}

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = Request;

	if (!NT_SUCCESS(status = WdfMemoryCreatePreallocated(
		&attributes,
		Brb,
		BrbSize,
		&memoryArg1
	)))
	{
		TraceError(TRACE_BTH,
			"Creating preallocated memory for Brb 0x%p failed, Request to be formatted 0x%p, "
			"Status code %!STATUS!",
			Brb,
			Request,
			status
		);

		return status;
	}

	if (!NT_SUCCESS(status = WdfIoTargetFormatRequestForInternalIoctlOthers(
		IoTarget,
		Request,
		IOCTL_INTERNAL_BTH_SUBMIT_BRB,
		memoryArg1,
		NULL, //OtherArg1Offset
		NULL, //OtherArg2
		NULL, //OtherArg2Offset
		NULL, //OtherArg4
		NULL  //OtherArg4Offset
	)))
	{
		TraceError(
			TRACE_BTH,
			"Formatting request 0x%p with Brb 0x%p failed, Status code %!STATUS!",
			Request,
			Brb,
			status
		);

		return status;
	}

	//
	// Set a CompletionRoutine callback function.
	//
	WdfRequestSetCompletionRoutine(
		Request,
		ComplRoutine,
		Context
	);

	if (FALSE == WdfRequestSend(
		Request,
		IoTarget,
		NULL
	))
	{
		status = WdfRequestGetStatus(Request);

		TraceError(
			TRACE_BTH,
			"Request send failed for request 0x%p, Brb 0x%p, Status code %!STATUS!",
			Request,
			Brb,
			status
		);

		return status;
	}

	return status;
}
