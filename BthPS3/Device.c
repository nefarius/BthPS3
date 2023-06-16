/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2023, Nefarius Software Solutions e.U.                      *
 * All rights reserved.                                                           *
 *                                                                                *
 * Redistribution and use in source and binary forms, with or without             *
 * modification, are permitted provided that the following conditions are met:    *
 *                                                                                *
 * 1. Redistributions of source code must retain the above copyright notice, this *
 *    list of conditions and the following disclaimer.                            *
 *                                                                                *
 * 2. Redistributions in binary form must reproduce the above copyright notice,   *
 *    this list of conditions and the following disclaimer in the documentation   *
 *    and/or other materials provided with the distribution.                      *
 *                                                                                *
 * 3. Neither the name of the copyright holder nor the names of its               *
 *    contributors may be used to endorse or promote products derived from        *
 *    this software without specific prior written permission.                    *
 *                                                                                *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"    *
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE      *
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE *
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE   *
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL     *
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR     *
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER     *
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  *
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  *
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.           *
 *                                                                                *
 **********************************************************************************/


#include "Driver.h"
#include "Device.tmh"
#include "BthPS3ETW.h"


 //
 // Framework device creation entry point
 // 
#pragma code_seg("PAGE")
NTSTATUS
BthPS3_CreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFDEVICE device = NULL;
    NTSTATUS status;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    PBTHPS3_SERVER_CONTEXT pSrvCtx = NULL;
    PDMFDEVICE_INIT dmfDeviceInit = NULL;
    DMF_EVENT_CALLBACKS dmfEventCallbacks;

    PAGED_CODE();


    FuncEntry(TRACE_DEVICE);

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_BUS_EXTENDER);

    dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);

    if (dmfDeviceInit == NULL)
    {
        TraceEvents(
            TRACE_LEVEL_ERROR,
            TRACE_DEVICE,
            "DMF_DmfDeviceInitAllocate failed"
        );

        status = STATUS_INSUFFICIENT_RESOURCES;

        EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"DMF_DmfDeviceInitAllocate", status);

        goto exitFailure;
    }

    DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit, NULL);
    DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit, NULL);

    //
    // Configure PNP/power callbacks
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = BthPS3_EvtWdfDeviceSelfManagedIoInit;
    pnpPowerCallbacks.EvtDeviceSelfManagedIoCleanup = BthPS3_EvtWdfDeviceSelfManagedIoCleanup;

    DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit, &pnpPowerCallbacks);

    WdfDeviceInitSetPnpPowerEventCallbacks(
        DeviceInit,
        &pnpPowerCallbacks
    );

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, BTHPS3_SERVER_CONTEXT);

    do
    {
        if (!NT_SUCCESS(status = WdfDeviceCreate(&DeviceInit, &attributes, &device)))
        {
            TraceError(
                TRACE_DEVICE,
                "WdfDeviceCreate failed with status %!STATUS!",
                status
            );
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfDeviceCreate", status);
            break;
        }

        pSrvCtx = GetServerDeviceContext(device);
        
        const STRING targetModuleName = RTL_CONSTANT_STRING("\\SystemRoot\\System32\\Drivers\\WppRecorder.sys");

        const PVOID driverBaseAddress = FindDriverBaseAddress(targetModuleName);

        if (driverBaseAddress)
        {
            EnumerateExportedFunctions(driverBaseAddress);
        }
        else
        {
            TraceError(
                TRACE_DEVICE,
                "FindDriverBaseAddress failed"
            );
        }

        if (!NT_SUCCESS(status = BthPS3_ServerContextInit(pSrvCtx, device)))
        {
            TraceError(
                TRACE_DEVICE,
                "Initialization of context failed with status %!STATUS!",
                status
            );
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"BthPS3_ServerContextInit", status);
            break;
        }

        //
        // Query for interfaces and pre-allocate BRBs
        //

        if (!NT_SUCCESS(status = BthPS3_Initialize(GetServerDeviceContext(device))))
        {
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"BthPS3_Initialize", status);
            break;
        }

        //
        // Search and open filter remote I/O target
        // 

        if (!NT_SUCCESS(status = BthPS3_OpenFilterIoTarget(device)))
        {
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"BthPS3_OpenFilterIoTarget", status);
            break;
        }

        //
        // Allocate request object for async filter communication
        // 

        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        attributes.ParentObject = pSrvCtx->PsmFilter.IoTarget;

        if (!NT_SUCCESS(status = WdfRequestCreate(
            &attributes,
            pSrvCtx->PsmFilter.IoTarget,
            &pSrvCtx->PsmFilter.AsyncRequest
        )))
        {
            TraceError(
                TRACE_DEVICE,
                "WdfRequestCreate failed with status %!STATUS!",
                status
            );
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfRequestCreate", status);
            break;
        }

        //
        // DMF Module initialization
        // 
        DMF_EVENT_CALLBACKS_INIT(&dmfEventCallbacks);
        dmfEventCallbacks.EvtDmfDeviceModulesAdd = DmfDeviceModulesAdd;
        DMF_DmfDeviceInitSetEventCallbacks(
            dmfDeviceInit,
            &dmfEventCallbacks
        );

        status = DMF_ModulesCreate(device, &dmfDeviceInit);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(
                TRACE_LEVEL_ERROR,
                TRACE_DEVICE,
                "DMF_ModulesCreate failed with status %!STATUS!",
                status
            );
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"DMF_ModulesCreate", status);
            break;
        }

    } while (FALSE);

exitFailure:

    if (dmfDeviceInit != NULL)
    {
        DMF_DmfDeviceInitFree(&dmfDeviceInit);
    }

    if (!NT_SUCCESS(status) && device != NULL)
    {
        WdfObjectDelete(device);
    }

    FuncExit(TRACE_DEVICE, "status=%!STATUS!", status);

    return status;
}
#pragma code_seg()

//
// Locate and attempt to open instance of BTHPS3PSM filter device
// 
#pragma code_seg("PAGE")
NTSTATUS
BthPS3_OpenFilterIoTarget(
    _In_ WDFDEVICE Device
)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    WDF_OBJECT_ATTRIBUTES ioTargetAttrib;
    WDF_IO_TARGET_OPEN_PARAMS openParams;

    DECLARE_CONST_UNICODE_STRING(symbolicLink, BTHPS3PSM_SYMBOLIC_NAME_STRING);

    PAGED_CODE();

    FuncEntry(TRACE_DEVICE);

    const PBTHPS3_SERVER_CONTEXT pCtx = GetServerDeviceContext(Device);

    WDF_OBJECT_ATTRIBUTES_INIT(&ioTargetAttrib);

    do
    {
        if (!NT_SUCCESS(status = WdfIoTargetCreate(
            Device,
            &ioTargetAttrib,
            &pCtx->PsmFilter.IoTarget
        )))
        {
            TraceError(
                TRACE_DEVICE,
                "WdfIoTargetCreate failed with status %!STATUS!",
                status
            );
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfIoTargetCreate", status);
            break;
        }

        WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
            &openParams,
            &symbolicLink,
            STANDARD_RIGHTS_ALL
        );

        if (!NT_SUCCESS(status = WdfIoTargetOpen(
            pCtx->PsmFilter.IoTarget,
            &openParams
        )))
        {
            TraceError(
                TRACE_DEVICE,
                "WdfIoTargetOpen failed with status %!STATUS!",
                status
            );
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfIoTargetOpen", status);
            WdfObjectDelete(pCtx->PsmFilter.IoTarget);
            break;
        }

    } while (FALSE);

    FuncExit(TRACE_DEVICE, "status=%!STATUS!", status);

    return status;
}
#pragma code_seg()

//
// Timed auto-reset of filter driver
// 
void BthPS3_EnablePatchEvtWdfTimer(
    WDFTIMER Timer
)
{
    NTSTATUS status;
    PBTHPS3_SERVER_CONTEXT devCtx = GetServerDeviceContext(WdfTimerGetParentObject(Timer));

    TraceVerbose(TRACE_DEVICE,
        "Requesting filter to enable patch"
    );

    if (!NT_SUCCESS(status = BthPS3PSM_EnablePatchAsync(
        devCtx->PsmFilter.IoTarget,
        0 // TODO: read from registry?
    )))
    {
        TraceVerbose(TRACE_DEVICE,
            "BthPS3PSM_EnablePatchAsync failed with status %!STATUS!",
            status
        );

        EventWriteFilterAutoEnabledFailed(NULL, status);
    }
    else
    {
        EventWriteFilterAutoEnabledSuccessfully(NULL);
    }
}

//
// Gets invoked on device power-up
// 
_Use_decl_annotations_
NTSTATUS
BthPS3_EvtWdfDeviceSelfManagedIoInit(
    WDFDEVICE Device
)
{
    NTSTATUS status;
    PBTHPS3_SERVER_CONTEXT devCtx = GetServerDeviceContext(Device);

    FuncEntry(TRACE_DEVICE);

    do
    {
        if (!NT_SUCCESS(status = BthPS3_RetrieveLocalInfo(&devCtx->Header)))
        {
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"BthPS3_RetrieveLocalInfo", status);
            break;
        }

        if (!NT_SUCCESS(status = BthPS3_RegisterPSM(devCtx)))
        {
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"BthPS3_RegisterPSM", status);
            break;
        }

        if (!NT_SUCCESS(status = BthPS3_RegisterL2CAPServer(devCtx)))
        {
            EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"BthPS3_RegisterL2CAPServer", status);
            break;
        }

        //
        // Attempt to enable, but ignore failure
        //
        if (devCtx->Settings.AutoEnableFilter)
        {
            (void)BthPS3PSM_EnablePatchSync(
                devCtx->PsmFilter.IoTarget,
                0
            );
        }

    } while (FALSE);

    FuncExit(TRACE_DEVICE, "status=%!STATUS!", status);

    return status;
}

//
// Gets invoked on device shutdown
// 
#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
BthPS3_EvtWdfDeviceSelfManagedIoCleanup(
    WDFDEVICE Device
)
{
    const PBTHPS3_SERVER_CONTEXT devCtx = GetServerDeviceContext(Device);

    PAGED_CODE();

    FuncEntry(TRACE_DEVICE);

    if (devCtx->PsmFilter.IoTarget != NULL)
    {
        WdfIoTargetClose(devCtx->PsmFilter.IoTarget);
        WdfObjectDelete(devCtx->PsmFilter.IoTarget);
    }

    if (NULL != devCtx->L2CAPServerHandle)
    {
        BthPS3_UnregisterL2CAPServer(devCtx);
    }

    if (0 != devCtx->PsmHidControl)
    {
        BthPS3_UnregisterPSM(devCtx);
    }

    FuncExitNoReturn(TRACE_DEVICE);
}
#pragma code_seg()

//
// Initializes DMF modules
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DmfDeviceModulesAdd(
    _In_ WDFDEVICE Device,
    _In_ PDMFMODULE_INIT DmfModuleInit
)
{
    FuncEntry(TRACE_DEVICE);

    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_Pdo moduleConfigPdo;
    DMF_CONFIG_QueuedWorkItem moduleConfigQwi;

    const PBTHPS3_SERVER_CONTEXT pSrvCtx = GetServerDeviceContext(Device);

    //
    // PDO module
    // 

    DMF_CONFIG_Pdo_AND_ATTRIBUTES_INIT(
        &moduleConfigPdo,
        &moduleAttributes
    );

    moduleConfigPdo.DeviceLocation = L"Nefarius Bluetooth PS Enumerator";
    moduleConfigPdo.InstanceIdFormatString = L"BTHPS3_DEVICE_%02d";
    // Do not create any PDOs during Module create.
    // PDOs will be created dynamically through Module Method.
    //
    moduleConfigPdo.PdoRecordCount = 0;
    moduleConfigPdo.PdoRecords = NULL;
    moduleConfigPdo.EvtPdoPnpCapabilities = NULL;
    moduleConfigPdo.EvtPdoPowerCapabilities = NULL;
    moduleConfigPdo.EvtPdoPreCreate = BthPS3_PDO_EvtPreCreate;
    moduleConfigPdo.EvtPdoPostCreate = BthPS3_PDO_EvtPostCreate;

    DMF_DmfModuleAdd(
        DmfModuleInit,
        &moduleAttributes,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pSrvCtx->Header.PdoModule
    );

    //
    // Queued Work Item Module
    // 

    DMF_CONFIG_QueuedWorkItem_AND_ATTRIBUTES_INIT(
        &moduleConfigQwi,
        &moduleAttributes
    );

    moduleConfigQwi.BufferQueueConfig.SourceSettings.BufferCount = 4;
    moduleConfigQwi.BufferQueueConfig.SourceSettings.BufferSize = sizeof(BTHPS3_QWI_CONTEXT);
    moduleConfigQwi.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigQwi.EvtQueuedWorkitemFunction = BthPS3_EvtQueuedWorkItemHandler;

    DMF_DmfModuleAdd(
        DmfModuleInit,
        &moduleAttributes,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pSrvCtx->Header.QueuedWorkItemModule
    );

    FuncExitNoReturn(TRACE_DEVICE);
}
