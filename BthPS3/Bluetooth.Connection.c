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
#include "Bluetooth.Connection.tmh"
#include "BthPS3ETW.h"


 //
 // Gets invoked by parent bus if there's work for our driver
 // 
_IRQL_requires_max_(DISPATCH_LEVEL)
void
BthPS3_IndicationCallback(
	_In_ PVOID Context,
	_In_ INDICATION_CODE Indication,
	_In_ PINDICATION_PARAMETERS Parameters
)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	FuncEntry(TRACE_BTH);

	switch (Indication)
	{
	case IndicationAddReference:
		TraceVerbose(TRACE_BTH, "IndicationAddReference");
		break;
	case IndicationReleaseReference:
		TraceVerbose(TRACE_BTH, "IndicationReleaseReference");
		break;
	case IndicationRemoteConnect:
	{
		const PBTHPS3_SERVER_CONTEXT devCtx = (PBTHPS3_SERVER_CONTEXT)Context;

		TraceInformation(
			TRACE_BTH,
			"New connection for PSM 0x%04X from %012llX arrived",
			Parameters->Parameters.Connect.Request.PSM,
			Parameters->BtAddress
		);

		if (KeGetCurrentIrql() <= PASSIVE_LEVEL)
		{
			//
			// Main entry point for a new connection, decides if valid etc.
			// 
			L2CAP_PS3_HandleRemoteConnect(devCtx, Parameters);

			break;
		}

		//
		// Can be DPC level, enqueue work item
		// 

		TraceVerbose(
			TRACE_BTH,
			"IRQL %!irql! too high, preparing async call",
			KeGetCurrentIrql()
		);

		BTHPS3_QWI_CONTEXT qwi;
		qwi.IndicationCode = Indication;
		qwi.IndicationParameters = *Parameters;
		qwi.Context.Server = devCtx;

		if (!NT_SUCCESS(status = DMF_QueuedWorkItem_Enqueue(
			devCtx->Header.QueuedWorkItemModule,
			&qwi,
			sizeof(BTHPS3_QWI_CONTEXT)
		)))
		{
			TraceError(
				TRACE_BTH,
				"DMF_QueuedWorkItem_Enqueue failed with status %!STATUS!",
				status
			);

			break;
		}

		break;
	}
	case IndicationRemoteDisconnect:
	{
		//
		// We register L2CAP_PS3_ConnectionIndicationCallback for disconnect
		//
		NT_ASSERT(FALSE);
		break;
	}
	case IndicationRemoteConfigRequest:
	case IndicationRemoteConfigResponse:
	case IndicationFreeExtraOptions:
		break;
	}

	FuncExitNoReturn(TRACE_BTH);
}
