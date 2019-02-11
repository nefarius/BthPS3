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


#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3_CreateDevice)
#pragma alloc_text (PAGE, BthPS3_EvtWdfDeviceSelfManagedIoCleanup)
#endif

NTSTATUS
BthPS3_CreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    WDF_OBJECT_ATTRIBUTES           attributes;
    WDFDEVICE                       device;
    NTSTATUS                        status;
    WDF_PNPPOWER_EVENT_CALLBACKS    pnpPowerCallbacks;
    WDF_CHILD_LIST_CONFIG           childListCfg;

    PAGED_CODE();


    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Entry");

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_BUS_EXTENDER);

    //
    // Prepare child list
    // 
    WDF_CHILD_LIST_CONFIG_INIT(
        &childListCfg,
        sizeof(PDO_IDENTIFICATION_DESCRIPTION),
        BthPS3_EvtWdfChildListCreateDevice
    );
    childListCfg.EvtChildListIdentificationDescriptionCompare =
        BthPS3_PDO_EvtChildListIdentificationDescriptionCompare;

    WdfFdoInitSetDefaultChildListConfig(DeviceInit,
        &childListCfg,
        WDF_NO_OBJECT_ATTRIBUTES
    );

    //
    // Add request context shared with PDOs
    // 
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &attributes,
        BTHPS3_FDO_PDO_REQUEST_CONTEXT
    );
    WdfDeviceInitSetRequestAttributes(
        DeviceInit,
        &attributes
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

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, BTHPS3_SERVER_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);

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

    status = BthPS3QueueInitialize(device);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "BthPS3QueueInitialize failed with status %!STATUS!", status);
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
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

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

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Entry");

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

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

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

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Entry");

    //
    // Drop children
    // 
    WdfChildListBeginScan(WdfFdoGetDefaultChildList(Device));
    WdfChildListEndScan(WdfFdoGetDefaultChildList(Device));

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

    //
    // TODO: implement me!
    // 

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return;
}
