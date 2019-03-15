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

DEFINE_GUID(BTHPS3_SERVICE_GUID,
    0x1cb831ea, 0x79cd, 0x4508, 0xb0, 0xfc, 0x85, 0xf7, 0xc8, 0x5a, 0xe8, 0xe0);
// {1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}

//
// TODO: deprecated, use sideband device instead!
// 
DEFINE_GUID(GUID_DEVINTERFACE_BTHPS3PSM,
    0x1e1f8b68, 0xeaa2, 0x4d19, 0x8b, 0x02, 0xe8, 0xb0, 0x91, 0x6c, 0x77, 0xdb);
// {1e1f8b68-eaa2-4d19-8b02-e8b0916c77db}

extern __declspec(selectany) PCWSTR BthPS3FilterName = L"BthPS3PSM";
extern __declspec(selectany) PCSTR BthPS3FilterServiceName = "BthPS3PSM";
extern __declspec(selectany) PCWSTR BthPS3ServiceName = L"BthPS3Service";

// 0x11 -> 0x5053
#define PSM_DS3_HID_CONTROL     0x5053
// 0x13 -> 0x5055
#define PSM_DS3_HID_INTERRUPT   0x5055

#pragma region Bus children specifics

extern __declspec(selectany) PCWSTR BthPS3BusEnumeratorName = L"BTHPS3BUS";

// SIXAXIS/DualShock Hardware ID
// {53F88889-1AAF-4353-A047-556B69EC6DA6}
DEFINE_GUID(BTHPS3_BUSENUM_SIXAXIS,
    0x53f88889, 0x1aaf, 0x4353, 0xa0, 0x47, 0x55, 0x6b, 0x69, 0xec, 0x6d, 0xa6);

// Navigation Hardware ID
// {206F84FC-1615-4D9F-954D-21F5A5D388C5}
DEFINE_GUID(BTHPS3_BUSENUM_NAVIGATION,
    0x206f84fc, 0x1615, 0x4d9f, 0x95, 0x4d, 0x21, 0xf5, 0xa5, 0xd3, 0x88, 0xc5);

// Motion Hardware ID
// {84957238-D867-421F-89C1-67847A3B55B5}
DEFINE_GUID(BTHPS3_BUSENUM_MOTION,
    0x84957238, 0xd867, 0x421f, 0x89, 0xc1, 0x67, 0x84, 0x7a, 0x3b, 0x55, 0xb5);

// DualShock 4 Rev.1/2 Hardware ID
// {13D12A06-D0B0-4D7E-8D1F-F55914A2ED7C}
DEFINE_GUID(BTHPS3_BUSENUM_WIRELESS,
    0x13d12a06, 0xd0b0, 0x4d7e, 0x8d, 0x1f, 0xf5, 0x59, 0x14, 0xa2, 0xed, 0x7c);

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
    // PlayStation Move Navigation Controller
    // 
    DS_DEVICE_TYPE_NAVIGATION,

    //
    // PlayStation Move Motion Controller
    // 
    DS_DEVICE_TYPE_MOTION,

    //
    // DualShock 4 compatible
    // 
    DS_DEVICE_TYPE_WIRELESS

} DS_DEVICE_TYPE, *PDS_DEVICE_TYPE;

#define BTHPS3_SIXAXIS_HID_INPUT_REPORT_SIZE       0x32
#define DS3_HID_OUTPUT_REPORT_SIZE      0x32

#pragma region I/O Control section

#define FILE_DEVICE_BUSENUM             FILE_DEVICE_BUS_EXTENDER
#define BUSENUM_IOCTL(_index_)          CTL_CODE(FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_READ_DATA)
#define BUSENUM_W_IOCTL(_index_)        CTL_CODE(FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_WRITE_DATA)
#define BUSENUM_R_IOCTL(_index_)        CTL_CODE(FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_READ_DATA)
#define BUSENUM_RW_IOCTL(_index_)       CTL_CODE(FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_BTHPS3_BASE 0x801

// 
// I/O control codes for function-to-bus-driver communication
// 

#define IOCTL_BTHPS3_HID_CONTROL_READ           BUSENUM_R_IOCTL (IOCTL_BTHPS3_BASE + 0x200)
#define IOCTL_BTHPS3_HID_CONTROL_WRITE          BUSENUM_W_IOCTL (IOCTL_BTHPS3_BASE + 0x201)
#define IOCTL_BTHPS3_HID_INTERRUPT_READ         BUSENUM_R_IOCTL (IOCTL_BTHPS3_BASE + 0x202)
#define IOCTL_BTHPS3_HID_INTERRUPT_WRITE        BUSENUM_W_IOCTL (IOCTL_BTHPS3_BASE + 0x203)

#pragma endregion
