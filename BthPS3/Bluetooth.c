#include "Driver.h"
#include "bluetooth.tmh"
#include <bthguid.h>


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

        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "Failed to allocate brb BRB_HCI_GET_LOCAL_BD_ADDR, returning status code %!STATUS!\n", status);

        goto exit;
    }

    status = BthPS3SendBrbSynchronously(
        DevCtxHdr->IoTarget,
        DevCtxHdr->Request,
        (PBRB)brb,
        sizeof(*brb)
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "Retrieving local bth address failed, Status code %!STATUS!\n", status);

        goto exit1;
    }

    DevCtxHdr->LocalBthAddr = brb->BtAddress;

#if (NTDDI_VERSION >= NTDDI_WIN8)

    //
    // Now retrieve local host supported features
    //
    status = BthPS3GetHostSupportedFeatures(DevCtxHdr);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "Sending IOCTL for reading supported features failed, Status code %!STATUS!\n", status);

        goto exit1;
    }

#endif

exit1:
    DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
exit:

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Exit");

    return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3SendBrbSynchronously(
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
        goto exit;
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

exit:
    return status;
}

#if (NTDDI_VERSION >= NTDDI_WIN8)

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3GetHostSupportedFeatures(
    _In_ PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr
)
{
    WDF_MEMORY_DESCRIPTOR outMemDesc = { 0 };
    BTH_HOST_FEATURE_MASK localFeatures = { 0 };
    NTSTATUS status = STATUS_SUCCESS;

    DevCtxHdr->LocalFeatures.Mask = 0;

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
        &outMemDesc,
        &localFeatures,
        sizeof(localFeatures)
    );

    status = WdfIoTargetSendIoctlSynchronously(
        DevCtxHdr->IoTarget,
        NULL,
        IOCTL_BTH_GET_HOST_SUPPORTED_FEATURES,
        NULL,
        &outMemDesc,
        NULL,
        NULL
    );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    DevCtxHdr->LocalFeatures = localFeatures;

    return status;
}

#endif

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

    status = BthPS3SendBrbSynchronously(
        DevCtx->Header.IoTarget,
        DevCtx->Header.Request,
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

    status = BthPS3SendBrbSynchronously(
        DevCtx->Header.IoTarget,
        DevCtx->Header.Request,
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

    if (0 == DevCtx->Psm)
    {
        return;
    }

    DevCtx->Header.ProfileDrvInterface.BthReuseBrb(
        &(DevCtx->RegisterUnregisterBrb),
        BRB_UNREGISTER_PSM
    );

    brb = (struct _BRB_PSM *)
        &(DevCtx->RegisterUnregisterBrb);

    //
    // Format Brb
    //

    brb->Psm = DevCtx->PsmHidControl;

    status = BthPS3SendBrbSynchronously(
        DevCtx->Header.IoTarget,
        DevCtx->Header.Request,
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

    brb->Psm = DevCtx->PsmHidInterrupt;

    status = BthPS3SendBrbSynchronously(
        DevCtx->Header.IoTarget,
        DevCtx->Header.Request,
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
    brb->PSM = 0; //we have already registered the PSM
    brb->IndicationCallback = &BthPS3IndicationCallback;
    brb->IndicationCallbackContext = DevCtx;
    brb->IndicationFlags = 0;
    brb->ReferenceObject = WdfDeviceWdmGetDeviceObject(DevCtx->Header.Device);

    status = BthPS3SendBrbSynchronously(
        DevCtx->Header.IoTarget,
        DevCtx->Header.Request,
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
    // Store server handle
    //
    DevCtx->L2CAPServerHandle = brb->ServerHandle;

exit:

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

    status = BthPS3SendBrbSynchronously(
        DevCtx->Header.IoTarget,
        DevCtx->Header.Request,
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

        goto exit;
    }

    DevCtx->L2CAPServerHandle = NULL;

exit:
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
        &Header->Request
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "Failed to pre-allocate request in device context, Status code %!STATUS!\n", status);

        goto exit;
    }

exit:
    return status;
}

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
            "QueryInterface failed for Interface profile driver interface, version %d, Status code %!STATUS!\n",
            BTHDDI_PROFILE_DRIVER_INTERFACE_VERSION_FOR_QI,
            status);

        goto exit;
    }
exit:
    return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3Initialize(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
    NTSTATUS status;

    PAGED_CODE();

    status = BthPS3QueryInterfaces(DevCtx);
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

exit:
    return status;
}

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
            "%!FUNC! ++ IndicationRemoteConnect");

        UNREFERENCED_PARAMETER(devCtx);
        UNREFERENCED_PARAMETER(Parameters);

        BthPS3SendConnectResponse(devCtx, Parameters);
        break;
    }
    case IndicationRemoteDisconnect:
    {
        //
        // We register BthEchoSrvConnectionIndicationCallback for disconnect
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

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3SendConnectResponse(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx,
    _In_ PINDICATION_PARAMETERS ConnectParams
)
{
    NTSTATUS status, statusReuse;
    WDF_REQUEST_REUSE_PARAMS reuseParams;
    struct _BRB_L2CA_OPEN_CHANNEL *brb = NULL;
    WDFOBJECT connectionObject = NULL;
    PBTHPS3_CONNECTION connection = NULL;


    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Entry");

    //
    // We create the connection object as the first step so that if we receive 
    // remove before connect response is completed
    // we can wait for connection and disconnect.
    //
    status = BthPS3ConnectionObjectCreate(
        &DevCtx->Header,
        DevCtx->Header.Device,
        &connectionObject
    );

    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    connection = GetConnectionObjectContext(connectionObject);

    connection->ConnectionState = ConnectionStateConnecting;

    //
    // Insert the connection object in the connection list that we track
    //
    InsertConnectionEntryLocked(DevCtx, &connection->ConnectionListEntry);

    WDF_REQUEST_REUSE_PARAMS_INIT(&reuseParams, WDF_REQUEST_REUSE_NO_FLAGS, STATUS_NOT_SUPPORTED);
    statusReuse = WdfRequestReuse(connection->ConnectDisconnectRequest, &reuseParams);
    NT_ASSERT(NT_SUCCESS(statusReuse));
    UNREFERENCED_PARAMETER(statusReuse);

    brb = (struct _BRB_L2CA_OPEN_CHANNEL*) &(connection->ConnectDisconnectBrb);
    DevCtx->Header.ProfileDrvInterface.BthReuseBrb((PBRB)brb, BRB_L2CA_OPEN_CHANNEL_RESPONSE);

    brb->Hdr.ClientContext[0] = connectionObject;
    brb->BtAddress = ConnectParams->BtAddress;
    brb->Psm = ConnectParams->Parameters.Connect.Request.PSM;
    brb->ChannelHandle = ConnectParams->ConnectionHandle;
    brb->Response = CONNECT_RSP_RESULT_SUCCESS;

    brb->ChannelFlags = CF_ROLE_EITHER;

    brb->ConfigOut.Flags = 0;
    brb->ConfigIn.Flags = 0;

    brb->ConfigOut.Flags |= CFG_MTU;
    brb->ConfigOut.Mtu.Max = L2CAP_DEFAULT_MTU;
    brb->ConfigOut.Mtu.Min = L2CAP_MIN_MTU;
    brb->ConfigOut.Mtu.Preferred = L2CAP_DEFAULT_MTU;

    brb->ConfigIn.Flags = CFG_MTU;
    brb->ConfigIn.Mtu.Max = brb->ConfigOut.Mtu.Max;
    brb->ConfigIn.Mtu.Min = brb->ConfigOut.Mtu.Min;
    brb->ConfigIn.Mtu.Preferred = brb->ConfigOut.Mtu.Max;

    //
    // Get notifications about disconnect
    //
    brb->CallbackFlags = CALLBACK_DISCONNECT;
    brb->Callback = &BthPS3ConnectionIndicationCallback;
    brb->CallbackContext = connectionObject;
    brb->ReferenceObject = (PVOID)WdfDeviceWdmGetDeviceObject(DevCtx->Header.Device);

    status = BthPS3SendBrbAsync(
        DevCtx->Header.IoTarget,
        connection->ConnectDisconnectRequest,
        (PBRB)brb,
        sizeof(*brb),
        BthPS3RemoteConnectResponseCompletion,
        brb
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BTH,
            "BthPS3SendBrbAsync failed, status = %!STATUS!", status);
    }

exit:

    if (!NT_SUCCESS(status) && connectionObject)
    {
        //
        // If we failed to connect remove connection from list and
        // delete the object
        //
        TraceEvents(TRACE_LEVEL_WARNING, 
            TRACE_BTH, 
            "Failed to establish connection, clean-up");

        //
        // connection should not be NULL if connectionObject is not NULL
        // since first thing we do after creating connectionObject is to
        // get context which gives us connection
        //
        NT_ASSERT(connection != NULL);

        if (connection != NULL)
        {
            connection->ConnectionState = ConnectionStateConnectFailed;

            RemoveConnectionEntryLocked(
                (PBTHPS3_SERVER_CONTEXT)connection->DevCtxHdr,
                &connection->ConnectionListEntry
            );
        }

        WdfObjectDelete(connectionObject);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Exit (%!STATUS!)", status);

    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
BthPS3ConnectionIndicationCallback(
    _In_ PVOID Context,
    _In_ INDICATION_CODE Indication,
    _In_ PINDICATION_PARAMETERS Parameters
)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(Parameters);

    switch (Indication)
    {
    case IndicationAddReference:
        break;
    case IndicationReleaseReference:
        break;
    case IndicationRemoteConnect:
    {
        //
        // We don't expect connect on this callback
        //
        NT_ASSERT(FALSE);
        break;
    }
    case IndicationRemoteDisconnect:
    {
        WDFOBJECT connectionObject = (WDFOBJECT)Context;
        PBTHPS3_CONNECTION connection = GetConnectionObjectContext(connectionObject);

        UNREFERENCED_PARAMETER(connection);

        //BthEchoSrvDisconnectConnection(connection);

        break;
    }
    case IndicationRemoteConfigRequest:
    case IndicationRemoteConfigResponse:
    case IndicationFreeExtraOptions:
        break;
    default:
        //
        // We don't expect any other indications on this callback
        //
        NT_ASSERT(FALSE);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Exit");
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3SendBrbAsync(
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

        goto exit;
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

        goto exit;
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

        goto exit;
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

        goto exit;
    }

exit:
    return status;
}

void
BthPS3RemoteConnectResponseCompletion(
    _In_ WDFREQUEST  Request,
    _In_ WDFIOTARGET  Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS  Params,
    _In_ WDFCONTEXT  Context
)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Entry");

    NTSTATUS status;
    struct _BRB_L2CA_OPEN_CHANNEL *brb;
    PBTHPS3_CONNECTION connection;
    WDFOBJECT connectionObject;


    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Request); //we reuse the request, hence it is not needed here

    status = Params->IoStatus.Status;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BTH,
        "Connection completion, status: %!STATUS!", status);

    brb = (struct _BRB_L2CA_OPEN_CHANNEL *) Context;

    //
    // We receive connection object as the context in the BRB
    //
    connectionObject = (WDFOBJECT)brb->Hdr.ClientContext[0];
    connection = GetConnectionObjectContext(connectionObject);

    if (NT_SUCCESS(status))
    {
        connection->OutMTU = brb->OutResults.Params.Mtu;
        connection->InMTU = brb->InResults.Params.Mtu;
        connection->ChannelHandle = brb->ChannelHandle;
        connection->RemoteAddress = brb->BtAddress;

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BTH,
            "Connection established with client");

        //
        // Check if we already received a disconnect request
        // and if so disconnect
        //

        WdfSpinLockAcquire(connection->ConnectionLock);

        if (connection->ConnectionState == ConnectionStateDisconnecting)
        {
            //
            // We allow transition to disconnected state only from
            // connected state
            //
            // If we are in disconnection state this means that
            // we were waiting for connect to complete before we
            // can send disconnect down
            //
            // Set the state to Connected and call BthEchoConnectionObjectRemoteDisconnect
            //
            connection->ConnectionState = ConnectionStateConnected;
            WdfSpinLockRelease(connection->ConnectionLock);

            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BTH,
                "Remote connect response completion: "
                "disconnect has been received "
                "for connection 0x%p", connection);

            //BthEchoConnectionObjectRemoteDisconnect(connection->DevCtxHdr, connection);
        }
        else
        {
            connection->ConnectionState = ConnectionStateConnected;
            WdfSpinLockRelease(connection->ConnectionLock);

            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BTH,
                "Connection completed, connection: 0x%p", connection);

            //
            // Call BthEchoSrvConnectionStateConnected to perform post connect processing
            // (namely initializing continuous readers)
            //
            //status = BthEchoSrvConnectionStateConnected(connectionObject);
            //if (!NT_SUCCESS(status))
            //{
            //    //
            //    // If the post connect processing failed, let us disconnect
            //    //
            //    BthEchoConnectionObjectRemoteDisconnect(connection->DevCtxHdr, connection);
            //}

        }
    }
    else
    {
        BOOLEAN setDisconnectEvent = FALSE;

        WdfSpinLockAcquire(connection->ConnectionLock);
        if (connection->ConnectionState == ConnectionStateDisconnecting)
        {
            setDisconnectEvent = TRUE;
        }

        connection->ConnectionState = ConnectionStateConnectFailed;
        WdfSpinLockRelease(connection->ConnectionLock);

        if (setDisconnectEvent)
        {
            KeSetEvent(&connection->DisconnectEvent,
                0,
                FALSE);
        }

    }

    if (!NT_SUCCESS(status))
    {
        RemoveConnectionEntryLocked(
            (PBTHPS3_SERVER_CONTEXT)connection->DevCtxHdr,
            &connection->ConnectionListEntry
        );

        WdfObjectDelete(connectionObject);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BTH, "%!FUNC! Exit");

    return;
}
