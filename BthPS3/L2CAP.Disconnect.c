#include "Driver.h"
#include "L2CAP.Disconnect.tmh"


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
    PBTHPS3_SERVER_CONTEXT deviceCtx = NULL;
    PBTHPS3_PDO_CONTEXT pPdoCtx = NULL;

	//
	// TODO: queue work item to lower IRQL instead of this!
	// 

	LARGE_INTEGER timeout;
    timeout.QuadPart = 0;

    FuncEntryArguments(TRACE_L2CAP, "Indication=0x%X, Context=0x%p", 
        Indication, Context);
	

    switch (Indication)
    {
    case IndicationAddReference:
        break;
    case IndicationReleaseReference:
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

        pPdoCtx = (PBTHPS3_PDO_CONTEXT)Context;
        deviceCtx = GetServerDeviceContext(pPdoCtx->DevCtxHdr->Device);

        //
        // HID Control Channel disconnected
        // 
        if (Parameters->ConnectionHandle ==
            pPdoCtx->HidControlChannel.ChannelHandle)
        {
            TraceVerbose(
                TRACE_L2CAP,
                "HID Control Channel 0x%p disconnected",
                Parameters->ConnectionHandle);

            L2CAP_PS3_RemoteDisconnect(
                pPdoCtx->DevCtxHdr,
                pPdoCtx->RemoteAddress,
                &pPdoCtx->HidControlChannel
            );
        }

        //
        // HID Interrupt Channel disconnected
        // 
        if (Parameters->ConnectionHandle ==
            pPdoCtx->HidInterruptChannel.ChannelHandle)
        {
            TraceVerbose(
                TRACE_L2CAP,
                "HID Interrupt Channel 0x%p disconnected",
                Parameters->ConnectionHandle);

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
        	if(!NT_SUCCESS(status))
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

            BthPS3_PDO_Destroy(&deviceCtx->Header, pPdoCtx);
        }

        break;

    case IndicationRemoteConfigRequest:

        TraceInformation(
            TRACE_L2CAP,
            "%!FUNC! ++ IndicationRemoteConfigRequest");

        //
        // This catches QOS configuration request and inherently succeeds it
        // 

        break;

    case IndicationRemoteConfigResponse:

        TraceInformation(
            TRACE_L2CAP,
            "%!FUNC! ++ IndicationRemoteConfigResponse");

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
