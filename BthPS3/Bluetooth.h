#pragma once

#include <bthdef.h>
#include <bthguid.h>
#include <bthioctl.h>
#include <sdpnode.h>
#include <bthddi.h>
#include <bthsdpddi.h>
#include <bthsdpdef.h>

#define POOLTAG_BTHPS3          'PhtB'

typedef struct _BTHPS3_DEVICE_CONTEXT_HEADER
{
    //
    // Framework device this context is associated with
    //
    WDFDEVICE Device;

    //
    // Default I/O target
    //
    WDFIOTARGET IoTarget;

    //
    // Profile driver interface which contains profile driver DDI
    //
    BTH_PROFILE_DRIVER_INTERFACE ProfileDrvInterface;

    //
    // Local Bluetooth Address
    //
    BTH_ADDR LocalBthAddr;

    //
    // Preallocated request to be reused during initialization/deinitialzation phase
    // Access to this reqeust is not synchronized
    //
    WDFREQUEST Request;
} BTHPS3_DEVICE_CONTEXT_HEADER, *PBTHPS3_DEVICE_CONTEXT_HEADER;

typedef struct _BTHPS3_SERVER_CONTEXT
{
    //
    // Context common to client and server
    //
    BTHPS3_DEVICE_CONTEXT_HEADER Header;

    //
    // Artificial HID Control PSM
    // 
    USHORT PsmHidControl;

    //
    // Artificial HID Interrupt PSM
    // 
    USHORT PsmHidInterrupt;

    //
    // Handle obtained by registering L2CAP server
    //
    L2CAP_SERVER_HANDLE L2CAPServerHandle;

    //
    // BRB used for server and PSM register and unregister
    //
    // Server and PSM register and unregister must be done
    // sequentially since access to this brb is not
    // synchronized.
    //
    struct _BRB RegisterUnregisterBrb;

    //
    // Connection List lock
    //
    WDFSPINLOCK ConnectionListLock;

    //
    // Outstanding open connections
    //
    LIST_ENTRY ConnectionList;

    WDFCOLLECTION ClientConnections;

    WDFSPINLOCK ClientConnectionsLock;

} BTHPS3_SERVER_CONTEXT, *PBTHPS3_SERVER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_SERVER_CONTEXT, GetServerDeviceContext)


_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3RetrieveLocalInfo(
    _In_ PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr
);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3SendBrbSynchronously(
    _In_ WDFIOTARGET IoTarget,
    _In_ WDFREQUEST Request,
    _In_ PBRB Brb,
    _In_ ULONG BrbSize
);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3RegisterPSM(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3UnregisterPSM(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3RegisterL2CAPServer(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3UnregisterL2CAPServer(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3DeviceContextHeaderInit(
    PBTHPS3_DEVICE_CONTEXT_HEADER Header,
    WDFDEVICE Device
);

NTSTATUS
FORCEINLINE
BTHPS3_SERVER_CONTEXT_INIT(
    PBTHPS3_SERVER_CONTEXT Context,
    WDFDEVICE Device
)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    status = BthPS3DeviceContextHeaderInit(&Context->Header, Device);
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Device;

    status = WdfSpinLockCreate(
        &attributes,
        &Context->ConnectionListLock
    );
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    InitializeListHead(&Context->ConnectionList);

exit:
    return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3QueryInterfaces(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3Initialize(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(DISPATCH_LEVEL)
void
BthPS3IndicationCallback(
    _In_ PVOID Context,
    _In_ INDICATION_CODE Indication,
    _In_ PINDICATION_PARAMETERS Parameters
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3SendBrbAsync(
    _In_ WDFIOTARGET IoTarget,
    _In_ WDFREQUEST Request,
    _In_ PBRB Brb,
    _In_ size_t BrbSize,
    _In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE ComplRoutine,
    _In_opt_ WDFCONTEXT Context
);
