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

EVT_WDF_WORKITEM L2CAP_PS3_HandleRemoteConnectAsync;

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
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ReadControlTransferAsync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    WDFREQUEST Request,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ReadInterruptTransferAsync(
    _In_ PBTHPS3_CLIENT_CONNECTION ClientConnection,
    _In_ WDFREQUEST Request,
    _In_ PVOID Buffer,
    _In_ size_t BufferLength,
    _In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendInterruptTransferAsync(
    _In_ PBTHPS3_CLIENT_CONNECTION ClientConnection,
    _In_ WDFREQUEST Request,
    _In_ PVOID Buffer,
    _In_ size_t BufferLength,
    _In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
);

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
L2CAP_PS3_RemoteDisconnect(
    _In_ PBTHPS3_DEVICE_CONTEXT_HEADER CtxHdr,
    _In_ BTH_ADDR RemoteAddress,
    _In_ PBTHPS3_CLIENT_L2CAP_CHANNEL Channel
);

EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_ChannelDisconnectCompleted;

//
// HID Control Channel Completion Routines
// 
EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_AsyncReadControlTransferCompleted;
EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_AsyncSendControlTransferCompleted;

//
// HID Interrupt Channel Completion Routines
// 
EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_AsyncReadInterruptTransferCompleted;
EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_AsyncSendInterruptTransferCompleted;
