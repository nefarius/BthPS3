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
// Service GUID for BTHENUM to expose
// 
DEFINE_GUID(BTHPS3_SERVICE_GUID,
    0x1cb831ea, 0x79cd, 0x4508, 0xb0, 0xfc, 0x85, 0xf7, 0xc8, 0x5a, 0xe8, 0xe0);
// {1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}

//
// Filter device enumeration interface GUID
// 
DEFINE_GUID(GUID_DEVINTERFACE_BTHPS3PSM,
    0x1e1f8b68, 0xeaa2, 0x4d19, 0x8b, 0x02, 0xe8, 0xb0, 0x91, 0x6c, 0x77, 0xdb);
// {1e1f8b68-eaa2-4d19-8b02-e8b0916c77db}

//
// Compatible ID to load filter driver on
// 
#define BTHPS3PSM_FILTER_COMPATIBLE_ID      L"USB\\Class_E0&SubClass_01&Prot_01"

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
// Bus enumerator name
// 
extern __declspec(selectany) PCWSTR BthPS3BusEnumeratorName = L"BTHPS3BUS";

//
// Path to control device in user-land
// 
#define BTHPS3PSM_CONTROL_DEVICE_PATH         L"\\\\.\\BthPS3PSMControl"

//
// Path to control device exposed in kernel-land
// 
#define BTHPS3PSM_NTDEVICE_NAME_STRING        L"\\Device\\BthPS3PSMControl"
#define BTHPS3PSM_SYMBOLIC_NAME_STRING        L"\\DosDevices\\BthPS3PSMControl"

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

typedef struct _BTHPS3PSM_ENABLE_PSM_PATCHING
{
    IN ULONG DeviceIndex;

} BTHPS3PSM_ENABLE_PSM_PATCHING, *PBTHPS3PSM_ENABLE_PSM_PATCHING;

typedef struct _BTHPS3PSM_DISABLE_PSM_PATCHING
{
    IN ULONG DeviceIndex;

} BTHPS3PSM_DISABLE_PSM_PATCHING, *PBTHPS3PSM_DISABLE_PSM_PATCHING;

typedef struct _BTHPS3PSM_GET_PSM_PATCHING
{
    IN ULONG DeviceIndex;

    OUT ULONG IsEnabled;

} BTHPS3PSM_GET_PSM_PATCHING, *PBTHPS3PSM_GET_PSM_PATCHING;

#include <poppack.h>

#pragma endregion
