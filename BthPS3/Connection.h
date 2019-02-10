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


#pragma once

#include <ntstrsafe.h>


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
