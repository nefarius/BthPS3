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
#include "L2CAP.tmh"
#include "BthPS3ETW.h"


#pragma region L2CAP remote connection handling

//
// Control channel connection result
// 
void
L2CAP_PS3_ControlConnectResponseCompleted(
	_In_ WDFREQUEST Request,
	_In_ WDFIOTARGET Target,
	_In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
	_In_ WDFCONTEXT Context
)
{
	NTSTATUS status;
	struct _BRB_L2CA_OPEN_CHANNEL* brb = NULL;
	PBTHPS3_PDO_CONTEXT pPdoCtx = NULL;

	UNREFERENCED_PARAMETER(Request);
	UNREFERENCED_PARAMETER(Target);

	status = Params->IoStatus.Status;

	FuncEntryArguments(TRACE_L2CAP, "status=%!STATUS!", status);


	brb = (struct _BRB_L2CA_OPEN_CHANNEL*)Context;
	pPdoCtx = brb->Hdr.ClientContext[0];

	//
	// Connection acceptance successful, channel ready to operate
	// 
	if (NT_SUCCESS(status))
	{
		WdfSpinLockAcquire(pPdoCtx->HidControlChannel.ConnectionStateLock);

		pPdoCtx->HidControlChannel.ConnectionState = ConnectionStateConnected;

		//
		// This will be set again once disconnect has occurred
		// 
		KeClearEvent(&pPdoCtx->HidControlChannel.DisconnectEvent);

		WdfSpinLockRelease(pPdoCtx->HidControlChannel.ConnectionStateLock);

		TraceInformation(
			TRACE_L2CAP,
			"HID Control Channel connection established"
		);

		EventWriteHidControlChannelConnected(NULL);

		//
		// Channel connected, queues ready to start processing
		// 

		if (!NT_SUCCESS(status = WdfIoQueueReadyNotify(
			pPdoCtx->Queues.HidControlReadRequests,
			BthPS3_PDO_DispatchHidControlRead,
			pPdoCtx
		)))
		{
			TraceError(
				TRACE_L2CAP,
				"WdfIoQueueReadyNotify (HidControlReadRequests) failed with status %!STATUS!",
				status
			);

			EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfIoQueueReadyNotify (HidControlReadRequests)", status);
		}

		if (!NT_SUCCESS(status = WdfIoQueueReadyNotify(
			pPdoCtx->Queues.HidControlWriteRequests,
			BthPS3_PDO_DispatchHidControlWrite,
			pPdoCtx
		)))
		{
			TraceError(
				TRACE_L2CAP,
				"WdfIoQueueReadyNotify (HidControlWriteRequests) failed with status %!STATUS!",
				status
			);

			EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfIoQueueReadyNotify (HidControlWriteRequests)", status);
		}
	}
	else
	{
		TraceError(
			TRACE_L2CAP,
			"HID Control Channel connection failed with status %!STATUS!",
			status
		);

		EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"", Params->IoStatus.Status);

		BthPS3_PDO_Destroy(pPdoCtx->DevCtxHdr, pPdoCtx);
	}

	FuncExitNoReturn(TRACE_L2CAP);
}

//
// Interrupt channel connection result
// 
void
L2CAP_PS3_InterruptConnectResponseCompleted(
	_In_ WDFREQUEST  Request,
	_In_ WDFIOTARGET  Target,
	_In_ PWDF_REQUEST_COMPLETION_PARAMS  Params,
	_In_ WDFCONTEXT  Context
)
{
	NTSTATUS status;
	struct _BRB_L2CA_OPEN_CHANNEL* brb = NULL;
	PBTHPS3_PDO_CONTEXT pPdoCtx = NULL;
	BTHPS3_CONNECTION_STATE controlState;

	UNREFERENCED_PARAMETER(Request);
	UNREFERENCED_PARAMETER(Target);

	status = Params->IoStatus.Status;

	FuncEntryArguments(TRACE_L2CAP, "status=%!STATUS!", status);


	brb = (struct _BRB_L2CA_OPEN_CHANNEL*)Context;
	pPdoCtx = brb->Hdr.ClientContext[0];

	//
	// Connection acceptance successful, channel ready to operate
	// 
	if (NT_SUCCESS(status))
	{
		WdfSpinLockAcquire(pPdoCtx->HidInterruptChannel.ConnectionStateLock);

		pPdoCtx->HidInterruptChannel.ConnectionState = ConnectionStateConnected;

		//
		// This will be set again once disconnect has occurred
		// 
		KeClearEvent(&pPdoCtx->HidInterruptChannel.DisconnectEvent);

		WdfSpinLockRelease(pPdoCtx->HidInterruptChannel.ConnectionStateLock);

		TraceInformation(
			TRACE_L2CAP,
			"HID Interrupt Channel connection established"
		);

		EventWriteHidInterruptChannelConnected(NULL);

		//
		// Control channel is expected to be established by now
		// 
		WdfSpinLockAcquire(pPdoCtx->HidControlChannel.ConnectionStateLock);
		controlState = pPdoCtx->HidInterruptChannel.ConnectionState;
		WdfSpinLockRelease(pPdoCtx->HidControlChannel.ConnectionStateLock);

		if (controlState != ConnectionStateConnected)
		{
			TraceError(
				TRACE_L2CAP,
				"HID Control Channel in invalid state (0x%02X), dropping connection",
				controlState
			);

			goto failedDrop;
		}

		//
		// Channel connected, queues ready to start processing
		// 

		if (!NT_SUCCESS(status = WdfIoQueueReadyNotify(
			pPdoCtx->Queues.HidInterruptReadRequests,
			BthPS3_PDO_DispatchHidInterruptRead,
			pPdoCtx
		)))
		{
			TraceError(
				TRACE_L2CAP,
				"WdfIoQueueReadyNotify (HidInterruptReadRequests) failed with status %!STATUS!",
				status
			);

			EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfIoQueueReadyNotify (HidInterruptReadRequests)", status);
		}

		if (!NT_SUCCESS(status = WdfIoQueueReadyNotify(
			pPdoCtx->Queues.HidInterruptWriteRequests,
			BthPS3_PDO_DispatchHidInterruptWrite,
			pPdoCtx
		)))
		{
			TraceError(
				TRACE_L2CAP,
				"WdfIoQueueReadyNotify (HidInterruptWriteRequests) failed with status %!STATUS!",
				status
			);

			EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfIoQueueReadyNotify (HidInterruptWriteRequests)", status);
		}

		EventWriteRemoteDeviceOnline(NULL, pPdoCtx->RemoteAddress);
	}
	else
	{
		goto failedDrop;
	}

	FuncExitNoReturn(TRACE_L2CAP);

	return;

failedDrop:

	TraceError(
		TRACE_L2CAP,
		"Connection failed with status %!STATUS!",
		status
	);

	EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"", Params->IoStatus.Status);

	BthPS3_PDO_Destroy(pPdoCtx->DevCtxHdr, pPdoCtx);

	FuncExitNoReturn(TRACE_L2CAP);
}

#pragma endregion

#pragma region L2CAP deny connection request

//
// Deny an L2CAP connection request
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_DenyRemoteConnect(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx,
	_In_ PINDICATION_PARAMETERS ConnectParams
)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDFREQUEST brbAsyncRequest = NULL;
	struct _BRB_L2CA_OPEN_CHANNEL* brb = NULL;


	FuncEntry(TRACE_L2CAP);

	if (!NT_SUCCESS(status = WdfRequestCreate(
		WDF_NO_OBJECT_ATTRIBUTES,
		DevCtx->Header.IoTarget,
		&brbAsyncRequest)))
	{
		TraceError(
			TRACE_L2CAP,
			"WdfRequestCreate failed with status %!STATUS!",
			status
		);

		return status;
	}

	brb = (struct _BRB_L2CA_OPEN_CHANNEL*)
		DevCtx->Header.ProfileDrvInterface.BthAllocateBrb(
			BRB_L2CA_OPEN_CHANNEL_RESPONSE,
			POOLTAG_BTHPS3
		);

	if (brb == NULL)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		TraceError(
			TRACE_L2CAP,
			"Failed to allocate brb BRB_L2CA_OPEN_CHANNEL_RESPONSE with status %!STATUS!",
			status
		);

		WdfObjectDelete(brbAsyncRequest);
		return status;
	}

	brb->Hdr.ClientContext[0] = DevCtx;

	brb->BtAddress = ConnectParams->BtAddress;
	brb->Psm = ConnectParams->Parameters.Connect.Request.PSM;
	brb->ChannelHandle = ConnectParams->ConnectionHandle;

	//
	// Drop connection
	// 
	brb->Response = CONNECT_RSP_RESULT_PSM_NEG;

	brb->ChannelFlags = CF_ROLE_EITHER;

	brb->ConfigOut.Flags = 0;
	brb->ConfigIn.Flags = 0;

	//
	// Submit response
	// 
	if (!NT_SUCCESS(status = BthPS3_SendBrbAsync(
		DevCtx->Header.IoTarget,
		brbAsyncRequest,
		(PBRB)brb,
		sizeof(*brb),
		L2CAP_PS3_DenyRemoteConnectCompleted,
		brb
	)))
	{
		TraceError(
			TRACE_L2CAP,
			"BthPS3_SendBrbAsync failed with status %!STATUS!",
			status
		);

		DevCtx->Header.ProfileDrvInterface.BthFreeBrb((PBRB)brb);
		WdfObjectDelete(brbAsyncRequest);
	}

	FuncExit(TRACE_L2CAP, "status=%!STATUS!", status);

	return status;
}

//
// Free resources used by deny connection response
// 
void
L2CAP_PS3_DenyRemoteConnectCompleted(
	_In_ WDFREQUEST  Request,
	_In_ WDFIOTARGET  Target,
	_In_ PWDF_REQUEST_COMPLETION_PARAMS  Params,
	_In_ WDFCONTEXT  Context
)
{
	PBTHPS3_SERVER_CONTEXT deviceCtx = NULL;
	struct _BRB_L2CA_OPEN_CHANNEL* brb = NULL;

	UNREFERENCED_PARAMETER(Target);


	FuncEntryArguments(TRACE_L2CAP, "status=%!STATUS!", Params->IoStatus.Status);

	brb = (struct _BRB_L2CA_OPEN_CHANNEL*)Context;
	deviceCtx = (PBTHPS3_SERVER_CONTEXT)brb->Hdr.ClientContext[0];
	deviceCtx->Header.ProfileDrvInterface.BthFreeBrb((PBRB)brb);
	WdfObjectDelete(Request);

	FuncExitNoReturn(TRACE_L2CAP);
}

#pragma endregion
