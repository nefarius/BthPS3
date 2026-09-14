/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2025, Nefarius Software Solutions e.U.                      *
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
#include "BthPS3ETW.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3_UnregisterPSM)
#endif

_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
BthPS3_RegisterSinglePSM(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx,
	_In_ USHORT DesiredPsm,
	_Out_ PUSHORT ObtainedPsm
);

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
BthPS3_UnregisterSinglePSM(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx,
	_In_ USHORT Psm
);


_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RegisterPSM(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
	NTSTATUS status = STATUS_SUCCESS;

	FuncEntry(TRACE_BTH);

	do
	{
		if (DevCtx->PsmHidControlOwned)
		{
			TraceInformation(
				TRACE_BTH,
				"PSM 0x%04X already owned, skipping registration",
				PSM_DS3_HID_CONTROL
			);
		}
		else
		{
			status = BthPS3_RegisterSinglePSM(
				DevCtx,
				PSM_DS3_HID_CONTROL,
				&DevCtx->PsmHidControl
			);

			if (0 != DevCtx->PsmHidControl)
			{
				DevCtx->PsmHidControlOwned = TRUE;
			}

			if (!NT_SUCCESS(status))
			{
				break;
			}
		}

		if (DevCtx->PsmHidInterruptOwned)
		{
			TraceInformation(
				TRACE_BTH,
				"PSM 0x%04X already owned, skipping registration",
				PSM_DS3_HID_INTERRUPT
			);
		}
		else
		{
			status = BthPS3_RegisterSinglePSM(
				DevCtx,
				PSM_DS3_HID_INTERRUPT,
				&DevCtx->PsmHidInterrupt
			);

			if (0 != DevCtx->PsmHidInterrupt)
			{
				DevCtx->PsmHidInterruptOwned = TRUE;
			}

			if (!NT_SUCCESS(status))
			{
				break;
			}
		}

	} while (FALSE);

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
BthPS3_RegisterSinglePSM(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx,
	_In_ USHORT DesiredPsm,
	_Out_ PUSHORT ObtainedPsm
)
{
	NTSTATUS status;
	struct _BRB_PSM* brb;
	ULONG attempt;

	FuncEntry(TRACE_BTH);

	*ObtainedPsm = 0;

	for (attempt = 0; attempt < 2; attempt++)
	{
		DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
			&(DevCtx->RegisterUnregisterBrb),
			BRB_REGISTER_PSM
		);

		brb = (struct _BRB_PSM*)
			&(DevCtx->RegisterUnregisterBrb);

		brb->Psm = DesiredPsm;

		TraceInformation(
			TRACE_BTH,
			"Trying to register PSM 0x%04X",
			brb->Psm
		);

		status = BthPS3_SendBrbSynchronously(
			DevCtx->Header.IoTarget,
			DevCtx->Header.HostInitRequest,
			(PBRB)brb,
			sizeof(*brb)
		);

		if (NT_SUCCESS(status))
		{
			*ObtainedPsm = brb->Psm;

			TraceInformation(
				TRACE_BTH,
				"Got PSM 0x%04X",
				brb->Psm
			);

			if (brb->Psm != DesiredPsm)
			{
				TraceError(
					TRACE_BTH,
					"Requested PSM 0x%04X but got 0x%04X instead",
					DesiredPsm,
					brb->Psm
				);

				status = STATUS_INVALID_PARAMETER;
			}

			break;
		}

		if (status != STATUS_ALREADY_COMMITTED || attempt > 0)
		{
			TraceError(
				TRACE_BTH,
				"BRB_REGISTER_PSM failed with status %!STATUS!",
				status
			);
			break;
		}

		TraceWarning(
			TRACE_BTH,
			"PSM 0x%04X already committed, attempting reclaim",
			DesiredPsm
		);

		EventWritePsmRegistrationStale(NULL, DesiredPsm, status);

		DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
			&(DevCtx->RegisterUnregisterBrb),
			BRB_UNREGISTER_PSM
		);

		brb = (struct _BRB_PSM*)
			&(DevCtx->RegisterUnregisterBrb);

		brb->Psm = DesiredPsm;

		if (!NT_SUCCESS(status = BthPS3_SendBrbSynchronously(
			DevCtx->Header.IoTarget,
			DevCtx->Header.HostInitRequest,
			(PBRB)brb,
			sizeof(*brb)
		)))
		{
			TraceWarning(
				TRACE_BTH,
				"BRB_UNREGISTER_PSM reclaim of 0x%04X failed with status %!STATUS!",
				DesiredPsm,
				status
			);

			status = STATUS_ALREADY_COMMITTED;
			break;
		}

		TraceInformation(
			TRACE_BTH,
			"Reclaimed stale PSM 0x%04X, retrying registration",
			DesiredPsm
		);
	}

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3_UnregisterPSM(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
	PAGED_CODE();

	FuncEntry(TRACE_BTH);

	if (DevCtx->PsmHidControlOwned)
	{
		BthPS3_UnregisterSinglePSM(DevCtx, DevCtx->PsmHidControl);
		DevCtx->PsmHidControlOwned = FALSE;
		DevCtx->PsmHidControl = 0;
	}

	if (DevCtx->PsmHidInterruptOwned)
	{
		BthPS3_UnregisterSinglePSM(DevCtx, DevCtx->PsmHidInterrupt);
		DevCtx->PsmHidInterruptOwned = FALSE;
		DevCtx->PsmHidInterrupt = 0;
	}

	FuncExitNoReturn(TRACE_BTH);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
BthPS3_UnregisterSinglePSM(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx,
	_In_ USHORT Psm
)
{
	NTSTATUS status;
	struct _BRB_PSM* brb;

	DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
		&(DevCtx->RegisterUnregisterBrb),
		BRB_UNREGISTER_PSM
	);

	brb = (struct _BRB_PSM*)
		&(DevCtx->RegisterUnregisterBrb);

	brb->Psm = Psm;

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
}
