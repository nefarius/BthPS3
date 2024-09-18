/**********************************************************************************
 *                                                                                *
 * BthPS3PSM - Windows kernel-mode BTHUSB lower filter driver                     *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2024, Nefarius Software Solutions e.U.                      *
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

#include "BthPS3.h"
#include <usb.h>

EXTERN_C_START

#pragma region Registry key/value names

//
// Filter will be auto-enabled on device boot if value > 0
// 
#define G_PatchPSMRegValue  L"BthPS3PSMPatchEnabled"

//
// Symbolic link name of the radio the filter is currently loaded on
// 
#define G_SymbolicLinkName  L"SymbolicLinkName"

#define MAX_DEVICE_ID_LEN   200

#pragma endregion

//
// Device context data
// 
typedef struct _DEVICE_CONTEXT
{
	//
	// USB Bulk Read (in) handle
	// 
	USBD_PIPE_HANDLE BulkReadPipe;

	//
	// Patches PSM values if TRUE
	// 
	ULONG IsPsmPatchingEnabled;

	//
	// Symbolic link name of host radio we're loaded onto
	// 
	WDFSTRING SymbolicLinkName;

    //
    // Device Instance ID
    // 
    WDFMEMORY InstanceId;

    //
    // Registry handle to device node
    // 
    WDFKEY RegKeyDeviceNode;

} DEVICE_CONTEXT, * PDEVICE_CONTEXT;

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

//
// Function to initialize the device and its callbacks
//
_Success_(return == STATUS_SUCCESS)
_Must_inspect_result_
NTSTATUS
BthPS3PSM_CreateDevice(
	_Inout_ PWDFDEVICE_INIT DeviceInit
);

EVT_WDF_DEVICE_CONTEXT_CLEANUP BthPS3PSM_EvtDeviceContextCleanup;

_Success_(return == STATUS_SUCCESS)
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3PSM_IsBthUsbDevice(
	_In_ PWDFDEVICE_INIT DeviceInit,
	_Inout_opt_ PBOOLEAN Result
);

_Success_(return == STATUS_SUCCESS)
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3PSM_GetPropertyInstanceId(
	_In_ PWDFDEVICE_INIT DeviceInit,
	_Inout_ WDFMEMORY * Memory
);

EXTERN_C_END
