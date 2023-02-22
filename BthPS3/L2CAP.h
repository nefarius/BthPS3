/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2023, Nefarius Software Solutions e.U.                      *
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


typedef struct _BTHPS3_PDO_CONTEXT              *PBTHPS3_PDO_CONTEXT;
typedef struct _BTHPS3_CLIENT_L2CAP_CHANNEL     *PBTHPS3_CLIENT_L2CAP_CHANNEL;

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
L2CAP_PS3_HandleRemoteConnect(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx,
    _In_ PINDICATION_PARAMETERS ConnectParams
);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
L2CAP_PS3_HandleRemoteDisconnect(
    _In_ PBTHPS3_PDO_CONTEXT Context,
    _In_ PINDICATION_PARAMETERS DisconnectParams
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
L2CAP_PS3_SendControlTransferAsync(
    PBTHPS3_PDO_CONTEXT ClientConnection,
    WDFREQUEST Request,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ReadControlTransferAsync(
    PBTHPS3_PDO_CONTEXT ClientConnection,
    WDFREQUEST Request,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ReadInterruptTransferAsync(
    _In_ PBTHPS3_PDO_CONTEXT ClientConnection,
    _In_ WDFREQUEST Request,
    _In_ PVOID Buffer,
    _In_ size_t BufferLength,
    _In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendInterruptTransferAsync(
    _In_ PBTHPS3_PDO_CONTEXT ClientConnection,
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
