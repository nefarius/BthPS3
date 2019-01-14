#include "Driver.h"
#include "connection.tmh"

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3ConnectionObjectInit(
    _In_ WDFOBJECT ConnectionObject,
    _In_ PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr
)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    PBTHPS3_CONNECTION connection = GetConnectionObjectContext(ConnectionObject);


    connection->DevCtxHdr = DevCtxHdr;

    connection->ConnectionState = ConnectionStateInitialized;

    //
    // Initialize spinlock
    //

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = ConnectionObject;

    status = WdfSpinLockCreate(
        &attributes,
        &connection->ConnectionLock
    );
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    //
    // Create connect/disconnect request
    //

    status = WdfRequestCreate(
        &attributes,
        DevCtxHdr->IoTarget,
        &connection->ConnectDisconnectRequest
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Initialize event
    //

    KeInitializeEvent(&connection->DisconnectEvent, NotificationEvent, TRUE);

    //
    // Initialize list entry
    //

    InitializeListHead(&connection->ConnectionListEntry);

    connection->ConnectionState = ConnectionStateInitialized;

exit:
    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3ConnectionObjectCreate(
    _In_ PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr,
    _In_ WDFOBJECT ParentObject,
    _Out_ WDFOBJECT*  ConnectionObject
)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFOBJECT connectionObject = NULL;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, BTHPS3_CONNECTION);
    attributes.ParentObject = ParentObject;
    attributes.EvtCleanupCallback = BthPS3EvtConnectionObjectCleanup;

    //
    // We set execution level to passive so that we get cleanup at passive
    // level where we can wait for continuous readers to run down
    // and for completion of disconnect
    //
    attributes.ExecutionLevel = WdfExecutionLevelPassive;

    status = WdfObjectCreate(
        &attributes,
        &connectionObject
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_CONNECTION,
            "WdfObjectCreate for connection object failed, Status code %!STATUS!\n", status);

        goto exit;
    }

    status = BthPS3ConnectionObjectInit(connectionObject, DevCtxHdr);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_CONNECTION,
            "Context initialize for connection object failed, ConnectionObject 0x%p, Status code %!STATUS!\n",
            connectionObject,
            status
        );

        goto exit;
    }

    *ConnectionObject = connectionObject;

exit:
    if (!NT_SUCCESS(status) && connectionObject)
    {
        WdfObjectDelete(connectionObject);
    }

    return status;
}

#pragma warning(push)
#pragma warning(disable:28118) // this callback will run at IRQL=PASSIVE_LEVEL
_Use_decl_annotations_
VOID
BthPS3EvtConnectionObjectCleanup(
    WDFOBJECT  ConnectionObject
)
{
    PBTHPS3_CONNECTION connection = GetConnectionObjectContext(ConnectionObject);

    PAGED_CODE();

    //BthEchoConnectionObjectWaitForAndUninitializeContinuousReader(connection);

    KeWaitForSingleObject(&connection->DisconnectEvent,
        Executive,
        KernelMode,
        FALSE,
        NULL);

    WdfObjectDelete(connection->ConnectDisconnectRequest);
}
#pragma warning(pop) // enable 28118 again

/************************************************************************/
/* The new stuff                                                        */
/************************************************************************/

NTSTATUS
ClientConnections_CreateAndInsert(
    _In_ PBTHPS3_SERVER_CONTEXT Context,
    _In_ PFN_WDF_OBJECT_CONTEXT_CLEANUP CleanupCallback,
    _Out_ PBTHPS3_CLIENT_CONNECTION ClientConnection
)
{
    NTSTATUS                    status;
    WDF_OBJECT_ATTRIBUTES       attributes;
    WDFOBJECT                   connectionObject = NULL;
    PBTHPS3_CLIENT_CONNECTION   connectionCtx = NULL;


    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, BTHPS3_CLIENT_CONNECTION);
    attributes.ParentObject = Context->Header.Device;
    attributes.EvtCleanupCallback = CleanupCallback;
    attributes.ExecutionLevel = WdfExecutionLevelPassive;

    //
    // Create piggyback object carrying the context
    // 
    status = WdfObjectCreate(
        &attributes,
        &connectionObject
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_CONNECTION,
            "WdfObjectCreate for connection object failed with status %!STATUS!",
            status
        );

        return status;
    }

    //
    // Piggyback object is parent
    // 
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = connectionObject;

    connectionCtx = GetClientConnection(connectionObject);
    connectionCtx->DevCtxHdr = &Context->Header;

    //
    // Initialize HidControlChannel properties
    // 

    status = WdfRequestCreate(
        &attributes,
        connectionCtx->DevCtxHdr->IoTarget,
        &connectionCtx->HidControlChannel.ConnectDisconnectRequest
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_CONNECTION,
            "WdfRequestCreate for HidControlChannel failed with status %!STATUS!",
            status
        );

        goto exitFailure;
    }

    KeInitializeEvent(&connectionCtx->HidControlChannel.DisconnectEvent,
        NotificationEvent,
        TRUE
    );

    connectionCtx->HidControlChannel.ConnectionState = ConnectionStateInitialized;

    //
    // Initialize HidInterruptChannel properties
    // 

    status = WdfRequestCreate(
        &attributes,
        connectionCtx->DevCtxHdr->IoTarget,
        &connectionCtx->HidInterruptChannel.ConnectDisconnectRequest
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_CONNECTION,
            "WdfRequestCreate for HidInterruptChannel failed with status %!STATUS!",
            status
        );

        goto exitFailure;
    }

    KeInitializeEvent(&connectionCtx->HidInterruptChannel.DisconnectEvent,
        NotificationEvent,
        TRUE
    );

    connectionCtx->HidInterruptChannel.ConnectionState = ConnectionStateInitialized;

    //
    // Insert initialized connection list in connection collection
    // 
    status = WdfCollectionAdd(Context->ClientConnections, connectionObject);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_CONNECTION,
            "WdfCollectionAdd for connection object failed with status %!STATUS!",
            status
        );

        goto exitFailure;
    }

    //
    // Pass back valid pointer
    // 
    ClientConnection = connectionCtx;

    return status;

exitFailure:

    WdfObjectDelete(connectionObject);
    return status;
}

