/*++

Module Name:

    device.h

Abstract:

    This file contains the device definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include "BthPS3.h"
#include "Bluetooth.h"

EXTERN_C_START



//
// Function to initialize the device and its callbacks
//
NTSTATUS
BthPS3CreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT BthPS3_EvtWdfDeviceSelfManagedIoInit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP BthPS3_EvtWdfDeviceSelfManagedIoCleanup;


EXTERN_C_END
