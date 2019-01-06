#include "Driver.h"
#include "l2cap.tmh"

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


    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

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
    InsertConnectionEntryLocked(DevCtx, &connection->ConnectionListEntry);

    WDF_REQUEST_REUSE_PARAMS_INIT(&reuseParams, WDF_REQUEST_REUSE_NO_FLAGS, STATUS_NOT_SUPPORTED);
    statusReuse = WdfRequestReuse(connection->ConnectDisconnectRequest, &reuseParams);
    NT_ASSERT(NT_SUCCESS(statusReuse));
    UNREFERENCED_PARAMETER(statusReuse);

    brb = (struct _BRB_L2CA_OPEN_CHANNEL*) &(connection->ConnectDisconnectBrb);
    DevCtx->Header.ProfileDrvInterface.BthReuseBrb((PBRB)brb, BRB_L2CA_OPEN_CHANNEL_RESPONSE);

    brb->Hdr.ClientContext[0] = connectionObject;
    brb->Hdr.ClientContext[1] = DevCtx;

    brb->BtAddress = ConnectParams->BtAddress;
    brb->Psm = ConnectParams->Parameters.Connect.Request.PSM;
    brb->ChannelHandle = ConnectParams->ConnectionHandle;
    brb->Response = CONNECT_RSP_RESULT_PENDING;

    brb->ChannelFlags = CF_ROLE_EITHER;

    brb->ConfigOut.Flags = 0;
    brb->ConfigIn.Flags = 0;

    brb->ConfigOut.Flags |= CFG_MTU;
    brb->ConfigOut.Mtu.Max = L2CAP_DEFAULT_MTU;
    brb->ConfigOut.Mtu.Min = L2CAP_MIN_MTU;
    brb->ConfigOut.Mtu.Preferred = L2CAP_DEFAULT_MTU;

    brb->ConfigIn.Flags = CFG_MTU;
    brb->ConfigIn.Mtu.Max = brb->ConfigOut.Mtu.Max;
    brb->ConfigIn.Mtu.Min = brb->ConfigOut.Mtu.Min;
    brb->ConfigIn.Mtu.Preferred = brb->ConfigOut.Mtu.Max;

    //
    // Get notifications about disconnect
    //
    brb->CallbackFlags = CALLBACK_DISCONNECT;
    brb->Callback = &BthPS3ConnectionIndicationCallback;
    brb->CallbackContext = connectionObject;
    brb->ReferenceObject = (PVOID)WdfDeviceWdmGetDeviceObject(DevCtx->Header.Device);

    status = BthPS3SendBrbAsync(
        DevCtx->Header.IoTarget,
        connection->ConnectDisconnectRequest,
        (PBRB)brb,
        sizeof(*brb),
        L2CAP_PS3_ConnectResponsePendingCompleted,
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

            RemoveConnectionEntryLocked(
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
L2CAP_PS3_ConnectResponsePendingCompleted(
    _In_ WDFREQUEST  Request,
    _In_ WDFIOTARGET  Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS  Params,
    _In_ WDFCONTEXT  Context
)
{
    NTSTATUS status;
    struct _BRB_L2CA_OPEN_CHANNEL *brb;
    PBTHPS3_CONNECTION connection;
    WDFOBJECT connectionObject;
    NTSTATUS statusReuse;
    WDF_REQUEST_REUSE_PARAMS reuseParams;
    PBTHPS3_SERVER_CONTEXT DevCtx;

    UNREFERENCED_PARAMETER(Request); // Request gets reused, ignore
    UNREFERENCED_PARAMETER(Target);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");


    status = Params->IoStatus.Status;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_L2CAP,
        "Connection PENDING completed with status: %!STATUS!", status);

    brb = (struct _BRB_L2CA_OPEN_CHANNEL *) Context;

    //
    // We receive connection object as the context in the BRB
    //
    connectionObject = (WDFOBJECT)brb->Hdr.ClientContext[0];
    connection = GetConnectionObjectContext(connectionObject);

    if (NT_SUCCESS(status))
    {
        WDF_REQUEST_REUSE_PARAMS_INIT(&reuseParams, WDF_REQUEST_REUSE_NO_FLAGS, STATUS_NOT_SUPPORTED);
        statusReuse = WdfRequestReuse(connection->ConnectDisconnectRequest, &reuseParams);
        NT_ASSERT(NT_SUCCESS(statusReuse));
        UNREFERENCED_PARAMETER(statusReuse);

        brb = (struct _BRB_L2CA_OPEN_CHANNEL*) &(connection->ConnectDisconnectBrb);
        DevCtx = (PBTHPS3_SERVER_CONTEXT)brb->Hdr.ClientContext[1];

        L2CAP_REUSE_OPEN_CHANNEL_RESPONSE(
            &DevCtx->Header.ProfileDrvInterface,
            BRB_L2CA_OPEN_CHANNEL_RESPONSE,
            brb
        );

        brb->Hdr.ClientContext[0] = connectionObject;

        brb->Response = CONNECT_RSP_RESULT_SUCCESS;

        brb->ChannelFlags = CF_ROLE_EITHER;

        brb->ConfigOut.Flags = 0;
        brb->ConfigIn.Flags = 0;

        brb->ConfigOut.Flags |= CFG_MTU;
        brb->ConfigOut.Mtu.Max = L2CAP_DEFAULT_MTU;
        brb->ConfigOut.Mtu.Min = L2CAP_MIN_MTU;
        brb->ConfigOut.Mtu.Preferred = L2CAP_DEFAULT_MTU;

        brb->ConfigIn.Flags = CFG_MTU;
        brb->ConfigIn.Mtu.Max = brb->ConfigOut.Mtu.Max;
        brb->ConfigIn.Mtu.Min = brb->ConfigOut.Mtu.Min;
        brb->ConfigIn.Mtu.Preferred = brb->ConfigOut.Mtu.Max;

        //
        // Get notifications about disconnect
        //
        brb->CallbackFlags = CALLBACK_DISCONNECT;
        brb->Callback = &BthPS3ConnectionIndicationCallback;
        brb->CallbackContext = connectionObject;
        brb->ReferenceObject = (PVOID)WdfDeviceWdmGetDeviceObject(DevCtx->Header.Device);

        status = BthPS3SendBrbAsync(
            DevCtx->Header.IoTarget,
            connection->ConnectDisconnectRequest,
            (PBRB)brb,
            sizeof(*brb),
            BthPS3RemoteConnectResponseCompletion,
            brb
        );

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_L2CAP,
                "BthPS3SendBrbAsync failed with status %!STATUS!", status);
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
        RemoveConnectionEntryLocked(
            (PBTHPS3_SERVER_CONTEXT)connection->DevCtxHdr,
            &connection->ConnectionListEntry
        );

        WdfObjectDelete(connectionObject);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");

    return;
}