/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
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


#include "Driver.h"
#include "Bluetooth.PSM.tmh"


#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3_UnregisterPSM)
#endif


_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RegisterPSM(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
	NTSTATUS status;
	struct _BRB_PSM* brb;

	FuncEntry(TRACE_BTH);

	DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
		&(DevCtx->RegisterUnregisterBrb),
		BRB_REGISTER_PSM
	);

	do 
	{
		brb = (struct _BRB_PSM*)
			&(DevCtx->RegisterUnregisterBrb);

		//
		// Register PSM_DS3_HID_CONTROL
		// 

		brb->Psm = PSM_DS3_HID_CONTROL;

		TraceInformation(
			TRACE_BTH,
			"Trying to register PSM 0x%04X",
			brb->Psm
		);

		if (!NT_SUCCESS(status = BthPS3_SendBrbSynchronously(
			DevCtx->Header.IoTarget,
			DevCtx->Header.HostInitRequest,
			(PBRB)brb,
			sizeof(*brb)
		)))
		{
			TraceError(
				TRACE_BTH,
				"BRB_REGISTER_PSM failed with status %!STATUS!",
				status
			);
			break;
		}

		//
		// Store PSM obtained
		//
		DevCtx->PsmHidControl = brb->Psm;

		TraceInformation(
			TRACE_BTH,
			"Got PSM 0x%04X",
			brb->Psm
		);

		// 
		// Shouldn't happen but validate anyway
		// 
		if (brb->Psm != PSM_DS3_HID_CONTROL)
		{
			TraceError(
				TRACE_BTH,
				"Requested PSM 0x%04X but got 0x%04X instead",
				PSM_DS3_HID_CONTROL,
				brb->Psm
			);

			status = STATUS_INVALID_PARAMETER_1;
			break;
		}

		//
		// Register PSM_DS3_HID_CONTROL
		// 

		brb->Psm = PSM_DS3_HID_INTERRUPT;

		TraceInformation(
			TRACE_BTH,
			"Trying to register PSM 0x%04X",
			brb->Psm
		);

		if (!NT_SUCCESS(status = BthPS3_SendBrbSynchronously(
			DevCtx->Header.IoTarget,
			DevCtx->Header.HostInitRequest,
			(PBRB)brb,
			sizeof(*brb)
		)))
		{
			TraceError(
				TRACE_BTH,
				"BRB_REGISTER_PSM failed with status %!STATUS!",
				status
			);
			break;
		}

		//
		// Store PSM obtained
		//
		DevCtx->PsmHidInterrupt = brb->Psm;

		TraceInformation(
			TRACE_BTH,
			"Got PSM 0x%04X",
			brb->Psm
		);

		// 
		// Shouldn't happen but validate anyway
		// 
		if (brb->Psm != PSM_DS3_HID_INTERRUPT)
		{
			TraceError(
				TRACE_BTH,
				"Requested PSM 0x%04X but got 0x%04X instead",
				PSM_DS3_HID_INTERRUPT,
				brb->Psm
			);

			status = STATUS_INVALID_PARAMETER_2;
		}

	} while (FALSE);

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3_UnregisterPSM(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
	NTSTATUS status;
	struct _BRB_PSM* brb;

	PAGED_CODE();

	FuncEntry(TRACE_BTH);

	DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
		&(DevCtx->RegisterUnregisterBrb),
		BRB_UNREGISTER_PSM
	);

	brb = (struct _BRB_PSM*)
		&(DevCtx->RegisterUnregisterBrb);

	brb->Psm = DevCtx->PsmHidControl;

	if (!NT_SUCCESS(status = BthPS3_SendBrbSynchronously(
		DevCtx->Header.IoTarget,
		DevCtx->Header.HostInitRequest,
		(PBRB)brb,
		sizeof(*(brb))
	)))
	{
		TraceError(
			TRACE_BTH,
			"BRB_UNREGISTER_PSM failed with status %!STATUS!",
			status
		);
	}

	DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
		&(DevCtx->RegisterUnregisterBrb),
		BRB_UNREGISTER_PSM
	);

	brb = (struct _BRB_PSM*)
		&(DevCtx->RegisterUnregisterBrb);

	brb->Psm = DevCtx->PsmHidInterrupt;

	if (!NT_SUCCESS(status = BthPS3_SendBrbSynchronously(
		DevCtx->Header.IoTarget,
		DevCtx->Header.HostInitRequest,
		(PBRB)brb,
		sizeof(*(brb))
	)))
	{
		TraceError(
			TRACE_BTH,
			"BRB_UNREGISTER_PSM failed with status %!STATUS!",
			status
		);
	}

	FuncExitNoReturn(TRACE_BTH);
}
