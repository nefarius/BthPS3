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


_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_HandleRemoteConnect(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx,
    _In_ PINDICATION_PARAMETERS ConnectParams
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_DenyRemoteConnect(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx,
    _In_ PINDICATION_PARAMETERS ConnectParams
);

_IRQL_requires_max_(DISPATCH_LEVEL)
void
L2CAP_PS3_ConnectionIndicationCallback(
    _In_ PVOID Context,
    _In_ INDICATION_CODE Indication,
    _In_ PINDICATION_PARAMETERS Parameters
);

EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_DenyRemoteConnectCompleted;

EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_ControlConnectResponseCompleted;
EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_InterruptConnectResponseCompleted;

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ConnectionStateConnected(
    PBTHPS3_CLIENT_CONNECTION ClientConnection
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendControlTransferAsync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    WDFREQUEST Request,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine,
    PVOID CompletionContext
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendControlTransferSync(
    _In_ PBTHPS3_CLIENT_CONNECTION ClientConnection,
    _In_ PVOID Buffer,
    _In_ size_t BufferLength
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendInterruptTransferSync(
    _In_ PBTHPS3_CLIENT_CONNECTION ClientConnection,
    _In_ PVOID Buffer,
    _In_ size_t BufferLength
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ReadInterruptTransferSync(
    _In_ PBTHPS3_CLIENT_CONNECTION ClientConnection,
    _In_ PVOID Buffer,
    _In_ size_t BufferLength,
    _Out_ PULONG_PTR BytesReturned
);

EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_ControlTransferCompleted;
EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_AsyncControlTransferCompleted;
