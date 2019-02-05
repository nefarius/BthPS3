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

//
// TODO: remove/port over to function driver
// 
#define DS3_HID_OUTPUT_REPORT_SIZE      0x32 // 50 bytes
extern CONST UCHAR G_Ds3HidOutputReport[];


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

EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_ConnectResponseCompleted;
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
L2CAP_PS3_SendControlTransfer(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendControlTransferAsync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine,
    PVOID CompletionContext
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendControlTransferSync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    PVOID Buffer,
    size_t BufferLength
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendInterruptTransferSync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    PVOID Buffer,
    size_t BufferLength
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ReadInterruptTransferAsync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine,
    WDFCONTEXT CompletionContext
);

EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_ReadInterruptTransferCompleted;

EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_ControlTransferCompleted;
