#include "Driver.h"

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
