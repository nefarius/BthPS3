/*++

Module Name:

    device.h

Abstract:

    This file contains the device definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include "public.h"
#include <usb.h>
#include <wdfusb.h>

EXTERN_C_START

typedef struct _BTHPS3_DEVICE_CONTEXT_FILTER
{
    WDFUSBDEVICE UsbDevice;

    WDFUSBINTERFACE UsbInterface;

    WDFUSBPIPE InterruptPipe;

    WDFUSBPIPE BulkReadPipe;

    WDFUSBPIPE BulkWritePipe;

} BTHPS3_DEVICE_CONTEXT_FILTER, *PBTHPS3_DEVICE_CONTEXT_FILTER;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_DEVICE_CONTEXT_FILTER, DeviceGetFilterContext)


//
// Function to initialize the device and its callbacks
//
NTSTATUS
BthPS3CreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT BthPS3EvtWdfDeviceSelfManagedIoInit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP BthPS3EvtWdfDeviceSelfManagedIoCleanup;


EXTERN_C_END
