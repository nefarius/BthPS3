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

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#include "device.h"
#include "queue.h"
#include "trace.h"
#include "Bluetooth.h"
#include "PSM.h"
#include "Connection.h"
#include "L2CAP.h"
#include "BusLogic.h"
#include "Util.h"

EXTERN_C_START

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD BthPS3EvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP BthPS3EvtDriverContextCleanup;

EXTERN_C_END
