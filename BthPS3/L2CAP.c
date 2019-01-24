#include "Driver.h"
#include "l2cap.tmh"

//
// Incoming connection request, prepare and send response
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_HandleRemoteConnect(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx,
    _In_ PINDICATION_PARAMETERS ConnectParams
)
{
    NTSTATUS status;
    struct _BRB_L2CA_OPEN_CHANNEL *brb = NULL;
    PFN_WDF_REQUEST_COMPLETION_ROUTINE completionRoutine = NULL;
    USHORT psm = ConnectParams->Parameters.Connect.Request.PSM;
    PBTHPS3_CLIENT_CONNECTION clientConnection = NULL;
    WDFREQUEST brbAsyncRequest = NULL;
    CHAR remoteName[BTH_MAX_NAME_SIZE];
    USHORT response = CONNECT_RSP_RESULT_SUCCESS;


    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

    status = BTHPS3_GET_DEVICE_NAME(
        DevCtx->Header.IoTarget,
        ConnectParams->BtAddress,
        remoteName
    );

    if (NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_L2CAP,
            "++ Device %llX name: %s",
            ConnectParams->BtAddress,
            remoteName
        );
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "BTHPS3_GET_DEVICE_NAME failed with status %!STATUS!",
            status
        );

        //
        // No name returned can happen; deny this connection and
        // the controller will retry, then the name request will
        // succeed and we can continue.
        // 
        response = CONNECT_RSP_RESULT_PSM_NEG;
    }

    //
    // Adjust control flow depending on PSM
    // 
    switch (psm)
    {
    case PSM_DS3_HID_CONTROL:
        completionRoutine = L2CAP_PS3_ControlConnectResponseCompleted;
        break;
    case PSM_DS3_HID_INTERRUPT:
        completionRoutine = L2CAP_PS3_InterruptConnectResponseCompleted;
        break;
    default:
        break;
    }

    //completionRoutine = L2CAP_PS3_ConnectResponseCompleted;

    //
    // Look for an existing connection object and reuse that
    // 
    status = ClientConnections_RetrieveByBthAddr(
        DevCtx,
        ConnectParams->BtAddress,
        &clientConnection
    );

    //
    // This device apparently isn't connected, allocate new object
    // 
    if (status == STATUS_NOT_FOUND)
    {
        status = ClientConnections_CreateAndInsert(
            DevCtx,
            ConnectParams->BtAddress,
            EvtClientConnectionsDestroyConnection,
            &clientConnection
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_L2CAP,
                "ClientConnections_CreateAndInsert failed with status %!STATUS!", status);
            goto exit;
        }
    }

    //
    // Prepare connection response BRB
    // 
    switch (psm)
    {
    case PSM_DS3_HID_CONTROL:
        clientConnection->HidControlChannel.ChannelHandle = ConnectParams->ConnectionHandle;
        brbAsyncRequest = clientConnection->HidControlChannel.ConnectDisconnectRequest;
        brb = (struct _BRB_L2CA_OPEN_CHANNEL*) &(clientConnection->HidControlChannel.ConnectDisconnectBrb);
        break;
    case PSM_DS3_HID_INTERRUPT:
        clientConnection->HidInterruptChannel.ChannelHandle = ConnectParams->ConnectionHandle;
        brbAsyncRequest = clientConnection->HidInterruptChannel.ConnectDisconnectRequest;
        brb = (struct _BRB_L2CA_OPEN_CHANNEL*) &(clientConnection->HidInterruptChannel.ConnectDisconnectBrb);
        break;
    default:
        break;
    }

    CLIENT_CONNECTION_REQUEST_REUSE(brbAsyncRequest);
    DevCtx->Header.ProfileDrvInterface.BthReuseBrb((PBRB)brb, BRB_L2CA_OPEN_CHANNEL_RESPONSE);

    //
    // Pass connection object along as context
    // 
    brb->Hdr.ClientContext[0] = clientConnection;

    brb->BtAddress = ConnectParams->BtAddress;
    brb->Psm = ConnectParams->Parameters.Connect.Request.PSM;
    brb->ChannelHandle = ConnectParams->ConnectionHandle;
    brb->Response = response;
    
    brb->ChannelFlags = CF_ROLE_EITHER;

    brb->ConfigOut.Flags = 0;
    brb->ConfigIn.Flags = 0;

    //
    // Set expected and preferred MTU to max value
    // 
    brb->ConfigOut.Flags |= CFG_MTU;
    brb->ConfigOut.Mtu.Max = L2CAP_MAX_MTU;
    brb->ConfigOut.Mtu.Min = L2CAP_MIN_MTU;
    brb->ConfigOut.Mtu.Preferred = L2CAP_MAX_MTU;

    brb->ConfigIn.Flags = CFG_MTU;
    brb->ConfigIn.Mtu.Max = brb->ConfigOut.Mtu.Max;
    brb->ConfigIn.Mtu.Min = brb->ConfigOut.Mtu.Min;
    brb->ConfigIn.Mtu.Preferred = brb->ConfigOut.Mtu.Max;

    //
    // Get notifications about disconnect and QOS
    //
    brb->CallbackFlags = CALLBACK_DISCONNECT | CALLBACK_CONFIG_QOS;
    brb->Callback = &L2CAP_PS3_ConnectionIndicationCallback;
    brb->CallbackContext = clientConnection;
    brb->ReferenceObject = (PVOID)WdfDeviceWdmGetDeviceObject(DevCtx->Header.Device);

    //
    // Submit response
    // 
    status = BthPS3SendBrbAsync(
        DevCtx->Header.IoTarget,
        brbAsyncRequest,
        (PBRB)brb,
        sizeof(*brb),
        completionRoutine,
        brb
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_L2CAP,
            "BthPS3SendBrbAsync failed with status %!STATUS!", status);
    }

exit:

    if (!NT_SUCCESS(status) && clientConnection)
    {
        ClientConnections_RemoveAndDestroy(DevCtx, clientConnection);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit (%!STATUS!)", status);

    return status;
}

void
L2CAP_PS3_ConnectResponseCompleted(
    _In_ WDFREQUEST  Request,
    _In_ WDFIOTARGET  Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS  Params,
    _In_ WDFCONTEXT  Context
)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

    NTSTATUS status;
    struct _BRB_L2CA_OPEN_CHANNEL *brb;
    PBTHPS3_CONNECTION connection;
    WDFOBJECT connectionObject;


    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Request); //we reuse the request, hence it is not needed here

    status = Params->IoStatus.Status;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_L2CAP,
        "Connection completion, status: %!STATUS!", status);

    brb = (struct _BRB_L2CA_OPEN_CHANNEL *) Context;

    //
    // We receive connection object as the context in the BRB
    //
    connectionObject = (WDFOBJECT)brb->Hdr.ClientContext[0];
    connection = GetConnectionObjectContext(connectionObject);

    if (NT_SUCCESS(status))
    {
        connection->ChannelHandle = brb->ChannelHandle;
        connection->RemoteAddress = brb->BtAddress;

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_L2CAP,
            "Connection established with client");

        //
        // Check if we already received a disconnect request
        // and if so disconnect
        //

        WdfSpinLockAcquire(connection->ConnectionLock);

        if (connection->ConnectionState == ConnectionStateDisconnecting)
        {
            //
            // We allow transition to disconnected state only from
            // connected state
            //
            // If we are in disconnection state this means that
            // we were waiting for connect to complete before we
            // can send disconnect down
            //
            // Set the state to Connected and call BthEchoConnectionObjectRemoteDisconnect
            //
            connection->ConnectionState = ConnectionStateConnected;
            WdfSpinLockRelease(connection->ConnectionLock);

            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_L2CAP,
                "Remote connect response completion: "
                "disconnect has been received "
                "for connection 0x%p", connection);

            //BthEchoConnectionObjectRemoteDisconnect(connection->DevCtxHdr, connection);
        }
        else
        {
            connection->ConnectionState = ConnectionStateConnected;
            WdfSpinLockRelease(connection->ConnectionLock);

            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_L2CAP,
                "Connection completed, connection: 0x%p", connection);

            //
            // Call BthEchoSrvConnectionStateConnected to perform post connect processing
            // (namely initializing continuous readers)
            //
            status = L2CAP_PS3_ConnectionStateConnected(connectionObject);
            //if (!NT_SUCCESS(status))
            //{
            //    //
            //    // If the post connect processing failed, let us disconnect
            //    //
            //    BthEchoConnectionObjectRemoteDisconnect(connection->DevCtxHdr, connection);
            //}

        }
    }
    else
    {
        BOOLEAN setDisconnectEvent = FALSE;

        WdfSpinLockAcquire(connection->ConnectionLock);
        if (connection->ConnectionState == ConnectionStateDisconnecting)
        {
            setDisconnectEvent = TRUE;
        }

        connection->ConnectionState = ConnectionStateConnectFailed;
        WdfSpinLockRelease(connection->ConnectionLock);

        if (setDisconnectEvent)
        {
            KeSetEvent(&connection->DisconnectEvent,
                0,
                FALSE);
        }

    }

    if (!NT_SUCCESS(status))
    {
        //REMOVE_CONNECTION_ENTRY_LOCKED(
        //    (PBTHPS3_SERVER_CONTEXT)connection->DevCtxHdr,
        //    &connection->ConnectionListEntry
        //);

        WdfObjectDelete(connectionObject);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");

    return;
}

//
// Control channel connection result
// 
void
L2CAP_PS3_ControlConnectResponseCompleted(
    _In_ WDFREQUEST  Request,
    _In_ WDFIOTARGET  Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS  Params,
    _In_ WDFCONTEXT  Context
)
{
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);
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
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
L2CAP_PS3_ConnectionIndicationCallback(
    _In_ PVOID Context,
    _In_ INDICATION_CODE Indication,
    _In_ PINDICATION_PARAMETERS Parameters
)
{
    PBTHPS3_SERVER_CONTEXT deviceCtx = NULL;
    PBTHPS3_CLIENT_CONNECTION connection = NULL;

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_L2CAP,
        "%!FUNC! Entry (Indication: 0x%X, Context: 0x%p)",
        Indication, Context
    );

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

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_L2CAP,
            "++ IndicationRemoteDisconnect [0x%p]",
            Parameters->ConnectionHandle);

        connection = (PBTHPS3_CLIENT_CONNECTION)Context;
        deviceCtx = GetServerDeviceContext(connection->DevCtxHdr->Device);

        //
        // HID Control Channel disconnected
        // 
        if (Parameters->ConnectionHandle ==
            connection->HidControlChannel.ChannelHandle)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_L2CAP,
                "++ HID Control Channel 0x%p disconnected",
                Parameters->ConnectionHandle);

            connection->HidControlChannel.ConnectionState = ConnectionStateDisconnected;
        }

        //
        // HID Interrupt Channel disconnected
        // 
        if (Parameters->ConnectionHandle ==
            connection->HidInterruptChannel.ChannelHandle)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_L2CAP,
                "++ HID Interrupt Channel 0x%p disconnected",
                Parameters->ConnectionHandle);

            connection->HidInterruptChannel.ConnectionState = ConnectionStateDisconnected;
        }

        //
        // Both channels are gone, invoke clean-up
        // 
        if (connection->HidControlChannel.ConnectionState == ConnectionStateDisconnected
            && connection->HidInterruptChannel.ConnectionState == ConnectionStateDisconnected)
        {
            ClientConnections_RemoveAndDestroy(deviceCtx, connection);
        }

        break;

    case IndicationRemoteConfigRequest:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_L2CAP,
            "%!FUNC! ++ IndicationRemoteConfigRequest");

        //
        // TODO: this catches QOS configuration request and inherently succeeds it
        // 

        break;

    case IndicationRemoteConfigResponse:

        TraceEvents(TRACE_LEVEL_INFORMATION,
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

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ConnectionStateConnected(
    WDFOBJECT ConnectionObject
)
{
    UNREFERENCED_PARAMETER(ConnectionObject);

    return STATUS_SUCCESS;
}
