/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2020, Nefarius Software Solutions e.U.                      *
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
#include "bluetooth.tmh"
#include <bthguid.h>
#include "BthPS3.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3_UnregisterPSM)
#pragma alloc_text (PAGE, BthPS3_UnregisterL2CAPServer)
#pragma alloc_text (PAGE, BthPS3_QueryInterfaces)
#pragma alloc_text (PAGE, BthPS3_Initialize)
#endif

//
// Requests information about underlying radio
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RetrieveLocalInfo(
    _In_ PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr
)
{
    NTSTATUS status = STATUS_SUCCESS;
    struct _BRB_GET_LOCAL_BD_ADDR * brb = NULL;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Entry");

    brb = (struct _BRB_GET_LOCAL_BD_ADDR *)
        DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
            BRB_HCI_GET_LOCAL_BD_ADDR,
            POOLTAG_BTHPS3
        );

    if (brb == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;

        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_BTH,
            "Failed to allocate brb BRB_HCI_GET_LOCAL_BD_ADDR with status %!STATUS!",
            status
        );

        goto exit;
    }

    status = BthPS3_SendBrbSynchronously(
        DevCtxHdr->IoTarget,
        DevCtxHdr->HostInitRequest,
        (PBRB)brb,
        sizeof(*brb)
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "Retrieving local bth address failed, Status code %!STATUS!\n", status);
    }
    else
    {
        DevCtxHdr->LocalBthAddr = brb->BtAddress;
    }

    DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);

exit:

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Exit");

    return status;
}

#pragma region PSM

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RegisterPSM(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
    NTSTATUS status;
    struct _BRB_PSM * brb;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Entry");

    DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
        &(DevCtx->RegisterUnregisterBrb),
        BRB_REGISTER_PSM
    );

    brb = (struct _BRB_PSM *)
        &(DevCtx->RegisterUnregisterBrb);

    //
    // Register PSM_DS3_HID_CONTROL
    // 

    brb->Psm = PSM_DS3_HID_CONTROL;

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_BTH,
        "++ Trying to register PSM 0x%04X",
        brb->Psm
    );

    status = BthPS3_SendBrbSynchronously(
        DevCtx->Header.IoTarget,
        DevCtx->Header.HostInitRequest,
        (PBRB)brb,
        sizeof(*brb)
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "BRB_REGISTER_PSM failed with status %!STATUS!", status);
        goto exit;
    }

    //
    // Store PSM obtained
    //
    DevCtx->PsmHidControl = brb->Psm;

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_BTH,
        "++ Got PSM 0x%04X",
        brb->Psm
    );

    // 
    // Shouldn't happen but validate anyway
    // 
    if (brb->Psm != PSM_DS3_HID_CONTROL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "Requested PSM 0x%04X but got 0x%04X instead",
            PSM_DS3_HID_CONTROL,
            brb->Psm
        );

        status = STATUS_INVALID_PARAMETER_1;
        goto exit;
    }

    //
    // Register PSM_DS3_HID_CONTROL
    // 

    brb->Psm = PSM_DS3_HID_INTERRUPT;

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_BTH,
        "++ Trying to register PSM 0x%04X",
        brb->Psm
    );

    status = BthPS3_SendBrbSynchronously(
        DevCtx->Header.IoTarget,
        DevCtx->Header.HostInitRequest,
        (PBRB)brb,
        sizeof(*brb)
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "BRB_REGISTER_PSM failed with status %!STATUS!", status);
        goto exit;
    }

    //
    // Store PSM obtained
    //
    DevCtx->PsmHidInterrupt = brb->Psm;

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_BTH,
        "++ Got PSM 0x%04X",
        brb->Psm
    );

    // 
    // Shouldn't happen but validate anyway
    // 
    if (brb->Psm != PSM_DS3_HID_INTERRUPT)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "Requested PSM 0x%04X but got 0x%04X instead",
            PSM_DS3_HID_INTERRUPT,
            brb->Psm
        );

        status = STATUS_INVALID_PARAMETER_2;
    }

exit:

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Exit");

    return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3_UnregisterPSM(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
    NTSTATUS status;
    struct _BRB_PSM * brb;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Entry");

    DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
        &(DevCtx->RegisterUnregisterBrb),
        BRB_UNREGISTER_PSM
    );

    brb = (struct _BRB_PSM *)
        &(DevCtx->RegisterUnregisterBrb);

    brb->Psm = DevCtx->PsmHidControl;

    status = BthPS3_SendBrbSynchronously(
        DevCtx->Header.IoTarget,
        DevCtx->Header.HostInitRequest,
        (PBRB)brb,
        sizeof(*(brb))
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "BRB_UNREGISTER_PSM failed with status %!STATUS!", status);

        //
        // Send does not fail for resource reasons
        //
        NT_ASSERT(FALSE);

        goto exit;
    }

    DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
        &(DevCtx->RegisterUnregisterBrb),
        BRB_UNREGISTER_PSM
    );

    brb = (struct _BRB_PSM *)
        &(DevCtx->RegisterUnregisterBrb);

    brb->Psm = DevCtx->PsmHidInterrupt;

    status = BthPS3_SendBrbSynchronously(
        DevCtx->Header.IoTarget,
        DevCtx->Header.HostInitRequest,
        (PBRB)brb,
        sizeof(*(brb))
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "BRB_UNREGISTER_PSM failed with status %!STATUS!", status);

        //
        // Send does not fail for resource reasons
        //
        NT_ASSERT(FALSE);

        goto exit;
    }

exit:

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Exit");

    return;
}

#pragma endregion

#pragma region L2CAP

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RegisterL2CAPServer(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
    NTSTATUS status;
    struct _BRB_L2CA_REGISTER_SERVER *brb;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Entry");

    DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
        &(DevCtx->RegisterUnregisterBrb),
        BRB_L2CA_REGISTER_SERVER
    );

    brb = (struct _BRB_L2CA_REGISTER_SERVER *)
        &(DevCtx->RegisterUnregisterBrb);

    //
    // Format brb
    //
    brb->BtAddress = BTH_ADDR_NULL;
    brb->PSM = 0; //we have already registered the PSMs
    brb->IndicationCallback = &BthPS3_IndicationCallback;
    brb->IndicationCallbackContext = DevCtx;
    brb->IndicationFlags = 0;
    brb->ReferenceObject = WdfDeviceWdmGetDeviceObject(DevCtx->Header.Device);

    status = BthPS3_SendBrbSynchronously(
        DevCtx->Header.IoTarget,
        DevCtx->Header.HostInitRequest,
        (PBRB)brb,
        sizeof(*brb)
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "BRB_REGISTER_PSM failed with status %!STATUS!", status);
    }
    else
    {
        //
        // Store server handle
        //
        DevCtx->L2CAPServerHandle = brb->ServerHandle;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Exit");

    return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3_UnregisterL2CAPServer(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
    NTSTATUS status;
    struct _BRB_L2CA_UNREGISTER_SERVER *brb;

    PAGED_CODE();

    if (NULL == DevCtx->L2CAPServerHandle)
    {
        return;
    }

    DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
        &(DevCtx->RegisterUnregisterBrb),
        BRB_L2CA_UNREGISTER_SERVER
    );

    brb = (struct _BRB_L2CA_UNREGISTER_SERVER *)
        &(DevCtx->RegisterUnregisterBrb);

    //
    // Format Brb
    //
    brb->BtAddress = BTH_ADDR_NULL;//DevCtx->LocalAddress;
    brb->Psm = 0; //since we will use unregister PSM to unregister.
    brb->ServerHandle = DevCtx->L2CAPServerHandle;

    status = BthPS3_SendBrbSynchronously(
        DevCtx->Header.IoTarget,
        DevCtx->Header.HostInitRequest,
        (PBRB)brb,
        sizeof(*(brb))
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "BRB_L2CA_UNREGISTER_SERVER failed with status %!STATUS!", status);

        //
        // Send does not fail for resource reasons
        //
        NT_ASSERT(FALSE);

        return;
    }

    DevCtx->L2CAPServerHandle = NULL;

    return;
}

#pragma endregion

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3_DeviceContextHeaderInit(
    PBTHPS3_DEVICE_CONTEXT_HEADER Header,
    WDFDEVICE Device
)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    Header->Device = Device;

    Header->IoTarget = WdfDeviceGetIoTarget(Device);

    //
    // Initialize request object
    //

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Device;

    status = WdfRequestCreate(
        &attributes,
        Header->IoTarget,
        &Header->HostInitRequest
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_BTH,
            "Failed to pre-allocate request in device context with status %!STATUS!",
            status
        );
    }

    return status;
}

//
// Initialize all members of the server device context
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_ServerContextInit(
    PBTHPS3_SERVER_CONTEXT Context,
    WDFDEVICE Device
)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_TIMER_CONFIG timerCfg;

    //
    // Initialize crucial header struct first
    // 
    status = BthPS3_DeviceContextHeaderInit(&Context->Header, Device);
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Device;

    status = WdfSpinLockCreate(
        &attributes,
        &Context->ClientConnectionsLock
    );
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Device;

    status = WdfCollectionCreate(
        &attributes,
        &Context->ClientConnections
    );
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Device;

    WDF_TIMER_CONFIG_INIT(&timerCfg, BthPS3_EnablePatchEvtWdfTimer);

    status = WdfTimerCreate(
        &timerCfg,
        &attributes,
        &Context->PsmFilter.AutoResetTimer
    );
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Device;

    status = WdfCollectionCreate(
        &attributes,
        &Context->Settings.SIXAXISSupportedNames
    );
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    status = WdfCollectionCreate(
        &attributes,
        &Context->Settings.NAVIGATIONSupportedNames
    );
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    status = WdfCollectionCreate(
        &attributes,
        &Context->Settings.MOTIONSupportedNames
    );
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    status = WdfCollectionCreate(
        &attributes,
        &Context->Settings.WIRELESSSupportedNames
    );
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    //
    // Query registry for dynamic values
    // 
    status = BthPS3_SettingsContextInit(Context);

exit:
    return status;
}

//
// Read runtime properties from registry
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_SettingsContextInit(
    PBTHPS3_SERVER_CONTEXT Context
)
{
    NTSTATUS                status;
    WDFKEY                  hKey = NULL;
    WDF_OBJECT_ATTRIBUTES   attribs;

    DECLARE_CONST_UNICODE_STRING(autoEnableFilter, BTHPS3_REG_VALUE_AUTO_ENABLE_FILTER);
    DECLARE_CONST_UNICODE_STRING(autoDisableFilter, BTHPS3_REG_VALUE_AUTO_DISABLE_FILTER);
    DECLARE_CONST_UNICODE_STRING(autoEnableFilterDelay, BTHPS3_REG_VALUE_AUTO_ENABLE_FILTER_DELAY);

    DECLARE_CONST_UNICODE_STRING(isSIXAXISSupported, BTHPS3_REG_VALUE_IS_SIXAXIS_SUPPORTED);
    DECLARE_CONST_UNICODE_STRING(isNAVIGATIONSupported, BTHPS3_REG_VALUE_IS_NAVIGATION_SUPPORTED);
    DECLARE_CONST_UNICODE_STRING(isMOTIONSupported, BTHPS3_REG_VALUE_IS_MOTION_SUPPORTED);
    DECLARE_CONST_UNICODE_STRING(isWIRELESSSupported, BTHPS3_REG_VALUE_IS_WIRELESS_SUPPORTED);

    DECLARE_CONST_UNICODE_STRING(SIXAXISSupportedNames, BTHPS3_REG_VALUE_SIXAXIS_SUPPORTED_NAMES);
    DECLARE_CONST_UNICODE_STRING(NAVIGATIONSupportedNames, BTHPS3_REG_VALUE_NAVIGATION_SUPPORTED_NAMES);
    DECLARE_CONST_UNICODE_STRING(MOTIONSupportedNames, BTHPS3_REG_VALUE_MOTION_SUPPORTED_NAMES);
    DECLARE_CONST_UNICODE_STRING(WIRELESSSupportedNames, BTHPS3_REG_VALUE_WIRELESS_SUPPORTED_NAMES);

    //
    // Set default values
    //
    Context->Settings.AutoEnableFilter = TRUE;
    Context->Settings.AutoDisableFilter = TRUE;
    Context->Settings.AutoEnableFilterDelay = 10; // Seconds

    Context->Settings.IsSIXAXISSupported = TRUE;
    Context->Settings.IsNAVIGATIONSupported = TRUE;
    Context->Settings.IsMOTIONSupported = TRUE;
    Context->Settings.IsWIRELESSSupported = TRUE;

    //
    // Open
    //   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\BthPS3\Parameters
    // key
    // 
    status = WdfDriverOpenParametersRegistryKey(
        WdfGetDriver(),
        STANDARD_RIGHTS_ALL,
        WDF_NO_OBJECT_ATTRIBUTES,
        &hKey
    );

    //
    // On success, read configuration values
    // 
    if (NT_SUCCESS(status))
    {
        //
        // Don't care, if it fails, keep default value
        // 
        (void)WdfRegistryQueryULong(
            hKey,
            &autoEnableFilter,
            &Context->Settings.AutoEnableFilter
        );

        (void)WdfRegistryQueryULong(
            hKey,
            &autoDisableFilter,
            &Context->Settings.AutoDisableFilter
        );

        (void)WdfRegistryQueryULong(
            hKey,
            &autoEnableFilterDelay,
            &Context->Settings.AutoEnableFilterDelay
        );

        (void)WdfRegistryQueryULong(
            hKey,
            &isSIXAXISSupported,
            &Context->Settings.IsSIXAXISSupported
        );

        (void)WdfRegistryQueryULong(
            hKey,
            &isNAVIGATIONSupported,
            &Context->Settings.IsNAVIGATIONSupported
        );

        (void)WdfRegistryQueryULong(
            hKey,
            &isMOTIONSupported,
            &Context->Settings.IsMOTIONSupported
        );

        (void)WdfRegistryQueryULong(
            hKey,
            &isWIRELESSSupported,
            &Context->Settings.IsWIRELESSSupported
        );

        WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
        attribs.ParentObject = Context->Settings.SIXAXISSupportedNames;
        (void)WdfRegistryQueryMultiString(
            hKey,
            &SIXAXISSupportedNames,
            &attribs,
            Context->Settings.SIXAXISSupportedNames
        );

        WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
        attribs.ParentObject = Context->Settings.NAVIGATIONSupportedNames;
        (void)WdfRegistryQueryMultiString(
            hKey,
            &NAVIGATIONSupportedNames,
            &attribs,
            Context->Settings.NAVIGATIONSupportedNames
        );

        WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
        attribs.ParentObject = Context->Settings.MOTIONSupportedNames;
        (void)WdfRegistryQueryMultiString(
            hKey,
            &MOTIONSupportedNames,
            &attribs,
            Context->Settings.MOTIONSupportedNames
        );

        WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
        attribs.ParentObject = Context->Settings.WIRELESSSupportedNames;
        (void)WdfRegistryQueryMultiString(
            hKey,
            &WIRELESSSupportedNames,
            &attribs,
            Context->Settings.WIRELESSSupportedNames
        );
        
        WdfRegistryClose(hKey);
    }

    return status;
}

//
// Grabs driver-to-driver interface
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_QueryInterfaces(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
    NTSTATUS status;

    PAGED_CODE();

    status = WdfFdoQueryForInterface(
        DevCtx->Header.Device,
        &GUID_BTHDDI_PROFILE_DRIVER_INTERFACE,
        (PINTERFACE)(&DevCtx->Header.ProfileDrvInterface),
        sizeof(DevCtx->Header.ProfileDrvInterface),
        BTHDDI_PROFILE_DRIVER_INTERFACE_VERSION_FOR_QI,
        NULL
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "QueryInterface failed for Interface profile driver interface, version %d, Status code %!STATUS!",
            BTHDDI_PROFILE_DRIVER_INTERFACE_VERSION_FOR_QI,
            status);
    }

    return status;
}

//
// Initialize Bluetooth driver-to-driver interface
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_Initialize(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
    PAGED_CODE();

    return BthPS3_QueryInterfaces(DevCtx);
}

//
// Gets invoked by parent bus if there's work for our driver
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
void
BthPS3_IndicationCallback(
    _In_ PVOID Context,
    _In_ INDICATION_CODE Indication,
    _In_ PINDICATION_PARAMETERS Parameters
)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    WDFWORKITEM asyncRemoteConnect;
    WDF_WORKITEM_CONFIG asyncConfig;
    WDF_OBJECT_ATTRIBUTES asyncAttribs;
    PBTHPS3_REMOTE_CONNECT_CONTEXT connectCtx = NULL;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Entry");

    switch (Indication)
    {
    case IndicationAddReference:
    case IndicationReleaseReference:
        break;
    case IndicationRemoteConnect:
    {
        PBTHPS3_SERVER_CONTEXT devCtx = (PBTHPS3_SERVER_CONTEXT)Context;

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BTH,
            "New connection for PSM 0x%04X from %012llX arrived",
            Parameters->Parameters.Connect.Request.PSM,
            Parameters->BtAddress);

        if (KeGetCurrentIrql() <= PASSIVE_LEVEL)
        {
            //
            // Main entry point for a new connection, decides if valid etc.
            // 
            L2CAP_PS3_HandleRemoteConnect(devCtx, Parameters);

            break;
        }

        //
        // Can be DPC level, enqueue work item
        // 

        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH,
            "IRQL %!irql! too high, preparing async call",
            KeGetCurrentIrql()
        );

        WDF_WORKITEM_CONFIG_INIT(
            &asyncConfig,
            L2CAP_PS3_HandleRemoteConnectAsync
        );
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&asyncAttribs, BTHPS3_REMOTE_CONNECT_CONTEXT);
        asyncAttribs.ParentObject = devCtx->Header.Device;

        status = WdfWorkItemCreate(
            &asyncConfig,
            &asyncAttribs,
            &asyncRemoteConnect
        );

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
                "WdfWorkItemCreate failed with status %!STATUS!",
                status
            );

            break;
        }

        //
        // Pass on parameters as work item context
        // 
        connectCtx = GetRemoteConnectContext(asyncRemoteConnect);
        connectCtx->ServerContext = devCtx;
        connectCtx->IndicationParameters = *Parameters;

        //
        // Kick off async call
        // 
        WdfWorkItemEnqueue(asyncRemoteConnect);

        break;
    }
    case IndicationRemoteDisconnect:
    {
        //
        // We register L2CAP_PS3_ConnectionIndicationCallback for disconnect
        //
        NT_ASSERT(FALSE);
        break;
    }
    case IndicationRemoteConfigRequest:
    case IndicationRemoteConfigResponse:
    case IndicationFreeExtraOptions:
        break;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Exit");
}

#pragma region BRB Request submission

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_SendBrbSynchronously(
    _In_ WDFIOTARGET IoTarget,
    _In_ WDFREQUEST Request,
    _In_ PBRB Brb,
    _In_ ULONG BrbSize
)
{
    NTSTATUS status;
    WDF_REQUEST_REUSE_PARAMS reuseParams;
    WDF_MEMORY_DESCRIPTOR OtherArg1Desc;

    WDF_REQUEST_REUSE_PARAMS_INIT(
        &reuseParams,
        WDF_REQUEST_REUSE_NO_FLAGS,
        STATUS_NOT_SUPPORTED
    );

    status = WdfRequestReuse(Request, &reuseParams);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
        &OtherArg1Desc,
        Brb,
        BrbSize
    );

    status = WdfIoTargetSendInternalIoctlOthersSynchronously(
        IoTarget,
        Request,
        IOCTL_INTERNAL_BTH_SUBMIT_BRB,
        &OtherArg1Desc,
        NULL, //OtherArg2
        NULL, //OtherArg4
        NULL, //RequestOptions
        NULL  //BytesReturned
    );

    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3_SendBrbAsync(
    _In_ WDFIOTARGET IoTarget,
    _In_ WDFREQUEST Request,
    _In_ PBRB Brb,
    _In_ size_t BrbSize,
    _In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE ComplRoutine,
    _In_opt_ WDFCONTEXT Context
)
{
    NTSTATUS status = BTH_ERROR_SUCCESS;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFMEMORY memoryArg1 = NULL;

    if (BrbSize <= 0)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "BrbSize has invalid value: %I64d\n",
            BrbSize
        );

        status = STATUS_INVALID_PARAMETER;

        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Request;

    status = WdfMemoryCreatePreallocated(
        &attributes,
        Brb,
        BrbSize,
        &memoryArg1
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "Creating preallocated memory for Brb 0x%p failed, Request to be formatted 0x%p, "
            "Status code %!STATUS!\n",
            Brb,
            Request,
            status
        );

        return status;
    }

    status = WdfIoTargetFormatRequestForInternalIoctlOthers(
        IoTarget,
        Request,
        IOCTL_INTERNAL_BTH_SUBMIT_BRB,
        memoryArg1,
        NULL, //OtherArg1Offset
        NULL, //OtherArg2
        NULL, //OtherArg2Offset
        NULL, //OtherArg4
        NULL  //OtherArg4Offset
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "Formatting request 0x%p with Brb 0x%p failed, Status code %!STATUS!",
            Request,
            Brb,
            status
        );

        return status;
    }

    //
    // Set a CompletionRoutine callback function.
    //
    WdfRequestSetCompletionRoutine(
        Request,
        ComplRoutine,
        Context
    );

    if (FALSE == WdfRequestSend(
        Request,
        IoTarget,
        NULL
    ))
    {
        status = WdfRequestGetStatus(Request);

        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "Request send failed for request 0x%p, Brb 0x%p, Status code %!STATUS!",
            Request,
            Brb,
            status
        );

        return status;
    }

    return status;
}

#pragma endregion
