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

#include <usb.h>
#include "L2CAP.h"

#define L2CAP_MIN_BUFFER_LEN    0x10

NTSTATUS
ProxyUrbSelectConfiguration(
    PURB Urb,
    PDEVICE_CONTEXT Context
);

EVT_WDF_REQUEST_COMPLETION_ROUTINE UrbFunctionBulkInTransferCompleted;
