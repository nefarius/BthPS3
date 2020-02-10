/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2020, Nefarius Software Solutions e.U.                      *
 * All rights reserved.                                                           *
 *                                                                                *
 * Redistribution and use in source and binary forms, with or without             *
 * modification, are permitted provided that the following conditions are met:    *
 *                                                                                *
 * 1. Redistributions of source code must retain the above copyright notice, this *
 *    list of conditions and the following disclaimer.                            *
 *                                                                                *
 * 2. Redistributions in binary form must reproduce the above copyright notice,   *
 *    this list of conditions and the following disclaimer in the documentation   *
 *    and/or other materials provided with the distribution.                      *
 *                                                                                *
 * 3. Neither the name of the copyright holder nor the names of its               *
 *    contributors may be used to endorse or promote products derived from        *
 *    this software without specific prior written permission.                    *
 *                                                                                *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"    *
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE      *
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE *
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE   *
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL     *
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR     *
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER     *
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  *
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  *
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.           *
 *                                                                                *
 **********************************************************************************/


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
        TRACE_CONNECTION,
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

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_CONNECTION, "%!FUNC! Entry");

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

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_CONNECTION, "%!FUNC! Exit (%!STATUS!)", status);

    return status;
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

    TraceEvents(TRACE_LEVEL_INFORMATION, 
        TRACE_CONNECTION, 
        "%!FUNC! Entry (DISPOSING CONNECTION MEMORY)"
    );

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

    //
    // STATUS_NO_SUCH_DEVICE can happen on parent shutdown
    // as the framework will reap all children faster than
    // this clean-up callback can get to it
    // 
    if (!NT_SUCCESS(status) && status != STATUS_NO_SUCH_DEVICE)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_CONNECTION,
            "WdfChildListUpdateChildDescriptionAsMissing failed with status %!STATUS!",
            status);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_CONNECTION, "%!FUNC! Exit");
}
