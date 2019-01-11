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
