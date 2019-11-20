/*
* BthPS3 - Windows kernel-mode Bluetooth profile and bus driver
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
    // Client connection context
    // 
    PBTHPS3_CLIENT_CONNECTION ClientConnection;

} PDO_IDENTIFICATION_DESCRIPTION, *PPDO_IDENTIFICATION_DESCRIPTION;

//
// Context data of child device (PDO)
// 
typedef struct _BTHPS3_PDO_DEVICE_CONTEXT
{
    //
    // Client connection context
    // 
    PBTHPS3_CLIENT_CONNECTION ClientConnection;

} BTHPS3_PDO_DEVICE_CONTEXT, *PBTHPS3_PDO_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_PDO_DEVICE_CONTEXT, GetPdoDeviceContext)

EVT_WDF_CHILD_LIST_CREATE_DEVICE BthPS3_EvtWdfChildListCreateDevice;
EVT_WDF_CHILD_LIST_IDENTIFICATION_DESCRIPTION_COMPARE BthPS3_PDO_EvtChildListIdentificationDescriptionCompare;

EVT_WDF_DEVICE_CONTEXT_CLEANUP BthPS3_PDO_EvtDeviceContextCleanup;

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL BthPS3_PDO_EvtWdfIoQueueIoDeviceControl;

EVT_WDF_DEVICE_D0_EXIT BthPS3_PDO_EvtWdfDeviceD0Exit;
