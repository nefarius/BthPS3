/*
* BthPS3 - Windows kernel-mode Bluetooth profile and bus driver
* Copyright (C) 2018-2019  Nefarius Software Solutions e.U. and Contributors
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "Driver.h"
#include "connection.tmh"


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

    //
    // Initialize signaled, will be cleared once a connection is established
    // 
    KeInitializeEvent(&connectionCtx->HidControlChannel.DisconnectEvent,
        NotificationEvent,
        TRUE
    );

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = connectionObject;

    status = WdfSpinLockCreate(
        &attributes,
        &connectionCtx->HidControlChannel.ConnectionStateLock
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_CONNECTION,
            "WdfSpinLockCreate for HidControlChannel failed with status %!STATUS!",
            status
        );

        goto exitFailure;
    }

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

    //
    // Initialize signaled, will be cleared once a connection is established
    // 
    KeInitializeEvent(&connectionCtx->HidInterruptChannel.DisconnectEvent,
        NotificationEvent,
        TRUE
    );

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = connectionObject;

    status = WdfSpinLockCreate(
        &attributes,
        &connectionCtx->HidInterruptChannel.ConnectionStateLock
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_CONNECTION,
            "WdfSpinLockCreate for HidInterruptChannel failed with status %!STATUS!",
            status
        );

        goto exitFailure;
    }

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

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");
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

//
// Drops all open connections and frees their resources
// 
VOID
ClientConnections_DropAndFreeAll(
    _In_ PBTHPS3_SERVER_CONTEXT Context
)
{
    ULONG itemCount;
    ULONG index;
    WDFOBJECT currentItem;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

    WdfSpinLockAcquire(Context->ClientConnectionsLock);

    itemCount = WdfCollectionGetCount(Context->ClientConnections);

    for (index = 0; index < itemCount; index++)
    {
        currentItem = WdfCollectionGetItem(Context->ClientConnections, index);

        WdfCollectionRemoveItem(Context->ClientConnections, index);

        //
        // Triggers clean-up
        // 
        WdfObjectDelete(currentItem);
    }

    WdfSpinLockRelease(Context->ClientConnectionsLock);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");
}

//
// Performs clean-up when a connection object is disposed
// 
_Use_decl_annotations_
VOID
EvtClientConnectionsDestroyConnection(
    WDFOBJECT Object
)
{
    NTSTATUS status;
    PDO_IDENTIFICATION_DESCRIPTION pdoDesc;
    PBTHPS3_CLIENT_CONNECTION connection = NULL;
    struct _BRB_L2CA_CLOSE_CHANNEL *disconnectBrb = NULL;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_CONNECTION, "%!FUNC! Entry");

    connection = GetClientConnection(Object);

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
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_CONNECTION,
            "WdfChildListUpdateChildDescriptionAsMissing failed with status %!STATUS!",
            status);
    }

    //
    // Invoke clean channel disconnect - if possible at this stage
    // 

    //
    // TODO: add proper locking!
    // 

#pragma region Disconnect HID Interrupt Channel

    CLIENT_CONNECTION_REQUEST_REUSE(connection->HidInterruptChannel.ConnectDisconnectRequest);
    connection->DevCtxHdr->ProfileDrvInterface.BthReuseBrb(
        &connection->HidInterruptChannel.ConnectDisconnectBrb,
        BRB_L2CA_CLOSE_CHANNEL
    );

    disconnectBrb = (struct _BRB_L2CA_CLOSE_CHANNEL *) &(connection->HidInterruptChannel.ConnectDisconnectBrb);
    disconnectBrb->BtAddress = connection->RemoteAddress;
    disconnectBrb->ChannelHandle = connection->HidInterruptChannel.ChannelHandle;

    //
    // May fail if radio got surprise-removed, so don't check return value
    // 
    (void)BthPS3_SendBrbSynchronously(
        connection->DevCtxHdr->IoTarget,
        connection->HidInterruptChannel.ConnectDisconnectRequest,
        (PBRB)disconnectBrb,
        sizeof(*disconnectBrb)
    );

    KeWaitForSingleObject(
        &connection->HidInterruptChannel.DisconnectEvent,
        Executive,
        KernelMode,
        FALSE,
        NULL
    );

#pragma endregion

#pragma region Disconnect HID Control Channel

    CLIENT_CONNECTION_REQUEST_REUSE(connection->HidControlChannel.ConnectDisconnectRequest);
    connection->DevCtxHdr->ProfileDrvInterface.BthReuseBrb(
        &connection->HidControlChannel.ConnectDisconnectBrb,
        BRB_L2CA_CLOSE_CHANNEL
    );

    disconnectBrb = (struct _BRB_L2CA_CLOSE_CHANNEL *) &(connection->HidControlChannel.ConnectDisconnectBrb);
    disconnectBrb->BtAddress = connection->RemoteAddress;
    disconnectBrb->ChannelHandle = connection->HidControlChannel.ChannelHandle;

    //
    // May fail if radio got surprise-removed, so don't check return value
    // 
    (void)BthPS3_SendBrbSynchronously(
        connection->DevCtxHdr->IoTarget,
        connection->HidControlChannel.ConnectDisconnectRequest,
        (PBRB)disconnectBrb,
        sizeof(*disconnectBrb)
    );

    KeWaitForSingleObject(
        &connection->HidControlChannel.DisconnectEvent,
        Executive,
        KernelMode,
        FALSE,
        NULL
    );

#pragma endregion

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_CONNECTION, "%!FUNC! Exit");
}
