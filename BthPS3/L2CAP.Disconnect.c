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
    PBTHPS3_CLIENT_CONNECTION connection = NULL;
    PDO_IDENTIFICATION_DESCRIPTION pdoDesc;

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

        connection = (PBTHPS3_CLIENT_CONNECTION)Context;
        deviceCtx = GetServerDeviceContext(connection->DevCtxHdr->Device);

        //
        // HID Control Channel disconnected
        // 
        if (Parameters->ConnectionHandle ==
            connection->HidControlChannel.ChannelHandle)
        {
            TraceVerbose(
                TRACE_L2CAP,
                "HID Control Channel 0x%p disconnected",
                Parameters->ConnectionHandle);

            L2CAP_PS3_RemoteDisconnect(
                connection->DevCtxHdr,
                connection->RemoteAddress,
                &connection->HidControlChannel
            );
        }

        //
        // HID Interrupt Channel disconnected
        // 
        if (Parameters->ConnectionHandle ==
            connection->HidInterruptChannel.ChannelHandle)
        {
            TraceVerbose(
                TRACE_L2CAP,
                "HID Interrupt Channel 0x%p disconnected",
                Parameters->ConnectionHandle);

            L2CAP_PS3_RemoteDisconnect(
                connection->DevCtxHdr,
                connection->RemoteAddress,
                &connection->HidInterruptChannel
            );
        }

        //
        // Both channels are gone, invoke clean-up
        // 
        if (connection->HidControlChannel.ConnectionState == ConnectionStateDisconnected
            && connection->HidInterruptChannel.ConnectionState == ConnectionStateDisconnected)
        {
            TraceVerbose(
                TRACE_L2CAP,
                "Both channels are gone, awaiting clean-up"
            );

            status = KeWaitForSingleObject(
                &connection->HidControlChannel.DisconnectEvent,
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
                &connection->HidInterruptChannel.DisconnectEvent,
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

            WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(
                &pdoDesc.Header,
                sizeof(PDO_IDENTIFICATION_DESCRIPTION)
            );

            pdoDesc.ClientConnection = connection;

            //
            // Init PDO destruction
            // 
            status = WdfChildListUpdateChildDescriptionAsMissing(
                WdfFdoGetDefaultChildList(connection->DevCtxHdr->Device),
                &pdoDesc.Header
            );

            if (!NT_SUCCESS(status))
            {
                TraceError(
                    TRACE_CONNECTION,
                    "WdfChildListUpdateChildDescriptionAsMissing failed with status %!STATUS!",
                    status);
            }

            ClientConnections_RemoveAndDestroy(&deviceCtx->Header, connection);
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
