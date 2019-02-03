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

#define MAX_DEVICE_ID_LEN   200
#define BTH_ADDR_HEX_LEN    12

//
// Identification information for dynamically enumerated bus children (PDOs)
// 
typedef struct _PDO_IDENTIFICATION_DESCRIPTION
{
    //
    // Mandatory header
    // 
    WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER Header;

    //
    // MAC address identifying this device
    // 
    BTH_ADDR RemoteAddress;

    //
    // Type (make, model) of remote device
    // 
    DS_DEVICE_TYPE DeviceType;

} PDO_IDENTIFICATION_DESCRIPTION, *PPDO_IDENTIFICATION_DESCRIPTION;

//
// Context data of child device (PDO)
// 
typedef struct _BTHPS3_PDO_DEVICE_CONTEXT
{
    //
    // MAC address identifying this device
    // 
    BTH_ADDR RemoteAddress;

    //
    // Type (make, model) of remote device
    // 
    DS_DEVICE_TYPE DeviceType;

} BTHPS3_PDO_DEVICE_CONTEXT, *PBTHPS3_PDO_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_PDO_DEVICE_CONTEXT, GetPdoDeviceContext)

EVT_WDF_CHILD_LIST_CREATE_DEVICE BthPS3_EvtWdfChildListCreateDevice;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL BthPS3_PDO_EvtWdfIoQueueIoInternalDeviceControl;
