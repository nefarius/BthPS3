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
#include "L2CAP.Disconnect.tmh"


_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
L2CAP_PS3_HandleRemoteDisconnect(
	_In_ PBTHPS3_PDO_CONTEXT Context,
	_In_ PINDICATION_PARAMETERS DisconnectParams
)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PBTHPS3_SERVER_CONTEXT pDevCtx = NULL;
	PBTHPS3_PDO_CONTEXT pPdoCtx = Context;
	LARGE_INTEGER timeout;
	timeout.QuadPart = 0;

	FuncEntryArguments(TRACE_L2CAP, "pdoContext=0x%p", DisconnectParams->ConnectionHandle);

	pDevCtx = GetServerDeviceContext(pPdoCtx->DevCtxHdr->Device);

	//
	// HID Control Channel disconnected
	// 
	if (DisconnectParams->ConnectionHandle ==
		pPdoCtx->HidControlChannel.ChannelHandle)
	{
		TraceVerbose(
			TRACE_L2CAP,
			"HID Control Channel 0x%p disconnected",
			DisconnectParams->ConnectionHandle);

		L2CAP_PS3_RemoteDisconnect(
			pPdoCtx->DevCtxHdr,
			pPdoCtx->RemoteAddress,
			&pPdoCtx->HidControlChannel
		);
	}

	//
	// HID Interrupt Channel disconnected
	// 
	if (DisconnectParams->ConnectionHandle ==
		pPdoCtx->HidInterruptChannel.ChannelHandle)
	{
		TraceVerbose(
			TRACE_L2CAP,
			"HID Interrupt Channel 0x%p disconnected",
			DisconnectParams->ConnectionHandle);

		L2CAP_PS3_RemoteDisconnect(
			pPdoCtx->DevCtxHdr,
			pPdoCtx->RemoteAddress,
			&pPdoCtx->HidInterruptChannel
		);
	}

	//
	// Both channels are gone, invoke clean-up
	// 
	if (pPdoCtx->HidControlChannel.ConnectionState == ConnectionStateDisconnected
		&& pPdoCtx->HidInterruptChannel.ConnectionState == ConnectionStateDisconnected)
	{
		TraceVerbose(
			TRACE_L2CAP,
			"Both channels are gone, awaiting clean-up"
		);

		status = KeWaitForSingleObject(
			&pPdoCtx->HidControlChannel.DisconnectEvent,
			Executive,
			KernelMode,
			FALSE,
			&timeout
		);
		if (!NT_SUCCESS(status))
		{
			TraceError(
				TRACE_L2CAP,
				"HID Control - KeWaitForSingleObject failed with status %!STATUS!",
				status
			);

			//
			// TODO: react properly to this!
			// 
		}

		status = KeWaitForSingleObject(
			&pPdoCtx->HidInterruptChannel.DisconnectEvent,
			Executive,
			KernelMode,
			FALSE,
			&timeout
		);
		if (!NT_SUCCESS(status))
		{
			TraceError(
				TRACE_L2CAP,
				"HID Interrupt - KeWaitForSingleObject failed with status %!STATUS!",
				status
			);

			//
			// TODO: react properly to this!
			// 
		}

		//
		// TODO: old PDO destruction happened here
		// 

		BthPS3_PDO_Destroy(&pDevCtx->Header, pPdoCtx);
	}

	FuncExit(TRACE_L2CAP, "status=%!STATUS!", status);

	return status;
}

//
// Gets invoked on remote disconnect or configuration request
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
void
L2CAP_PS3_ConnectionIndicationCallback(
	_In_ PVOID Context,
	_In_ INDICATION_CODE Indication,
	_In_ PINDICATION_PARAMETERS Parameters
)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PBTHPS3_PDO_CONTEXT pPdoCtx = Context;

	FuncEntryArguments(TRACE_L2CAP, "Indication=0x%X, Context=0x%p",
		Indication, Context);

	switch (Indication)
	{
	case IndicationAddReference:
		TraceVerbose(TRACE_L2CAP, "IndicationAddReference");
		break;
	case IndicationReleaseReference:
		TraceVerbose(TRACE_L2CAP, "IndicationReleaseReference");
		break;
	case IndicationRemoteConnect:
	{
		//
		// We don't expect connect on this callback
		//
		NT_ASSERT(FALSE);
		break;
	}
	case IndicationRemoteDisconnect:

		TraceVerbose(
			TRACE_L2CAP,
			"IndicationRemoteDisconnect [0x%p]",
			Parameters->ConnectionHandle);

		if (KeGetCurrentIrql() <= PASSIVE_LEVEL)
		{
			L2CAP_PS3_HandleRemoteDisconnect(pPdoCtx, Parameters);

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
		qwi.Context.Pdo = pPdoCtx;

		if (!NT_SUCCESS(status = DMF_QueuedWorkItem_Enqueue(
			pPdoCtx->DevCtxHdr->QueuedWorkItemModule,
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

	case IndicationRemoteConfigRequest:

		TraceVerbose(TRACE_L2CAP, "IndicationRemoteConfigRequest");

		//
		// This catches QOS configuration request and inherently succeeds it
		// 

		break;

	case IndicationRemoteConfigResponse:

		TraceVerbose(TRACE_L2CAP, "IndicationRemoteConfigResponse");

		break;

	case IndicationFreeExtraOptions:
		break;
	default:
		//
		// We don't expect any other indications on this callback
		//
		NT_ASSERT(FALSE);
	}

	FuncExitNoReturn(TRACE_L2CAP);
}

//
// Instructs a channel to disconnect
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
L2CAP_PS3_RemoteDisconnect(
	_In_ PBTHPS3_DEVICE_CONTEXT_HEADER CtxHdr,
	_In_ BTH_ADDR RemoteAddress,
	_In_ PBTHPS3_CLIENT_L2CAP_CHANNEL Channel
)
{
	struct _BRB_L2CA_CLOSE_CHANNEL* disconnectBrb = NULL;

	FuncEntry(TRACE_L2CAP);

	WdfSpinLockAcquire(Channel->ConnectionStateLock);

	if (Channel->ConnectionState == ConnectionStateConnecting)
	{
		//
		// If the connection is not completed yet set the state 
		// to disconnecting.
		// In such case we should send CLOSE_CHANNEL Brb down after 
		// we receive connect completion.
		//

		Channel->ConnectionState = ConnectionStateDisconnecting;

		//
		// Clear event to indicate that we are in disconnecting
		// state. It will be set when disconnect is completed
		//
		KeClearEvent(&Channel->DisconnectEvent);

		WdfSpinLockRelease(Channel->ConnectionStateLock);
		return TRUE;
	}

	if (Channel->ConnectionState != ConnectionStateConnected)
	{
		//
		// Do nothing if we are not connected
		//

		WdfSpinLockRelease(Channel->ConnectionStateLock);
		FuncExit(TRACE_L2CAP, "returns=FALSE");
		return FALSE;
	}

	Channel->ConnectionState = ConnectionStateDisconnecting;
	WdfSpinLockRelease(Channel->ConnectionStateLock);

	//
	// We are now sending the disconnect, so clear the event.
	//

	KeClearEvent(&Channel->DisconnectEvent);

	CLIENT_CONNECTION_REQUEST_REUSE(Channel->ConnectDisconnectRequest);
	CtxHdr->ProfileDrvInterface.BthReuseBrb(
		&Channel->ConnectDisconnectBrb,
		BRB_L2CA_CLOSE_CHANNEL
	);

	disconnectBrb = (struct _BRB_L2CA_CLOSE_CHANNEL*)&(Channel->ConnectDisconnectBrb);

	disconnectBrb->BtAddress = RemoteAddress;
	disconnectBrb->ChannelHandle = Channel->ChannelHandle;

	//
	// The BRB can fail with STATUS_DEVICE_DISCONNECT if the device is already
	// disconnected, hence we don't assert for success
	//
	(void)BthPS3_SendBrbAsync(
		CtxHdr->IoTarget,
		Channel->ConnectDisconnectRequest,
		(PBRB)disconnectBrb,
		sizeof(*disconnectBrb),
		L2CAP_PS3_ChannelDisconnectCompleted,
		Channel
	);

	FuncExit(TRACE_L2CAP, "returns=TRUE");

	return TRUE;
}

//
// Gets called once a channel disconnect request has been completed
// 
void
L2CAP_PS3_ChannelDisconnectCompleted(
	_In_ WDFREQUEST Request,
	_In_ WDFIOTARGET Target,
	_In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
	_In_ WDFCONTEXT Context
)
{
	PBTHPS3_CLIENT_L2CAP_CHANNEL channel = (PBTHPS3_CLIENT_L2CAP_CHANNEL)Context;

	UNREFERENCED_PARAMETER(Request);
	UNREFERENCED_PARAMETER(Target);

	FuncEntryArguments(TRACE_L2CAP, "status=%!STATUS!", Params->IoStatus.Status);

	WdfSpinLockAcquire(channel->ConnectionStateLock);
	channel->ConnectionState = ConnectionStateDisconnected;
	WdfSpinLockRelease(channel->ConnectionStateLock);

	//
	// Disconnect complete, set the event
	//
	KeSetEvent(
		&channel->DisconnectEvent,
		0,
		FALSE
	);

	FuncExitNoReturn(TRACE_L2CAP);
}
