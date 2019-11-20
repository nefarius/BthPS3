/*
* BthPS3PSM - Windows kernel-mode BTHUSB lower filter driver
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


#include "BthPS3.h"

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

#pragma endregion

//
// Device context data
// 
typedef struct _DEVICE_CONTEXT
{
	//
	// Framework USB object
	// 
    WDFUSBDEVICE UsbDevice;

	//
	// USB interface object
	// 
    WDFUSBINTERFACE UsbInterface;

	//
	// USB Interrupt (in) pipe handle
	// 
    WDFUSBPIPE InterruptPipe;

	//
	// USB Bulk Read (in) handle
	// 
    WDFUSBPIPE BulkReadPipe;

	//
	// USB Bulk Write (out) handle
	// 
    WDFUSBPIPE BulkWritePipe;

	//
	// Patches PSM values if TRUE
	// 
    ULONG IsPsmPatchingEnabled;

	//
	// Symbolic link name of host radio we're loaded onto
	// 
    WDFSTRING SymbolicLinkName;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

//
// Function to initialize the device and its callbacks
//
NTSTATUS
BthPS3PSM_CreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

EVT_WDF_DEVICE_CONTEXT_CLEANUP BthPS3PSM_EvtDeviceContextCleanup;
EVT_WDF_DEVICE_PREPARE_HARDWARE BthPS3PSM_EvtDevicePrepareHardware;

EXTERN_C_END
