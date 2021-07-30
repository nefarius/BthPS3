/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2021, Nefarius Software Solutions e.U.                      *
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

    UNICODE_STRING						RemoteName;

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
    _In_ PBTHPS3_DEVICE_CONTEXT_HEADER Context,
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
