#include "Driver.h"
#include "l2cap.tmh"

//
// Incoming connection request, prepare and send response
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendConnectResponse(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx,
    _In_ PINDICATION_PARAMETERS ConnectParams
)
{
    NTSTATUS status, statusReuse;
    WDF_REQUEST_REUSE_PARAMS reuseParams;
    struct _BRB_L2CA_OPEN_CHANNEL *brb = NULL;
    WDFOBJECT connectionObject = NULL;
    PBTHPS3_CONNECTION connection = NULL;
    PFN_WDF_REQUEST_COMPLETION_ROUTINE completionRoutine = NULL;
    USHORT psm = ConnectParams->Parameters.Connect.Request.PSM;


    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

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

    /*
    if (PSM_DS3_HID_INTERRUPT == ConnectParams->Parameters.Connect.Request.PSM)
    {
    }

    if (RETRIEVE_CONNECTION_ENTRY_BY_BTH_ADDR_LOCKED(
        DevCtx,
        &ConnectParams->BtAddress,
        connection
    ))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_L2CAP,
            "!! Existing connection found");
    }
    */

    //
    // We create the connection object as the first step so that if we receive 
    // remove before connect response is completed
    // we can wait for connection and disconnect.
    //
    status = BthPS3ConnectionObjectCreate(
        &DevCtx->Header,
        DevCtx->Header.Device,
        &connectionObject
    );

    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    connection = GetConnectionObjectContext(connectionObject);

    connection->ConnectionState = ConnectionStateConnecting;

    //
    // Insert the connection object in the connection list that we track
    //
    INSERT_CONNECTION_ENTRY_LOCKED(DevCtx, &connection->ConnectionListEntry);

    //
    // Reuse request
    // 
    WDF_REQUEST_REUSE_PARAMS_INIT(&reuseParams, WDF_REQUEST_REUSE_NO_FLAGS, STATUS_NOT_SUPPORTED);
    statusReuse = WdfRequestReuse(connection->ConnectDisconnectRequest, &reuseParams);
    NT_ASSERT(NT_SUCCESS(statusReuse));
    UNREFERENCED_PARAMETER(statusReuse);

    //
    // Prepare connection response BRB
    // 
    brb = (struct _BRB_L2CA_OPEN_CHANNEL*) &(connection->ConnectDisconnectBrb);
    DevCtx->Header.ProfileDrvInterface.BthReuseBrb((PBRB)brb, BRB_L2CA_OPEN_CHANNEL_RESPONSE);

    //
    // Pass connection object along as context
    // 
    brb->Hdr.ClientContext[0] = connectionObject;

    brb->BtAddress = ConnectParams->BtAddress;
    brb->Psm = ConnectParams->Parameters.Connect.Request.PSM;
    brb->ChannelHandle = ConnectParams->ConnectionHandle;
    brb->Response = CONNECT_RSP_RESULT_SUCCESS;

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
    brb->CallbackContext = connectionObject;
    brb->ReferenceObject = (PVOID)WdfDeviceWdmGetDeviceObject(DevCtx->Header.Device);

    //
    // Submit response
    // 
    status = BthPS3SendBrbAsync(
        DevCtx->Header.IoTarget,
        connection->ConnectDisconnectRequest,
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

    if (!NT_SUCCESS(status) && connectionObject)
    {
        //
        // If we failed to connect remove connection from list and
        // delete the object
        //
        TraceEvents(TRACE_LEVEL_WARNING,
            TRACE_L2CAP,
            "Failed to establish connection, clean-up");

        //
        // connection should not be NULL if connectionObject is not NULL
        // since first thing we do after creating connectionObject is to
        // get context which gives us connection
        //
        NT_ASSERT(connection != NULL);

        if (connection != NULL)
        {
            connection->ConnectionState = ConnectionStateConnectFailed;

            REMOVE_CONNECTION_ENTRY_LOCKED(
                (PBTHPS3_SERVER_CONTEXT)connection->DevCtxHdr,
                &connection->ConnectionListEntry
            );
        }

        WdfObjectDelete(connectionObject);
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
        REMOVE_CONNECTION_ENTRY_LOCKED(
            (PBTHPS3_SERVER_CONTEXT)connection->DevCtxHdr,
            &connection->ConnectionListEntry
        );

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
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(Parameters);

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
    {
        WDFOBJECT connectionObject = (WDFOBJECT)Context;
        PBTHPS3_CONNECTION connection = GetConnectionObjectContext(connectionObject);

        UNREFERENCED_PARAMETER(connection);

        //BthEchoSrvDisconnectConnection(connection);

        break;
    }
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
