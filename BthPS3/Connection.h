#pragma once

#include <ntstrsafe.h>


typedef struct _BTHPS3_CONNECTION * PBTHPS3_CONNECTION;

#define BTHPS3_NUM_CONTINUOUS_READERS 2

typedef VOID
(*PFN_BTHPS3_CONNECTION_OBJECT_CONTREADER_READ_COMPLETE) (
    _In_ PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr,
    _In_ PBTHPS3_CONNECTION Connection,
    _In_ PVOID Buffer,
    _In_ size_t BufferSize
    );

typedef VOID
(*PFN_BTHPS3_CONNECTION_OBJECT_CONTREADER_FAILED) (
    _In_ PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr,
    _In_ PBTHPS3_CONNECTION Connection
    );


typedef struct _BTHPS3_REPEAT_READER
{
    //
    // BRB used for transfer
    //
    struct _BRB_L2CA_ACL_TRANSFER TransferBrb;

    //
    // WDF Request used for pending I/O
    //
    WDFREQUEST RequestPendingRead;

    //
    // Data buffer for pending read
    //
    WDFMEMORY MemoryPendingRead;

    //
    // Dpc for resubmitting pending read
    //
    KDPC ResubmitDpc;

    //
    // Whether the continuous reader is transitioning to stopped state
    //

    LONG Stopping;

    //
    // Event used to wait for read completion while stopping the continuos reader
    //
    KEVENT StopEvent;

    //
    // Back pointer to connection
    //
    PBTHPS3_CONNECTION Connection;

} BTHPS3_REPEAT_READER, *PBTHPS3_REPEAT_READER;

//
// Connection state
//

typedef enum _BTHPS3_CONNECTION_STATE {
    ConnectionStateUnitialized = 0,
    ConnectionStateInitialized,
    ConnectionStateConnecting,
    ConnectionStateConnected,
    ConnectionStateConnectFailed,
    ConnectionStateDisconnecting,
    ConnectionStateDisconnected

} BTHPS3_CONNECTION_STATE, *PBTHPS3_CONNECTION_STATE;

typedef struct _BTHPS3_CONTINUOUS_READER {

    BTHPS3_REPEAT_READER
        RepeatReaders[BTHPS3_NUM_CONTINUOUS_READERS];

    PFN_BTHPS3_CONNECTION_OBJECT_CONTREADER_READ_COMPLETE
        BthEchoConnectionObjectContReaderReadCompleteCallback;

    PFN_BTHPS3_CONNECTION_OBJECT_CONTREADER_FAILED
        BthEchoConnectionObjectContReaderFailedCallback;

    DWORD                                   InitializedReadersCount;

} BTHPS3_CONTINUOUS_READER, *PBTHPS3_CONTINUOUS_READER;

//
// Connection data structure for L2Ca connection
//

typedef struct _BTHPS3_CONNECTION {

    //
    // List entry for connection list maintained at device level
    //
    LIST_ENTRY                              ConnectionListEntry;

    PBTHPS3_DEVICE_CONTEXT_HEADER    DevCtxHdr;

    BTHPS3_CONNECTION_STATE          ConnectionState;

    //
    // Connection lock, used to synchronize access to _BTHECHO_CONNECTION data structure
    //
    WDFSPINLOCK                             ConnectionLock;

    L2CAP_CHANNEL_HANDLE                    ChannelHandle;
    BTH_ADDR                                RemoteAddress;

    //
    // Preallocated Brb, Request used for connect/disconnect
    //
    struct _BRB                             ConnectDisconnectBrb;
    WDFREQUEST                              ConnectDisconnectRequest;

    //
    // Event used to wait for disconnection
    // It is non-signaled when connection is in ConnectionStateDisconnecting
    // transitionary state and signaled otherwise
    //
    KEVENT                                  DisconnectEvent;

    //
    // Continuous readers (used only by server)
    // PLEASE NOTE that KMDF USB Pipe Target uses a single continuous reader
    //
    BTHPS3_CONTINUOUS_READER               ContinuousReader;
} BTHPS3_CONNECTION, *PBTHPS3_CONNECTION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_CONNECTION, GetConnectionObjectContext)




/************************************************************************/
/* The new stuff                                                        */
/************************************************************************/

typedef struct _BTHPS3_CLIENT_L2CAP_CHANNEL
{
    BTHPS3_CONNECTION_STATE     ConnectionState;

    L2CAP_CHANNEL_HANDLE        ChannelHandle;

    struct _BRB                 ConnectDisconnectBrb;

    WDFREQUEST                  ConnectDisconnectRequest;

    KEVENT                      DisconnectEvent;

} BTHPS3_CLIENT_L2CAP_CHANNEL, *PBTHPS3_CLIENT_L2CAP_CHANNEL;

typedef struct _BTHPS3_CLIENT_CONNECTION
{
    PBTHPS3_DEVICE_CONTEXT_HEADER       DevCtxHdr;

    BTH_ADDR                            RemoteAddress;

    BTHPS3_CLIENT_L2CAP_CHANNEL         HidControlChannel;

    BTHPS3_CLIENT_L2CAP_CHANNEL         HidInterruptChannel;

} BTHPS3_CLIENT_CONNECTION, *PBTHPS3_CLIENT_CONNECTION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_CLIENT_CONNECTION, GetClientConnection)

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
ClientConnections_CreateAndInsert(
    _In_ PBTHPS3_SERVER_CONTEXT Context,
    _In_ BTH_ADDR RemoteAddress,
    _In_ PFN_WDF_OBJECT_CONTEXT_CLEANUP CleanupCallback,
    _Out_ PBTHPS3_CLIENT_CONNECTION *ClientConnection
);

VOID
ClientConnections_RemoveAndDestroy(
    _In_ PBTHPS3_SERVER_CONTEXT Context,
    _In_ PBTHPS3_CLIENT_CONNECTION ClientConnection
);

NTSTATUS
ClientConnections_RetrieveByBthAddr(
    _In_ PBTHPS3_SERVER_CONTEXT Context,
    _In_ BTH_ADDR RemoteAddress,
    _Out_ PBTHPS3_CLIENT_CONNECTION *ClientConnection
);

EVT_WDF_OBJECT_CONTEXT_CLEANUP EvtClientConnectionsDestroyConnection;

VOID
FORCEINLINE
CLIENT_CONNECTION_REQUEST_REUSE(
    _In_ WDFREQUEST Request
)
{
    NTSTATUS statusReuse;
    WDF_REQUEST_REUSE_PARAMS reuseParams;

    WDF_REQUEST_REUSE_PARAMS_INIT(&reuseParams, WDF_REQUEST_REUSE_NO_FLAGS, STATUS_NOT_SUPPORTED);
    statusReuse = WdfRequestReuse(Request, &reuseParams);
    NT_ASSERT(NT_SUCCESS(statusReuse));
    UNREFERENCED_PARAMETER(statusReuse);
}
