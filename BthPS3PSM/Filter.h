/**********************************************************************************
 *                                                                                *
 * BthPS3PSM - Windows kernel-mode Bluetooth lower filter driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2026, Nefarius Software Solutions e.U.                      *
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

#include <usb.h>
#include <bthxddi.h>
#include "L2CAP.h"

#define L2CAP_MIN_BUFFER_LEN    0x10

//
// Per-request context used to remember the (unchecked) output buffer
// pointer and length of a forwarded IOCTL_BTHX_READ_HCI request.
//
// The pointer/length are retrieved once at request dispatch time
// (PASSIVE_LEVEL, via WdfRequestRetrieveUnsafeUserOutputBuffer) since that
// API cannot be called again from the completion routine, which may run at
// a raised IRQL once the request comes back up from the BTHX transport
// stack.
// 
typedef struct _BTHX_READ_REQUEST_CONTEXT
{
    PVOID OutputBuffer;
    size_t OutputBufferLength;

} BTHX_READ_REQUEST_CONTEXT, *PBTHX_READ_REQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHX_READ_REQUEST_CONTEXT, BthxReadRequestGetContext)

EVT_WDF_REQUEST_COMPLETION_ROUTINE UrbSelectConfigurationCompleted;

EVT_WDF_REQUEST_COMPLETION_ROUTINE UrbFunctionBulkInTransferCompleted;

EVT_WDF_REQUEST_COMPLETION_ROUTINE BthxReadHciCompleted;

//
// Inspects an L2CAP buffer (as delivered via either the USB bulk-IN pipe or
// a BTHX ACL Data read) for an outgoing HID Control/Interrupt L2CAP
// Connection Request and, if enabled, patches the requested PSM to the
// artificial value the BthPS3 profile driver listens on.
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
BthPS3PSM_PatchL2capPsm(
    _In_ PDEVICE_CONTEXT DeviceContext,
    _In_reads_bytes_opt_(BufferLength) PUCHAR Buffer,
    _In_ ULONG BufferLength
);
