/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2026, Nefarius Software Solutions e.U.                      *
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


_IRQL_requires_max_(DISPATCH_LEVEL)
static
VOID
L2CAP_PS3_CompleteChannelDisconnect(
	_In_ PBTHPS3_CLIENT_L2CAP_CHANNEL Channel
)
{
	WdfSpinLockAcquire(Channel->ConnectionStateLock);
	Channel->ConnectionState = ConnectionStateDisconnected;
	WdfSpinLockRelease(Channel->ConnectionStateLock);

	KeSetEvent(
		&Channel->DisconnectEvent,
		0,
		FALSE
	);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
BOOLEAN
L2CAP_PS3_SendDisconnectBrb(
	_In_ PBTHPS3_DEVICE_CONTEXT_HEADER CtxHdr,
	_In_ BTH_ADDR RemoteAddress,
	_In_ PBTHPS3_CLIENT_L2CAP_CHANNEL Channel
)
{
	struct _BRB_L2CA_CLOSE_CHANNEL* disconnectBrb = NULL;
	NTSTATUS status;

	if (!NT_SUCCESS(status = BthPS3_PDO_RundownAcquire(Channel->PdoContext)))
	{
		TraceError(
			TRACE_L2CAP,
			"Cannot send CLOSE_CHANNEL, PDO rundown unavailable (%!STATUS!)",
			status
		);
		L2CAP_PS3_CompleteChannelDisconnect(Channel);
		return FALSE;
	}

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
	status = BthPS3_SendBrbAsync(
		CtxHdr->IoTarget,
		Channel->ConnectDisconnectRequest,
		(PBRB)disconnectBrb,
		sizeof(*disconnectBrb),
		L2CAP_PS3_ChannelDisconnectCompleted,
		Channel
	);

	if (!NT_SUCCESS(status))
	{
		TraceError(
			TRACE_L2CAP,
			"BthPS3_SendBrbAsync (CLOSE_CHANNEL) failed with status %!STATUS!",
			status
		);
		L2CAP_PS3_CompleteChannelDisconnect(Channel);
		BthPS3_PDO_RundownRelease(Channel->PdoContext);
		return FALSE;
	}

	return TRUE;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_HandleRemoteDisconnect(
	_In_ PBTHPS3_PDO_CONTEXT Context,
	_In_ PINDICATION_PARAMETERS DisconnectParams
)
{
	NTSTATUS status = STATUS_SUCCESS;
	PBTHPS3_PDO_CONTEXT pPdoCtx = Context;

	FuncEntryArguments(TRACE_L2CAP, "pdoContext=0x%p", DisconnectParams->ConnectionHandle);

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

	BthPS3_PDO_Destroy(pPdoCtx->DevCtxHdr, pPdoCtx);

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
	PBTHPS3_PDO_CONTEXT pPdoCtx = Context;
	NTSTATUS status;

	FuncEntryArguments(TRACE_L2CAP, "Indication=0x%X, Context=0x%p",
		Indication, Context);

	if (!NT_SUCCESS(status = BthPS3_PDO_RundownAcquire(pPdoCtx)))
	{
		TraceVerbose(
			TRACE_L2CAP,
			"Ignoring indication 0x%X, PDO rundown unavailable (%!STATUS!)",
			Indication,
			status
		);
		FuncExitNoReturn(TRACE_L2CAP);
		return;
	}

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

		L2CAP_PS3_HandleRemoteDisconnect(pPdoCtx, Parameters);

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

	BthPS3_PDO_RundownRelease(pPdoCtx);

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
	BOOLEAN submitted = FALSE;

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
		FuncExit(TRACE_L2CAP, "returns=TRUE");
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

	submitted = L2CAP_PS3_SendDisconnectBrb(
		CtxHdr,
		RemoteAddress,
		Channel
	);

	FuncExit(TRACE_L2CAP, "returns=%d", submitted);

	return submitted;
}

//
// Apply an OPEN_CHANNEL completion to channel + PDO lifecycle.
// Returns TRUE if the channel became Connected and I/O may be armed.
//
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
L2CAP_PS3_ApplyConnectCompletion(
	_In_ PBTHPS3_PDO_CONTEXT PdoContext,
	_In_ PBTHPS3_CLIENT_L2CAP_CHANNEL Channel,
	_In_ NTSTATUS Status
)
{
	BOOLEAN enableIo = FALSE;
	BOOLEAN sendClose = FALSE;

	WdfSpinLockAcquire(Channel->ConnectionStateLock);

	if (!NT_SUCCESS(Status))
	{
		Channel->ConnectionState = ConnectionStateConnectFailed;
		KeSetEvent(&Channel->DisconnectEvent, 0, FALSE);
		WdfSpinLockRelease(Channel->ConnectionStateLock);

		BthPS3_PDO_Destroy(PdoContext->DevCtxHdr, PdoContext);
		return FALSE;
	}

	if (Channel->ConnectionState == ConnectionStateDisconnecting ||
		PdoContext->Lifecycle != BthPS3PdoLifecycleActive)
	{
		if (Channel->ConnectionState != ConnectionStateDisconnecting)
		{
			Channel->ConnectionState = ConnectionStateDisconnecting;
			KeClearEvent(&Channel->DisconnectEvent);
		}

		sendClose = TRUE;
		WdfSpinLockRelease(Channel->ConnectionStateLock);
	}
	else
	{
		Channel->ConnectionState = ConnectionStateConnected;
		KeClearEvent(&Channel->DisconnectEvent);
		enableIo = (PdoContext->Lifecycle == BthPS3PdoLifecycleActive);
		WdfSpinLockRelease(Channel->ConnectionStateLock);
	}

	if (sendClose)
	{
		L2CAP_PS3_SendDisconnectBrb(
			PdoContext->DevCtxHdr,
			PdoContext->RemoteAddress,
			Channel
		);
		BthPS3_PDO_Destroy(PdoContext->DevCtxHdr, PdoContext);
	}

	return enableIo;
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
	PBTHPS3_PDO_CONTEXT pPdoCtx = channel->PdoContext;

	UNREFERENCED_PARAMETER(Request);
	UNREFERENCED_PARAMETER(Target);

	FuncEntryArguments(TRACE_L2CAP, "status=%!STATUS!", Params->IoStatus.Status);

	L2CAP_PS3_CompleteChannelDisconnect(channel);

	if (pPdoCtx != NULL)
	{
		BthPS3_PDO_Destroy(pPdoCtx->DevCtxHdr, pPdoCtx);
		BthPS3_PDO_RundownRelease(pPdoCtx);
	}

	FuncExitNoReturn(TRACE_L2CAP);
}
