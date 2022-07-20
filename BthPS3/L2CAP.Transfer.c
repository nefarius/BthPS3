/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2022, Nefarius Software Solutions e.U.                      *
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
#include "L2CAP.Transfer.tmh"


//
// Submits an outgoing control request
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendControlTransferAsync(
    PBTHPS3_PDO_CONTEXT ClientConnection,
    WDFREQUEST Request,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;

    //
    // Allocate BRB
    // 
    brb = (struct _BRB_L2CA_ACL_TRANSFER*)
        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
            BRB_L2CA_ACL_TRANSFER,
            POOLTAG_BTHPS3
        );

    if (brb == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Used in completion routine to free BRB
    // 
    brb->Hdr.ClientContext[0] = ClientConnection->DevCtxHdr;

    //
    // Set channel properties
    // 
    brb->BtAddress = ClientConnection->RemoteAddress;
    brb->ChannelHandle = ClientConnection->HidControlChannel.ChannelHandle;
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_OUT;
    brb->BufferMDL = NULL;
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbAsync(
        ClientConnection->DevCtxHdr->IoTarget,
        Request,
        (PBRB)brb,
        sizeof(*brb),
        CompletionRoutine,
        brb
    );

    if (!NT_SUCCESS(status))
    {
        TraceError( 
            TRACE_L2CAP,
            "BthPS3_SendBrbAsync failed with status %!STATUS!", 
            status
        );

        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    }

    return status;
}

//
// Submits an incoming control request
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ReadControlTransferAsync(
    PBTHPS3_PDO_CONTEXT ClientConnection,
    WDFREQUEST Request,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;

    //
    // Allocate BRB
    // 
    brb = (struct _BRB_L2CA_ACL_TRANSFER*)
        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
            BRB_L2CA_ACL_TRANSFER,
            POOLTAG_BTHPS3
        );

    if (brb == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Used in completion routine to free BRB
    // 
    brb->Hdr.ClientContext[0] = ClientConnection->DevCtxHdr;

    //
    // Set channel properties
    // 
    brb->BtAddress = ClientConnection->RemoteAddress;
    brb->ChannelHandle = ClientConnection->HidControlChannel.ChannelHandle;
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_IN | ACL_SHORT_TRANSFER_OK;
    brb->BufferMDL = NULL;
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbAsync(
        ClientConnection->DevCtxHdr->IoTarget,
        Request,
        (PBRB)brb,
        sizeof(*brb),
        CompletionRoutine,
        brb
    );

    if (!NT_SUCCESS(status))
    {
        TraceError( 
            TRACE_L2CAP,
            "BthPS3_SendBrbAsync failed with status %!STATUS!", 
            status
        );

        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    }

    return status;
}

//
// Submits an incoming interrupt request
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ReadInterruptTransferAsync(
    _In_ PBTHPS3_PDO_CONTEXT ClientConnection,
    _In_ WDFREQUEST Request,
    _In_ PVOID Buffer,
    _In_ size_t BufferLength,
    _In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;

    //
    // Allocate BRB
    // 
    brb = (struct _BRB_L2CA_ACL_TRANSFER*)
        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
            BRB_L2CA_ACL_TRANSFER,
            POOLTAG_BTHPS3
        );

    if (brb == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Used in completion routine to free BRB
    // 
    brb->Hdr.ClientContext[0] = ClientConnection->DevCtxHdr;

    //
    // Set channel properties
    // 
    brb->BtAddress = ClientConnection->RemoteAddress;
    brb->ChannelHandle = ClientConnection->HidInterruptChannel.ChannelHandle;
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_IN;
    brb->BufferMDL = NULL;
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbAsync(
        ClientConnection->DevCtxHdr->IoTarget,
        Request,
        (PBRB)brb,
        sizeof(*brb),
        CompletionRoutine,
        brb
    );

    if (!NT_SUCCESS(status))
    {
        TraceError( 
            TRACE_L2CAP,
            "BthPS3_SendBrbAsync failed with status %!STATUS!",
            status
        );

        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    }

    return status;
}

//
// Submits an outgoing interrupt transfer
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendInterruptTransferAsync(
    _In_ PBTHPS3_PDO_CONTEXT ClientConnection,
    _In_ WDFREQUEST Request,
    _In_ PVOID Buffer,
    _In_ size_t BufferLength,
    _In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;

    //
    // Allocate BRB
    // 
    brb = (struct _BRB_L2CA_ACL_TRANSFER*)
        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
            BRB_L2CA_ACL_TRANSFER,
            POOLTAG_BTHPS3
        );

    if (brb == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Used in completion routine to free BRB
    // 
    brb->Hdr.ClientContext[0] = ClientConnection->DevCtxHdr;

    //
    // Set channel properties
    // 
    brb->BtAddress = ClientConnection->RemoteAddress;
    brb->ChannelHandle = ClientConnection->HidInterruptChannel.ChannelHandle;
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_OUT;
    brb->BufferMDL = NULL;
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;
    brb->Timeout = 0;
    brb->RemainingBufferSize = 0;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbAsync(
        ClientConnection->DevCtxHdr->IoTarget,
        Request,
        (PBRB)brb,
        sizeof(*brb),
        CompletionRoutine,
        brb
    );

    if (!NT_SUCCESS(status))
    {
        TraceError( 
            TRACE_L2CAP,
            "BthPS3_SendBrbAsync failed with status %!STATUS!", 
            status
        );

        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    }

    return status;
}

//
// Outgoing control transfer has been completed
// 
void
L2CAP_PS3_AsyncSendControlTransferCompleted(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
)
{
    struct _BRB_L2CA_ACL_TRANSFER* brb =
        (struct _BRB_L2CA_ACL_TRANSFER*)Context;
    PBTHPS3_DEVICE_CONTEXT_HEADER deviceCtxHdr =
        (PBTHPS3_DEVICE_CONTEXT_HEADER)brb->Hdr.ClientContext[0];

    UNREFERENCED_PARAMETER(Target);

    TraceVerbose(
        TRACE_L2CAP,
        "Control transfer request completed with status %!STATUS!",
        Params->IoStatus.Status
    );

    deviceCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfRequestComplete(Request, Params->IoStatus.Status);
}

//
// Incoming control transfer has been completed
// 
void
L2CAP_PS3_AsyncReadControlTransferCompleted(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
)
{
    size_t length = 0;
    struct _BRB_L2CA_ACL_TRANSFER* brb =
        (struct _BRB_L2CA_ACL_TRANSFER*)Context;
    PBTHPS3_DEVICE_CONTEXT_HEADER deviceCtxHdr =
        (PBTHPS3_DEVICE_CONTEXT_HEADER)brb->Hdr.ClientContext[0];

    UNREFERENCED_PARAMETER(Target);

    TraceVerbose(
        TRACE_L2CAP,
        "Control read transfer request completed with status %!STATUS!",
        Params->IoStatus.Status
    );

    length = brb->BufferSize;
    deviceCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfRequestCompleteWithInformation(
        Request,
        Params->IoStatus.Status,
        length
    );
}

//
// Incoming interrupt transfer has been completed
// 
void
L2CAP_PS3_AsyncReadInterruptTransferCompleted(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
)
{
    size_t length = 0;
    struct _BRB_L2CA_ACL_TRANSFER* brb =
        (struct _BRB_L2CA_ACL_TRANSFER*)Context;
    PBTHPS3_DEVICE_CONTEXT_HEADER deviceCtxHdr =
        (PBTHPS3_DEVICE_CONTEXT_HEADER)brb->Hdr.ClientContext[0];

    UNREFERENCED_PARAMETER(Target);

    TraceVerbose(
        TRACE_L2CAP,
        "Interrupt read transfer request completed with status %!STATUS! (remaining: %d)",
        Params->IoStatus.Status,
        brb->RemainingBufferSize
    );

    length = brb->BufferSize;
    deviceCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfRequestCompleteWithInformation(
        Request,
        Params->IoStatus.Status,
        length
    );
}

//
// Outgoing interrupt transfer has been completed
// 
void
L2CAP_PS3_AsyncSendInterruptTransferCompleted(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
)
{
    struct _BRB_L2CA_ACL_TRANSFER* brb =
        (struct _BRB_L2CA_ACL_TRANSFER*)Context;
    PBTHPS3_DEVICE_CONTEXT_HEADER deviceCtxHdr =
        (PBTHPS3_DEVICE_CONTEXT_HEADER)brb->Hdr.ClientContext[0];

    UNREFERENCED_PARAMETER(Target);

    TraceVerbose(
        TRACE_L2CAP,
        "Interrupt OUT transfer request completed with status %!STATUS!",
        Params->IoStatus.Status
    );

    deviceCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfRequestComplete(Request, Params->IoStatus.Status);
}
