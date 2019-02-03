/*++

Module Name:

    queue.h

Abstract:

    This file contains the queue definitions.

Environment:

    Kernel-mode Driver Framework

--*/

EXTERN_C_START

//
// Request context space for forwarded requests (PDO to FDO)
// 
typedef struct _BTHPS3_FDO_PDO_REQUEST_CONTEXT
{
    //
    // MAC address identifying this device
    // 
    BTH_ADDR RemoteAddress;

    //
    // Type (make, model) of remote device
    // 
    DS_DEVICE_TYPE DeviceType;

} BTHPS3_FDO_PDO_REQUEST_CONTEXT, *PBTHPS3_FDO_PDO_REQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_FDO_PDO_REQUEST_CONTEXT, GetFdoPdoRequestContext)

NTSTATUS
BthPS3QueueInitialize(
    _In_ WDFDEVICE Device
    );

//
// Events from the IoQueue object
//
EVT_WDF_IO_QUEUE_IO_STOP BthPS3_EvtIoStop;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL BthPS3_EvtWdfIoQueueIoInternalDeviceControl;

EXTERN_C_END
