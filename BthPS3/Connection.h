/*
* BthPS3 - Windows kernel-mode Bluetooth profile and bus driver
*
* MIT License
*
* Copyright (C) 2018-2019  Nefarius Software Solutions e.U. and Contributors
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/


#pragma once

#include <ntstrsafe.h>


//
// Connection state
//
typedef enum _BTHPS3_CONNECTION_STATE {
    ConnectionStateUninitialized = 0,
    ConnectionStateInitialized,
    ConnectionStateConnecting,
    ConnectionStateConnected,
    ConnectionStateConnectFailed,
    ConnectionStateDisconnecting,
    ConnectionStateDisconnected

} BTHPS3_CONNECTION_STATE, *PBTHPS3_CONNECTION_STATE;


//
// State information for a single L2CAP channel
// 
typedef struct _BTHPS3_CLIENT_L2CAP_CHANNEL
{
    BTHPS3_CONNECTION_STATE     ConnectionState;

    WDFSPINLOCK                 ConnectionStateLock;

    L2CAP_CHANNEL_HANDLE        ChannelHandle;

    struct _BRB                 ConnectDisconnectBrb;

    WDFREQUEST                  ConnectDisconnectRequest;

    KEVENT                      DisconnectEvent;

} BTHPS3_CLIENT_L2CAP_CHANNEL, *PBTHPS3_CLIENT_L2CAP_CHANNEL;

//
// State information for a remote device
// 
typedef struct _BTHPS3_CLIENT_CONNECTION
{
    PBTHPS3_DEVICE_CONTEXT_HEADER       DevCtxHdr;

    BTH_ADDR                            RemoteAddress;

    DS_DEVICE_TYPE                      DeviceType;

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
