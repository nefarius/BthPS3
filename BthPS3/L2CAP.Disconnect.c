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
