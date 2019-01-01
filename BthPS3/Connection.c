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
    // Initalize list entry
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
    // TODO: implement
    //attributes.EvtCleanupCallback = BthEchoEvtConnectionObjectCleanup;

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
