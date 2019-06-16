/*
* BthPS3PSM - Windows kernel-mode BTHUSB lower filter driver
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


#include "BthPS3.h"

EXTERN_C_START

#define G_PatchPSMRegValue  L"BthPS3PSMPatchEnabled"
#define G_SymbolicLinkName  L"SymbolicLinkName"

//
// The device context performs the same job as
// a WDM device extension in the driver frameworks
//
typedef struct _DEVICE_CONTEXT
{
    BOOLEAN IsDummyDevice;

    WDFUSBDEVICE UsbDevice;

    WDFUSBINTERFACE UsbInterface;

    WDFUSBPIPE InterruptPipe;

    WDFUSBPIPE BulkReadPipe;

    WDFUSBPIPE BulkWritePipe;

    ULONG IsPsmPatchingEnabled;

    BOOLEAN IsCompatible;

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
