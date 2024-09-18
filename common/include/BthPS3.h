/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
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

//
// Vendor & Product IDs
// 

static USHORT BTHPS3_SIXAXIS_VID = 0x054C;
static USHORT BTHPS3_SIXAXIS_PID = 0x0268;

static USHORT BTHPS3_NAVIGATION_VID	= 0x054C;
static USHORT BTHPS3_NAVIGATION_PID	= 0x042F;

static USHORT BTHPS3_MOTION_VID = 0x054C;
static USHORT BTHPS3_MOTION_PID = 0x03d5;

static USHORT BTHPS3_WIRELESS_VID = 0x054C;
static USHORT BTHPS3_WIRELESS_PID = 0x05C4;

//
// Service GUID for BTHENUM to expose
// 
DEFINE_GUID(BTHPS3_SERVICE_GUID,
    0x1cb831ea, 0x79cd, 0x4508, 0xb0, 0xfc, 0x85, 0xf7, 0xc8, 0x5a, 0xe8, 0xe0);
// {1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}

// {968E1849-73B1-4876-B80A-ED6DD171489B}
DEFINE_GUID(GUID_DEVINTERFACE_BTHPS3, 
	0x968e1849, 0x73b1, 0x4876, 0xb8, 0xa, 0xed, 0x6d, 0xd1, 0x71, 0x48, 0x9b);

//
// Filter device enumeration interface GUID
// 
DEFINE_GUID(GUID_DEVINTERFACE_BTHPS3PSM,
    0x1e1f8b68, 0xeaa2, 0x4d19, 0x8b, 0x02, 0xe8, 0xb0, 0x91, 0x6c, 0x77, 0xdb);
// {1e1f8b68-eaa2-4d19-8b02-e8b0916c77db}

//
// Hardware ID of virtual filter device
// 
#define BTHPS3PSM_FILTER_HARDWARE_ID        L"Nefarius\\{a3dc6d41-9e10-46d9-8be2-9b4a279841df}"

extern __declspec(selectany) PCWSTR BthPS3FilterName = L"BthPS3PSM";
extern __declspec(selectany) PCSTR BthPS3FilterServiceName = "BthPS3PSM";
extern __declspec(selectany) PCWSTR BthPS3ServiceName = L"BthPS3Service";

// 
// Artificial HID Control PSM (0x11 -> 0x5053)
// 
#define PSM_DS3_HID_CONTROL     0x5053
// 
// Artificial HID Interrupt PSM (0x13 -> 0x5055)
// 
#define PSM_DS3_HID_INTERRUPT   0x5055

//
// Lowest HCI major version to support
// 
#define BTHPS3_MIN_SUPPORTED_HCI_MAJOR_VERSION      0x03 // Bluetooth 2.0 + EDR
/*
 * Link Manager Versions
 * 
 * | LMP | Bluetooth Version   |
 * | --- | ------------------- |
 * | 0   | Bluetooth 1.0b      |
 * | 1   | Bluetooth 1.1       |
 * | 2   | Bluetooth 1.2       |
 * | 3   | Bluetooth 2.0 + EDR |
 * | 4   | Bluetooth 2.1 + EDR |
 * | 5   | Bluetooth 3.0 + HS  |
 * | 6   | Bluetooth 4.0       |
 * | 7   | Bluetooth 4.1       |
 * | 8   | Bluetooth 4.2       |
 * | 9   | Bluetooth 5         |
 * | 10  | Bluetooth 5.1       |
 * | 11  | Bluetooth 5.2       |
 * 
 */

//
// Bus enumerator name
// 
extern __declspec(selectany) PCWSTR BthPS3BusEnumeratorName = L"BTHPS3BUS";

//
// Path to control device in user-land
//
// Elevated (or kernel) access by default only
// 
#define BTHPS3PSM_CONTROL_DEVICE_PATH         L"\\\\.\\BthPS3PSMControl"

//
// Path to control device exposed in kernel-land
// 
#define BTHPS3PSM_NTDEVICE_NAME_STRING        L"\\Device\\BthPS3PSMControl"
#define BTHPS3PSM_SYMBOLIC_NAME_STRING        L"\\DosDevices\\BthPS3PSMControl"

//
// Host address the device is currently paired to, if supported
// 
// NOTE: this is a system-included property, but not defined in the WDK
// 
// {a92f26ca-eda7-4b1d-9db2-27b68aa5a2eb}
DEFINE_DEVPROPKEY(DEVPKEY_BluetoothRadio_Address,
    0xa92f26ca, 0xeda7, 0x4b1d, 0x9d, 0xb2, 0x27, 0xb6, 0x8a, 0xa5, 0xa2, 0xeb,
    1
); // DEVPROP_TYPE_UINT64

#pragma region PDO Hardware IDs

// SIXAXIS/DualShock Hardware ID
DEFINE_GUID(GUID_BUSENUM_BTHPS3_SIXAXIS,
    0x53f88889, 0x1aaf, 0x4353, 0xa0, 0x47, 0x55, 0x6b, 0x69, 0xec, 0x6d, 0xa6);
// {53F88889-1AAF-4353-A047-556B69EC6DA6}

// Navigation Hardware ID
DEFINE_GUID(GUID_BUSENUM_BTHPS3_NAVIGATION,
    0x206f84fc, 0x1615, 0x4d9f, 0x95, 0x4d, 0x21, 0xf5, 0xa5, 0xd3, 0x88, 0xc5);
// {206F84FC-1615-4D9F-954D-21F5A5D388C5}

// Motion Hardware ID
DEFINE_GUID(GUID_BUSENUM_BTHPS3_MOTION,
    0x84957238, 0xd867, 0x421f, 0x89, 0xc1, 0x67, 0x84, 0x7a, 0x3b, 0x55, 0xb5);
// {84957238-D867-421F-89C1-67847A3B55B5}

// DualShock 4 Rev.1/2 Hardware ID
DEFINE_GUID(GUID_BUSENUM_BTHPS3_WIRELESS,
    0x13d12a06, 0xd0b0, 0x4d7e, 0x8d, 0x1f, 0xf5, 0x59, 0x14, 0xa2, 0xed, 0x7c);
// {13D12A06-D0B0-4D7E-8D1F-F55914A2ED7C}

#pragma endregion

#pragma region Raw PDO Device Class GUIDs

// SIXAXIS/DualShock Device Class GUID
DEFINE_GUID(GUID_DEVCLASS_BTHPS3_SIXAXIS,
    0x2ffad411, 0x8a38, 0x4a36, 0x95, 0x7a, 0xc2, 0xe2, 0xd7, 0x69, 0xbe, 0x62);
// {2FFAD411-8A38-4A36-957A-C2E2D769BE62}

// Navigation Device Class GUID
DEFINE_GUID(GUID_DEVCLASS_BTHPS3_NAVIGATION,
    0x2ffad411, 0x8a38, 0x4a36, 0x95, 0x7a, 0xc2, 0xe2, 0xd7, 0x69, 0xbe, 0x62);
// {2FFAD411-8A38-4A36-957A-C2E2D769BE62}

// Motion Device Class GUID
DEFINE_GUID(GUID_DEVCLASS_BTHPS3_MOTION,
    0x2ffad411, 0x8a38, 0x4a36, 0x95, 0x7a, 0xc2, 0xe2, 0xd7, 0x69, 0xbe, 0x62);
// {2FFAD411-8A38-4A36-957A-C2E2D769BE62}

// DualShock 4 Rev.1/2 Device Class GUID
DEFINE_GUID(GUID_DEVCLASS_BTHPS3_WIRELESS,
    0x2ffad411, 0x8a38, 0x4a36, 0x95, 0x7a, 0xc2, 0xe2, 0xd7, 0x69, 0xbe, 0x62);
// {2FFAD411-8A38-4A36-957A-C2E2D769BE62}

#pragma endregion

#pragma region PDO Device Interface GUIDs

// SIXAXIS/DualShock Device Interface GUID
DEFINE_GUID(GUID_DEVINTERFACE_BTHPS3_SIXAXIS,
    0x7b0eae3d, 0x4414, 0x4024, 0xbc, 0xbd, 0x1c, 0x21, 0x52, 0x37, 0x68, 0xce);
// {7B0EAE3D-4414-4024-BCBD-1C21523768CE}

// Navigation Device Interface GUID
DEFINE_GUID(GUID_DEVINTERFACE_BTHPS3_NAVIGATION,
    0x3e53723a, 0x440c, 0x40af, 0x88, 0x95, 0xea, 0x43, 0x9d, 0x75, 0xe7, 0xbe);
// {3E53723A-440C-40AF-8895-EA439D75E7BE}

// Motion Device Interface GUID
DEFINE_GUID(GUID_DEVINTERFACE_BTHPS3_MOTION,
    0xbcec605d, 0x233c, 0x4bef, 0x9a, 0x10, 0xf2, 0xb8, 0x1b, 0x52, 0x97, 0xf6);
// {BCEC605D-233C-4BEF-9A10-F2B81B5297F6}

// DualShock 4 Rev.1/2 Device Interface GUID
DEFINE_GUID(GUID_DEVINTERFACE_BTHPS3_WIRELESS,
    0x64cb1ee2, 0xb428, 0x4ce8, 0x87, 0x94, 0xf6, 0x80, 0x36, 0xe5, 0x7b, 0xe5);
// {64CB1EE2-B428-4CE8-8794-F68036E57BE5}

#pragma endregion

#pragma region Registry constants

/***********************************************************/
/* HKLM\SYSTEM\CurrentControlSet\Services\BthPS3\Parameter */
/***********************************************************/

//
// Bring up exposed device in raw mode (no function driver required)
// 
#define BTHPS3_REG_VALUE_RAW_PDO                L"RawPDO"

//
// Hide driver-less device in Device Manager UI (cosmetic setting)
// 
#define BTHPS3_REG_VALUE_HIDE_PDO               L"HidePDO"

//
// Restrict access to RAW PDO device to elevated processes only
// 
#define BTHPS3_REG_VALUE_ADMIN_ONLY_PDO			L"AdminOnlyPDO"

//
// Restrict access to RAW PDO device to one handle only
// 
#define BTHPS3_REG_VALUE_EXCLUSIVE_PDO			L"ExclusivePDO"

//
// I/O idle timeout value in milliseconds
// 
#define BTHPS3_REG_VALUE_CHILD_IDLE_TIMEOUT     L"ChildIdleTimeout"

//
// Should the profile driver attempt to auto-enable the patch again
// 
#define BTHPS3_REG_VALUE_AUTO_ENABLE_FILTER         L"AutoEnableFilter"

//
// Should the profile driver attempt to auto-disable the patch
// 
#define BTHPS3_REG_VALUE_AUTO_DISABLE_FILTER        L"AutoDisableFilter"

//
// Time (in seconds) to wait for patch re-enable
// 
#define BTHPS3_REG_VALUE_AUTO_ENABLE_FILTER_DELAY   L"AutoEnableFilterDelay"


//
// SIXAXIS connection requests will be dropped, if FALSE
// 
#define BTHPS3_REG_VALUE_IS_SIXAXIS_SUPPORTED           L"IsSIXAXISSupported"

//
// NAVIGATION connection requests will be dropped, if FALSE
// 
#define BTHPS3_REG_VALUE_IS_NAVIGATION_SUPPORTED        L"IsNAVIGATIONSupported"

//
// MOTION connection requests will be dropped, if FALSE
// 
#define BTHPS3_REG_VALUE_IS_MOTION_SUPPORTED            L"IsMOTIONSupported"

//
// WIRELESS connection requests will be dropped, if FALSE
// 
#define BTHPS3_REG_VALUE_IS_WIRELESS_SUPPORTED          L"IsWIRELESSSupported"


//
// Collection of supported remote names for SIXAXIS device
// 
#define BTHPS3_REG_VALUE_SIXAXIS_SUPPORTED_NAMES        L"SIXAXISSupportedNames"

//
// Collection of supported remote names for NAVIGATION device
// 
#define BTHPS3_REG_VALUE_NAVIGATION_SUPPORTED_NAMES     L"NAVIGATIONSupportedNames"

//
// Collection of supported remote names for MOTION device
// 
#define BTHPS3_REG_VALUE_MOTION_SUPPORTED_NAMES         L"MOTIONSupportedNames"

//
// Collection of supported remote names for WIRELESS device
// 
#define BTHPS3_REG_VALUE_WIRELESS_SUPPORTED_NAMES       L"WIRELESSSupportedNames"


//
// Occupied slots information
// 
#define BTHPS3_REG_VALUE_SLOTS      L"Slots"

//
// Remote devices' pre-assigned serial/slot/index
// 
#define BTHPS3_REG_VALUE_SLOT_NO    L"SlotNo"

#pragma endregion

//
// Type (make, model) of remote device
// 
typedef enum _DS_DEVICE_TYPE
{
    //
    // Unknown device type
    // 
    DS_DEVICE_TYPE_UNKNOWN = 0,

    //
    // SIXAXIS/DualShock 3 compatible
    // 
    DS_DEVICE_TYPE_SIXAXIS,

    //
    // PlayStation(R) Move Navigation Controller
    // 
    DS_DEVICE_TYPE_NAVIGATION,

    //
    // PlayStation(R) Move Motion Controller
    // 
    DS_DEVICE_TYPE_MOTION,

    //
    // DualShock 4 compatible
    // 
    DS_DEVICE_TYPE_WIRELESS

} DS_DEVICE_TYPE, *PDS_DEVICE_TYPE;

#define BTHPS3_SIXAXIS_HID_INPUT_REPORT_SIZE        0x32
#define BTHPS3_SIXAXIS_HID_OUTPUT_REPORT_SIZE       0x32

//
// Maximum Hardware/Device ID length limitation
// 
#define BTHPS3_MAX_DEVICE_ID_LEN   0xC8 // 200 characters

#pragma region I/O Control section

#define FILE_DEVICE_BUSENUM             FILE_DEVICE_BUS_EXTENDER
#define BUSENUM_IOCTL(_index_)          CTL_CODE(FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_READ_DATA)
#define BUSENUM_W_IOCTL(_index_)        CTL_CODE(FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_WRITE_DATA)
#define BUSENUM_R_IOCTL(_index_)        CTL_CODE(FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_READ_DATA)
#define BUSENUM_RW_IOCTL(_index_)       CTL_CODE(FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_BTHPS3_BASE 0x801

/**************************************************************/
/* I/O control codes for function-to-bus-driver communication */
/**************************************************************/

// 
// Read from control channel
// 
#define IOCTL_BTHPS3_HID_CONTROL_READ           BUSENUM_R_IOCTL (IOCTL_BTHPS3_BASE + 0x200)

// 
// Write to control channel
// 
#define IOCTL_BTHPS3_HID_CONTROL_WRITE          BUSENUM_W_IOCTL (IOCTL_BTHPS3_BASE + 0x201)

// 
// Read from interrupt channel
// 
#define IOCTL_BTHPS3_HID_INTERRUPT_READ         BUSENUM_R_IOCTL (IOCTL_BTHPS3_BASE + 0x202)

// 
// Write to interrupt channel
// 
#define IOCTL_BTHPS3_HID_INTERRUPT_WRITE        BUSENUM_W_IOCTL (IOCTL_BTHPS3_BASE + 0x203)


/*************************************************************/
/* I/O control codes for filter control device communication */
/*************************************************************/

//
// Enable PSM patch for a supplied device index
// 
#define IOCTL_BTHPS3PSM_ENABLE_PSM_PATCHING     BUSENUM_W_IOCTL (IOCTL_BTHPS3_BASE + 0x300)

//
// Disable PSM patch for a supplied device index
// 
#define IOCTL_BTHPS3PSM_DISABLE_PSM_PATCHING    BUSENUM_W_IOCTL (IOCTL_BTHPS3_BASE + 0x301)

//
// Retrieve current PSM patch state for a supplied device index
// 
#define IOCTL_BTHPS3PSM_GET_PSM_PATCHING        BUSENUM_R_IOCTL (IOCTL_BTHPS3_BASE + 0x302)

#include <pshpack1.h>

//
// Payload for IOCTL_BTHPS3PSM_ENABLE_PSM_PATCHING
// 
typedef struct _BTHPS3PSM_ENABLE_PSM_PATCHING
{
    IN ULONG DeviceIndex;

} BTHPS3PSM_ENABLE_PSM_PATCHING, *PBTHPS3PSM_ENABLE_PSM_PATCHING;

//
// Payload for IOCTL_BTHPS3PSM_DISABLE_PSM_PATCHING
// 
typedef struct _BTHPS3PSM_DISABLE_PSM_PATCHING
{
    IN ULONG DeviceIndex;

} BTHPS3PSM_DISABLE_PSM_PATCHING, *PBTHPS3PSM_DISABLE_PSM_PATCHING;

//
// Payload for IOCTL_BTHPS3PSM_GET_PSM_PATCHING
// 
typedef struct _BTHPS3PSM_GET_PSM_PATCHING
{
    IN ULONG DeviceIndex;

    OUT ULONG IsEnabled;

    OUT WCHAR SymbolicLinkName[BTHPS3_MAX_DEVICE_ID_LEN];

} BTHPS3PSM_GET_PSM_PATCHING, *PBTHPS3PSM_GET_PSM_PATCHING;

#include <poppack.h>

#pragma endregion
