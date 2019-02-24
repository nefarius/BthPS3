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


#include "Driver.h"
#include "bluetooth.tmh"
#include <bthguid.h>


#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3UnregisterPSM)
#pragma alloc_text (PAGE, BthPS3UnregisterL2CAPServer)
#pragma alloc_text (PAGE, BthPS3QueryInterfaces)
#pragma alloc_text (PAGE, BthPS3Initialize)
#endif

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3RetrieveLocalInfo(
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

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3RegisterPSM(
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

exit:

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Exit");

    return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3UnregisterPSM(
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

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3RegisterL2CAPServer(
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
    brb->IndicationCallback = &BthPS3IndicationCallback;
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
BthPS3UnregisterL2CAPServer(
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

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3DeviceContextHeaderInit(
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
// Grabs driver-to-driver interface
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3QueryInterfaces(
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
BthPS3Initialize(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
    PAGED_CODE();

    return BthPS3QueryInterfaces(DevCtx);
}

//
// Gets invoked by parent bus if there's work for our driver
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
void
BthPS3IndicationCallback(
    _In_ PVOID Context,
    _In_ INDICATION_CODE Indication,
    _In_ PINDICATION_PARAMETERS Parameters
)
{
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
            "New connection for PSM 0x%04X from %llX arrived",
            Parameters->Parameters.Connect.Request.PSM,
            Parameters->BtAddress);

        //
        // Main entry point for a new connection, decides if valid etc.
        // 
        L2CAP_PS3_HandleRemoteConnect(devCtx, Parameters);

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
            "Formatting request 0x%p with Brb 0x%p failed, Status code %!STATUS!\n",
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
            "Request send failed for request 0x%p, Brb 0x%p, Status code %!STATUS!\n",
            Request,
            Brb,
            status
        );

        return status;
    }

    return status;
}

#pragma endregion
