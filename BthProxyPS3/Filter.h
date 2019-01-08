#pragma once

#include <usb.h>
#include "L2CAP.h"

NTSTATUS
ProxyUrbSelectConfiguration(
    PURB Urb,
    PDEVICE_CONTEXT Context
);

EVT_WDF_REQUEST_COMPLETION_ROUTINE UrbFunctionInterruptInTransferCompleted;
EVT_WDF_REQUEST_COMPLETION_ROUTINE UrbFunctionBulkInTransferCompleted;
