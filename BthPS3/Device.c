/*++

Module Name:

    device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3CreateDevice)
#endif

NTSTATUS
BthPS3CreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    WDF_OBJECT_ATTRIBUTES           deviceAttributes;
    WDFDEVICE                       device;
    NTSTATUS                        status;
    WDF_PNPPOWER_EVENT_CALLBACKS    pnpPowerCallbacks;
    WDF_CHILD_LIST_CONFIG           childListCfg;

    PAGED_CODE();


    //
    // Prepare child list
    // 
    WDF_CHILD_LIST_CONFIG_INIT(
        &childListCfg,
        sizeof(PDO_IDENTIFICATION_DESCRIPTION),
        BthPS3_EvtWdfChildListCreateDevice
    );

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_BUS_EXTENDER);

    WdfFdoInitSetDefaultChildListConfig(DeviceInit,
        &childListCfg,
        WDF_NO_OBJECT_ATTRIBUTES
    );

    //
    // Configure PNP/power callbacks
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = BthPS3_EvtWdfDeviceSelfManagedIoInit;
    pnpPowerCallbacks.EvtDeviceSelfManagedIoCleanup = BthPS3_EvtWdfDeviceSelfManagedIoCleanup;

    WdfDeviceInitSetPnpPowerEventCallbacks(
        DeviceInit,
        &pnpPowerCallbacks
    );

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, BTHPS3_SERVER_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfDeviceCreate failed with status %!STATUS!", status);

        goto exit;
    }

    status = BTHPS3_SERVER_CONTEXT_INIT(GetServerDeviceContext(device), device);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "Initialization of context failed with status %!STATUS!", status);

        goto exit;
    }

    //
    // Query for interfaces and pre-allocate BRBs
    //

    status = BthPS3Initialize(GetServerDeviceContext(device));
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

exit:
    return status;
}

_Use_decl_annotations_
NTSTATUS
BthPS3_EvtWdfDeviceSelfManagedIoInit(
    WDFDEVICE  Device
)
{
    NTSTATUS status;
    PBTHPS3_SERVER_CONTEXT devCtx = GetServerDeviceContext(Device);

    status = BthPS3RetrieveLocalInfo(&devCtx->Header);
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    status = BthPS3RegisterPSM(devCtx);
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    status = BthPS3RegisterL2CAPServer(devCtx);
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

exit:

    return status;
}

_Use_decl_annotations_
VOID
BthPS3_EvtWdfDeviceSelfManagedIoCleanup(
    WDFDEVICE  Device
)
{
    PBTHPS3_SERVER_CONTEXT devCtx = GetServerDeviceContext(Device);

    PAGED_CODE();

    if (NULL != devCtx->L2CAPServerHandle)
    {
        BthPS3UnregisterL2CAPServer(devCtx);
    }

    if (0 != devCtx->PsmHidControl)
    {
        BthPS3UnregisterPSM(devCtx);
    }

    //
    // Disconnect any open connections, after this point no more
    // connections can come because we have unregistered server
    //
    // BthEchoSrvDisconnectConnectionsOnRemove does not wait for disconnect
    // to complete. Connection object's cleanup callback waits on that. Since 
    // connection objects are children of device, they will be cleaned up and
    // disconnect would complete before device object is cleaned up.
    //

    // BthEchoSrvDisconnectConnectionsOnRemove(devCtx);

    return;
}
