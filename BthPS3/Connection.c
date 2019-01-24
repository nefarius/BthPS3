#include "Driver.h"
#include "connection.tmh"



/************************************************************************/
/* The new stuff                                                        */
/************************************************************************/

//
// Creates & allocates new connection object and inserts it into connection list
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
ClientConnections_CreateAndInsert(
    _In_ PBTHPS3_SERVER_CONTEXT Context,
    _In_ BTH_ADDR RemoteAddress,
    _In_ PFN_WDF_OBJECT_CONTEXT_CLEANUP CleanupCallback,
    _Out_ PBTHPS3_CLIENT_CONNECTION *ClientConnection
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
    // This is our "primary key"
    // 
    connectionCtx->RemoteAddress = RemoteAddress;

    //
    // Pass back valid pointer
    // 
    *ClientConnection = connectionCtx;

    return status;

exitFailure:

    WdfObjectDelete(connectionObject);
    return status;
}

//
// Removes supplied client connection from connection list and frees its resources
// 
VOID
ClientConnections_RemoveAndDestroy(
    _In_ PBTHPS3_SERVER_CONTEXT Context,
    _In_ PBTHPS3_CLIENT_CONNECTION ClientConnection
)
{
    ULONG itemCount;
    ULONG index;
    WDFOBJECT item, currentItem;

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_L2CAP,
        "%!FUNC! Entry (ClientConnection: 0x%p)",
        ClientConnection
    );

    WdfSpinLockAcquire(Context->ClientConnectionsLock);

    item = WdfObjectContextGetObject(ClientConnection);
    itemCount = WdfCollectionGetCount(Context->ClientConnections);

    for (index = 0; index < itemCount; index++)
    {
        currentItem = WdfCollectionGetItem(Context->ClientConnections, index);

        if (currentItem == item)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_CONNECTION,
                "++ Found desired connection item in connection list"
            );

            WdfCollectionRemoveItem(Context->ClientConnections, index);
            WdfObjectDelete(item);
            break;
        }
    }

    WdfSpinLockRelease(Context->ClientConnectionsLock);
}

//
// Retrieves an existing connection from connection list identified by BTH_ADDR
// 
NTSTATUS
ClientConnections_RetrieveByBthAddr(
    _In_ PBTHPS3_SERVER_CONTEXT Context,
    _In_ BTH_ADDR RemoteAddress,
    _Out_ PBTHPS3_CLIENT_CONNECTION *ClientConnection
)
{
    NTSTATUS status = STATUS_NOT_FOUND;
    ULONG itemCount;
    ULONG index;
    WDFOBJECT currentItem;
    PBTHPS3_CLIENT_CONNECTION connection;

    WdfSpinLockAcquire(Context->ClientConnectionsLock);

    itemCount = WdfCollectionGetCount(Context->ClientConnections);

    for (index = 0; index < itemCount; index++)
    {
        currentItem = WdfCollectionGetItem(Context->ClientConnections, index);
        connection = GetClientConnection(currentItem);

        if (connection->RemoteAddress == RemoteAddress)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_CONNECTION,
                "++ Found desired connection item in connection list"
            );

            status = STATUS_SUCCESS;
            *ClientConnection = connection;
            break;
        }
    }

    WdfSpinLockRelease(Context->ClientConnectionsLock);

    return status;
}

_Use_decl_annotations_
VOID
EvtClientConnectionsDestroyConnection(
    WDFOBJECT Object
)
{
    PBTHPS3_CLIENT_CONNECTION connection = NULL;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_CONNECTION, "%!FUNC! Entry");

    connection = GetClientConnection(Object);

    WdfObjectDelete(connection->HidControlChannel.ConnectDisconnectRequest);
    WdfObjectDelete(connection->HidInterruptChannel.ConnectDisconnectRequest);
}
