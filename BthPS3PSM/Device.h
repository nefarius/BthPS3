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
// Identifies the transport this filter instance is attached to
// 
typedef enum _BTHPS3PSM_TRANSPORT_TYPE
{
	//
	// Neither USB nor a recognized BTHX (extensible transport) device;
	// the filter will not create a device object for this stack.
	// 
	BthPS3PsmTransportUnsupported = 0,

	//
	// Bluetooth-class device running under the USB enumerator (BTHUSB.SYS
	// or a vendor equivalent). L2CAP traffic is intercepted via
	// IOCTL_INTERNAL_USB_SUBMIT_URB bulk-IN transfers.
	// 
	BthPS3PsmTransportUsb,

	//
	// Bluetooth-class device running on top of the Bluetooth Extensibility
	// Transport DDI (bthxddi.h), typically bound to Microsoft's inbox
	// BthMini.sys (e.g. PCIe/iBtPciBus or UART-attached radios). L2CAP
	// traffic is intercepted via IOCTL_BTHX_READ_HCI ACL data reads.
	// 
	BthPS3PsmTransportBthx

} BTHPS3PSM_TRANSPORT_TYPE;

//
// Device context data
// 
typedef struct _DEVICE_CONTEXT
{
	//
	// USB Bulk Read (in) handle; only valid if TransportType == BthPS3PsmTransportUsb
	// 
	USBD_PIPE_HANDLE BulkReadPipe;

	//
	// Transport this filter instance has been attached to
	// 
	BTHPS3PSM_TRANSPORT_TYPE TransportType;

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
BthPS3PSM_QueryTransportType(
	_In_ PWDFDEVICE_INIT DeviceInit,
	_Inout_ BTHPS3PSM_TRANSPORT_TYPE* TransportType
);

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
BthPS3PSM_IsBthxTransportDevice(
	_In_ PWDFDEVICE_INIT DeviceInit
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
