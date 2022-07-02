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
#include "Bluetooth.L2CAP.tmh"


#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3_UnregisterL2CAPServer)
#endif


_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RegisterL2CAPServer(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
	NTSTATUS status;
	struct _BRB_L2CA_REGISTER_SERVER* brb;

	FuncEntry(TRACE_BTH);

	DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
		&(DevCtx->RegisterUnregisterBrb),
		BRB_L2CA_REGISTER_SERVER
	);

	brb = (struct _BRB_L2CA_REGISTER_SERVER*)
		&(DevCtx->RegisterUnregisterBrb);

	//
	// Format brb
	//
	brb->BtAddress = BTH_ADDR_NULL;
	brb->PSM = 0; //we have already registered the PSMs
	brb->IndicationCallback = &BthPS3_IndicationCallback;
	brb->IndicationCallbackContext = DevCtx;
	brb->IndicationFlags = 0;
	brb->ReferenceObject = WdfDeviceWdmGetDeviceObject(DevCtx->Header.Device);

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
	}
	else
	{
		//
		// Store server handle
		//
		DevCtx->L2CAPServerHandle = brb->ServerHandle;
	}

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3_UnregisterL2CAPServer(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
	NTSTATUS status;
	struct _BRB_L2CA_UNREGISTER_SERVER* brb;

	FuncEntry(TRACE_BTH);

	PAGED_CODE();

	if (NULL == DevCtx->L2CAPServerHandle)
	{
		return;
	}

	DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
		&(DevCtx->RegisterUnregisterBrb),
		BRB_L2CA_UNREGISTER_SERVER
	);

	brb = (struct _BRB_L2CA_UNREGISTER_SERVER*)
		&(DevCtx->RegisterUnregisterBrb);

	//
	// Format Brb
	//
	brb->BtAddress = BTH_ADDR_NULL;//DevCtx->LocalAddress;
	brb->Psm = 0; //since we will use unregister PSM to unregister.
	brb->ServerHandle = DevCtx->L2CAPServerHandle;

	if (!NT_SUCCESS(status = BthPS3_SendBrbSynchronously(
		DevCtx->Header.IoTarget,
		DevCtx->Header.HostInitRequest,
		(PBRB)brb,
		sizeof(*(brb))
	)))
	{
		TraceError(
			TRACE_BTH,
			"BRB_L2CA_UNREGISTER_SERVER failed with status %!STATUS!",
			status
		);
	}
	else
	{
		DevCtx->L2CAPServerHandle = NULL;
	}

	FuncExitNoReturn(TRACE_BTH);
}
