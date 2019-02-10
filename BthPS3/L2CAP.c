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
#include "l2cap.tmh"


const UCHAR G_Ds3HidOutputReport[] = {
    0x52, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1E, 0xFF, 0x27, 0x10, 0x00,
    0x32, 0xFF, 0x27, 0x10, 0x00, 0x32, 0xFF, 0x27,
    0x10, 0x00, 0x32, 0xFF, 0x27, 0x10, 0x00, 0x32,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

//
// Incoming connection request, prepare and send response
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_HandleRemoteConnect(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx,
    _In_ PINDICATION_PARAMETERS ConnectParams
)
{
    NTSTATUS status;
    struct _BRB_L2CA_OPEN_CHANNEL *brb = NULL;
    PFN_WDF_REQUEST_COMPLETION_ROUTINE completionRoutine = NULL;
    USHORT psm = ConnectParams->Parameters.Connect.Request.PSM;
    PBTHPS3_CLIENT_CONNECTION clientConnection = NULL;
    WDFREQUEST brbAsyncRequest = NULL;
    CHAR remoteName[BTH_MAX_NAME_SIZE];
    DS_DEVICE_TYPE deviceType = DS_DEVICE_TYPE_UNKNOWN;


    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

    //
    // Request remote name from radio for device identification
    // 
    status = BTHPS3_GET_DEVICE_NAME(
        DevCtx->Header.IoTarget,
        ConnectParams->BtAddress,
        remoteName
    );

    if (NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_L2CAP,
            "++ Device %llX name: %s",
            ConnectParams->BtAddress,
            remoteName
        );
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "BTHPS3_GET_DEVICE_NAME failed with status %!STATUS!, dropping connection",
            status
        );

        //
        // Name couldn't be resolved, drop connection
        // 
        return L2CAP_PS3_DenyRemoteConnect(DevCtx, ConnectParams);
    }

    //
    // Distinguish device type based on reported remote name
    // 
    switch (remoteName[0])
    {
    case 'P': // First letter in PLAYSTATION(R)3 Controller ('P')
        deviceType = DS_DEVICE_TYPE_SIXAXIS;
        break;
    case 'N': // First letter in Navigation Controller ('N')
        deviceType = DS_DEVICE_TYPE_NAVIGATION;
        break;
    case 'M': // First letter in Motion Controller ('M')
        deviceType = DS_DEVICE_TYPE_MOTION;
        break;
    case 'W': // First letter in Wireless Controller ('W')
        deviceType = DS_DEVICE_TYPE_WIRELESS;
        break;
    default:
        //
        // Unsupported device, drop connection
        // 
        return L2CAP_PS3_DenyRemoteConnect(DevCtx, ConnectParams);
    }

    //
    // Look for an existing connection object and reuse that
    // 
    status = ClientConnections_RetrieveByBthAddr(
        DevCtx,
        ConnectParams->BtAddress,
        &clientConnection
    );

    //
    // This device apparently isn't connected, allocate new object
    // 
    if (status == STATUS_NOT_FOUND)
    {
        status = ClientConnections_CreateAndInsert(
            DevCtx,
            ConnectParams->BtAddress,
            EvtClientConnectionsDestroyConnection,
            &clientConnection
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_L2CAP,
                "ClientConnections_CreateAndInsert failed with status %!STATUS!", status);
            goto exit;
        }
    }

    clientConnection->DeviceType = deviceType;

    //
    // Adjust control flow depending on PSM
    // 
    switch (psm)
    {
    case PSM_DS3_HID_CONTROL:
        completionRoutine = L2CAP_PS3_ControlConnectResponseCompleted;
        clientConnection->HidControlChannel.ChannelHandle = ConnectParams->ConnectionHandle;
        brbAsyncRequest = clientConnection->HidControlChannel.ConnectDisconnectRequest;
        brb = (struct _BRB_L2CA_OPEN_CHANNEL*) &(clientConnection->HidControlChannel.ConnectDisconnectBrb);
        break;
    case PSM_DS3_HID_INTERRUPT:
        completionRoutine = L2CAP_PS3_InterruptConnectResponseCompleted;
        clientConnection->HidInterruptChannel.ChannelHandle = ConnectParams->ConnectionHandle;
        brbAsyncRequest = clientConnection->HidInterruptChannel.ConnectDisconnectRequest;
        brb = (struct _BRB_L2CA_OPEN_CHANNEL*) &(clientConnection->HidInterruptChannel.ConnectDisconnectBrb);
        break;
    default:
        // Doesn't happen
        break;
    }

    CLIENT_CONNECTION_REQUEST_REUSE(brbAsyncRequest);
    DevCtx->Header.ProfileDrvInterface.BthReuseBrb((PBRB)brb, BRB_L2CA_OPEN_CHANNEL_RESPONSE);

    //
    // Pass connection object along as context
    // 
    brb->Hdr.ClientContext[0] = clientConnection;

    brb->BtAddress = ConnectParams->BtAddress;
    brb->Psm = ConnectParams->Parameters.Connect.Request.PSM;
    brb->ChannelHandle = ConnectParams->ConnectionHandle;
    brb->Response = CONNECT_RSP_RESULT_SUCCESS;

    brb->ChannelFlags = CF_ROLE_EITHER;

    brb->ConfigOut.Flags = 0;
    brb->ConfigIn.Flags = 0;

    //
    // Set expected and preferred MTU to max value
    // 
    brb->ConfigOut.Flags |= CFG_MTU;
    brb->ConfigOut.Mtu.Max = L2CAP_MAX_MTU;
    brb->ConfigOut.Mtu.Min = L2CAP_MIN_MTU;
    brb->ConfigOut.Mtu.Preferred = L2CAP_MAX_MTU;

    brb->ConfigIn.Flags = CFG_MTU;
    brb->ConfigIn.Mtu.Max = brb->ConfigOut.Mtu.Max;
    brb->ConfigIn.Mtu.Min = brb->ConfigOut.Mtu.Min;
    brb->ConfigIn.Mtu.Preferred = brb->ConfigOut.Mtu.Max;

    //
    // Get notifications about disconnect and QOS
    //
    brb->CallbackFlags = CALLBACK_DISCONNECT | CALLBACK_CONFIG_QOS;
    brb->Callback = &L2CAP_PS3_ConnectionIndicationCallback;
    brb->CallbackContext = clientConnection;
    brb->ReferenceObject = (PVOID)WdfDeviceWdmGetDeviceObject(DevCtx->Header.Device);

    //
    // Submit response
    // 
    status = BthPS3_SendBrbAsync(
        DevCtx->Header.IoTarget,
        brbAsyncRequest,
        (PBRB)brb,
        sizeof(*brb),
        completionRoutine,
        brb
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_L2CAP,
            "BthPS3_SendBrbAsync failed with status %!STATUS!", status);
    }

exit:

    if (!NT_SUCCESS(status) && clientConnection)
    {
        ClientConnections_RemoveAndDestroy(DevCtx, clientConnection);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit (%!STATUS!)", status);

    return status;
}

//
// Deny an L2CAP connection request
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_DenyRemoteConnect(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx,
    _In_ PINDICATION_PARAMETERS ConnectParams
)
{
    NTSTATUS status = STATUS_SUCCESS;
    WDFREQUEST brbAsyncRequest = NULL;
    struct _BRB_L2CA_OPEN_CHANNEL *brb = NULL;


    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

    status = WdfRequestCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        DevCtx->Header.IoTarget,
        &brbAsyncRequest);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "WdfRequestCreate failed with status %!STATUS!",
            status
        );

        return status;
    }

    brb = (struct _BRB_L2CA_OPEN_CHANNEL*)
        DevCtx->Header.ProfileDrvInterface.BthAllocateBrb(
            BRB_L2CA_OPEN_CHANNEL_RESPONSE,
            POOLTAG_BTHPS3
        );

    if (brb == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;

        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "Failed to allocate brb BRB_L2CA_OPEN_CHANNEL_RESPONSE with status %!STATUS!",
            status
        );

        WdfObjectDelete(brbAsyncRequest);
        return status;
    }

    brb->Hdr.ClientContext[0] = DevCtx;

    brb->BtAddress = ConnectParams->BtAddress;
    brb->Psm = ConnectParams->Parameters.Connect.Request.PSM;
    brb->ChannelHandle = ConnectParams->ConnectionHandle;

    //
    // Drop connection
    // 
    brb->Response = CONNECT_RSP_RESULT_PSM_NEG;

    brb->ChannelFlags = CF_ROLE_EITHER;

    brb->ConfigOut.Flags = 0;
    brb->ConfigIn.Flags = 0;

    //
    // Submit response
    // 
    status = BthPS3_SendBrbAsync(
        DevCtx->Header.IoTarget,
        brbAsyncRequest,
        (PBRB)brb,
        sizeof(*brb),
        L2CAP_PS3_DenyRemoteConnectCompleted,
        brb
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_L2CAP,
            "BthPS3_SendBrbAsync failed with status %!STATUS!", status);

        DevCtx->Header.ProfileDrvInterface.BthFreeBrb((PBRB)brb);
        WdfObjectDelete(brbAsyncRequest);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");

    return status;
}

//
// Free resources used by deny connection response
// 
void
L2CAP_PS3_DenyRemoteConnectCompleted(
    _In_ WDFREQUEST  Request,
    _In_ WDFIOTARGET  Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS  Params,
    _In_ WDFCONTEXT  Context
)
{
    PBTHPS3_SERVER_CONTEXT deviceCtx = NULL;
    struct _BRB_L2CA_OPEN_CHANNEL *brb = NULL;

    UNREFERENCED_PARAMETER(Target);


    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry (%!STATUS!)",
        Params->IoStatus.Status);

    brb = (struct _BRB_L2CA_OPEN_CHANNEL*)Context;
    deviceCtx = (PBTHPS3_SERVER_CONTEXT)brb->Hdr.ClientContext[0];
    deviceCtx->Header.ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfObjectDelete(Request);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");
}

void
L2CAP_PS3_ConnectResponseCompleted(
    _In_ WDFREQUEST  Request,
    _In_ WDFIOTARGET  Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS  Params,
    _In_ WDFCONTEXT  Context
)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

    NTSTATUS status;
    struct _BRB_L2CA_OPEN_CHANNEL *brb;
    PBTHPS3_CONNECTION connection;
    WDFOBJECT connectionObject;


    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Request); //we reuse the request, hence it is not needed here

    status = Params->IoStatus.Status;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_L2CAP,
        "Connection completion, status: %!STATUS!", status);

    brb = (struct _BRB_L2CA_OPEN_CHANNEL *) Context;

    //
    // We receive connection object as the context in the BRB
    //
    connectionObject = (WDFOBJECT)brb->Hdr.ClientContext[0];
    connection = GetConnectionObjectContext(connectionObject);

    if (NT_SUCCESS(status))
    {
        connection->ChannelHandle = brb->ChannelHandle;
        connection->RemoteAddress = brb->BtAddress;

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_L2CAP,
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

            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_L2CAP,
                "Remote connect response completion: "
                "disconnect has been received "
                "for connection 0x%p", connection);

            //BthEchoConnectionObjectRemoteDisconnect(connection->DevCtxHdr, connection);
        }
        else
        {
            connection->ConnectionState = ConnectionStateConnected;
            WdfSpinLockRelease(connection->ConnectionLock);

            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_L2CAP,
                "Connection completed, connection: 0x%p", connection);

            //
            // Call BthEchoSrvConnectionStateConnected to perform post connect processing
            // (namely initializing continuous readers)
            //
            status = L2CAP_PS3_ConnectionStateConnected(connectionObject);
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
        //REMOVE_CONNECTION_ENTRY_LOCKED(
        //    (PBTHPS3_SERVER_CONTEXT)connection->DevCtxHdr,
        //    &connection->ConnectionListEntry
        //);

        WdfObjectDelete(connectionObject);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");

    return;
}

//
// Control channel connection result
// 
void
L2CAP_PS3_ControlConnectResponseCompleted(
    _In_ WDFREQUEST  Request,
    _In_ WDFIOTARGET  Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS  Params,
    _In_ WDFCONTEXT  Context
)
{
    NTSTATUS status;
    struct _BRB_L2CA_OPEN_CHANNEL *brb = NULL;
    PBTHPS3_CLIENT_CONNECTION clientConnection = NULL;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);

    status = Params->IoStatus.Status;

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_L2CAP,
        "Connection completion, status: %!STATUS!",
        status
    );

    brb = (struct _BRB_L2CA_OPEN_CHANNEL *) Context;
    clientConnection = brb->Hdr.ClientContext[0];

    //
    // Connection acceptance successful, channel ready to operate
    // 
    if (NT_SUCCESS(status))
    {
        WdfSpinLockAcquire(clientConnection->HidControlChannel.ConnectionStateLock);
        clientConnection->HidControlChannel.ConnectionState = ConnectionStateConnected;
        WdfSpinLockRelease(clientConnection->HidControlChannel.ConnectionStateLock);

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_L2CAP,
            "HID Control Channel connection established"
        );
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");
}

//
// Interrupt channel connection result
// 
void
L2CAP_PS3_InterruptConnectResponseCompleted(
    _In_ WDFREQUEST  Request,
    _In_ WDFIOTARGET  Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS  Params,
    _In_ WDFCONTEXT  Context
)
{
    NTSTATUS status;
    struct _BRB_L2CA_OPEN_CHANNEL *brb = NULL;
    PBTHPS3_CLIENT_CONNECTION clientConnection = NULL;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);

    status = Params->IoStatus.Status;

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_L2CAP,
        "Connection completion, status: %!STATUS!",
        status
    );

    brb = (struct _BRB_L2CA_OPEN_CHANNEL *) Context;
    clientConnection = brb->Hdr.ClientContext[0];

    //
    // Connection acceptance successful, channel ready to operate
    // 
    if (NT_SUCCESS(status))
    {
        WdfSpinLockAcquire(clientConnection->HidInterruptChannel.ConnectionStateLock);
        clientConnection->HidInterruptChannel.ConnectionState = ConnectionStateConnected;
        WdfSpinLockRelease(clientConnection->HidInterruptChannel.ConnectionStateLock);

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_L2CAP,
            "HID Interrupt Channel connection established"
        );

        L2CAP_PS3_ConnectionStateConnected(clientConnection);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");
}

//
// Gets invoked on remote disconnect or configuration request
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
void
L2CAP_PS3_ConnectionIndicationCallback(
    _In_ PVOID Context,
    _In_ INDICATION_CODE Indication,
    _In_ PINDICATION_PARAMETERS Parameters
)
{
    PBTHPS3_SERVER_CONTEXT deviceCtx = NULL;
    PBTHPS3_CLIENT_CONNECTION connection = NULL;

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_L2CAP,
        "%!FUNC! Entry (Indication: 0x%X, Context: 0x%p)",
        Indication, Context
    );

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

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_L2CAP,
            "++ IndicationRemoteDisconnect [0x%p]",
            Parameters->ConnectionHandle);

        connection = (PBTHPS3_CLIENT_CONNECTION)Context;
        deviceCtx = GetServerDeviceContext(connection->DevCtxHdr->Device);

        //
        // HID Control Channel disconnected
        // 
        if (Parameters->ConnectionHandle ==
            connection->HidControlChannel.ChannelHandle)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_L2CAP,
                "++ HID Control Channel 0x%p disconnected",
                Parameters->ConnectionHandle);

            WdfSpinLockAcquire(connection->HidControlChannel.ConnectionStateLock);
            connection->HidControlChannel.ConnectionState = ConnectionStateDisconnected;
            WdfSpinLockRelease(connection->HidControlChannel.ConnectionStateLock);
        }

        //
        // HID Interrupt Channel disconnected
        // 
        if (Parameters->ConnectionHandle ==
            connection->HidInterruptChannel.ChannelHandle)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_L2CAP,
                "++ HID Interrupt Channel 0x%p disconnected",
                Parameters->ConnectionHandle);

            WdfSpinLockAcquire(connection->HidInterruptChannel.ConnectionStateLock);
            connection->HidInterruptChannel.ConnectionState = ConnectionStateDisconnected;
            WdfSpinLockRelease(connection->HidInterruptChannel.ConnectionStateLock);
        }

        //
        // Both channels are gone, invoke clean-up
        // 
        if (connection->HidControlChannel.ConnectionState == ConnectionStateDisconnected
            && connection->HidInterruptChannel.ConnectionState == ConnectionStateDisconnected)
        {
            ClientConnections_RemoveAndDestroy(deviceCtx, connection);
        }

        break;

    case IndicationRemoteConfigRequest:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_L2CAP,
            "%!FUNC! ++ IndicationRemoteConfigRequest");

        //
        // This catches QOS configuration request and inherently succeeds it
        // 

        break;

    case IndicationRemoteConfigResponse:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_L2CAP,
            "%!FUNC! ++ IndicationRemoteConfigResponse");

        break;

    case IndicationFreeExtraOptions:
        break;
    default:
        //
        // We don't expect any other indications on this callback
        //
        NT_ASSERT(FALSE);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");
}

//
// Connection has been fully established (both channels)
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ConnectionStateConnected(
    PBTHPS3_CLIENT_CONNECTION ClientConnection
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PDO_IDENTIFICATION_DESCRIPTION pdoDesc;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(ClientConnection);

    status = L2CAP_PS3_SendControlTransfer(
        ClientConnection,
        (PVOID)G_Ds3HidOutputReport,
        DS3_HID_OUTPUT_REPORT_SIZE,
        L2CAP_PS3_ControlTransferCompleted
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "L2CAP_PS3_SendControlTransfer failed with status %!STATUS!",
            status
        );

        return status;
    }

    WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(
        &pdoDesc.Header,
        sizeof(PDO_IDENTIFICATION_DESCRIPTION)
    );

    pdoDesc.ClientConnection = ClientConnection;

    //
    // Invoke new child creation
    // 
    status = WdfChildListAddOrUpdateChildDescriptionAsPresent(
        WdfFdoGetDefaultChildList(ClientConnection->DevCtxHdr->Device),
        &pdoDesc.Header,
        NULL
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "WdfChildListAddOrUpdateChildDescriptionAsPresent failed with status %!STATUS!",
            status);
        return status;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");

    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendControlTransfer(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;
    WDFREQUEST brbAsyncRequest = NULL;

    //
    // Allocate request
    // 
    status = WdfRequestCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        ClientConnection->DevCtxHdr->IoTarget,
        &brbAsyncRequest);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "WdfRequestCreate failed with status %!STATUS!",
            status
        );

        return status;
    }

    //
    // Allocate BRB
    // 
    brb = (struct _BRB_L2CA_ACL_TRANSFER*)
        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
            BRB_L2CA_ACL_TRANSFER,
            POOLTAG_BTHPS3
        );

    if (brb == NULL)
    {
        WdfObjectDelete(brbAsyncRequest);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Used in completion routine to free BRB
    // 
    brb->Hdr.ClientContext[0] = ClientConnection->DevCtxHdr;

    //
    // Set channel properties
    // 
    brb->BtAddress = ClientConnection->RemoteAddress;
    brb->ChannelHandle = ClientConnection->HidControlChannel.ChannelHandle;
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_OUT;
    brb->BufferMDL = NULL;

    //
    // Allocate and fill actual payload buffer
    // 
    brb->Buffer = ExAllocatePoolWithTag(
        NonPagedPoolNx,
        BufferLength,
        POOLTAG_BTHPS3
    );
    RtlCopyMemory(brb->Buffer, Buffer, BufferLength);
    brb->BufferSize = (ULONG)BufferLength;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbAsync(
        ClientConnection->DevCtxHdr->IoTarget,
        brbAsyncRequest,
        (PBRB)brb,
        sizeof(*brb),
        CompletionRoutine,
        brb
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_L2CAP,
            "BthPS3_SendBrbAsync failed with status %!STATUS!", status);

        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
        WdfObjectDelete(brbAsyncRequest);
    }

    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendControlTransferAsync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine,
    PVOID CompletionContext
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;
    WDFREQUEST brbAsyncRequest = NULL;

    //
    // Allocate request
    // 
    status = WdfRequestCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        ClientConnection->DevCtxHdr->IoTarget,
        &brbAsyncRequest);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "WdfRequestCreate failed with status %!STATUS!",
            status
        );

        return status;
    }

    //
    // Allocate BRB
    // 
    brb = (struct _BRB_L2CA_ACL_TRANSFER*)
        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
            BRB_L2CA_ACL_TRANSFER,
            POOLTAG_BTHPS3
        );

    if (brb == NULL)
    {
        WdfObjectDelete(brbAsyncRequest);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Used in completion routine to free BRB
    // 
    brb->Hdr.ClientContext[0] = ClientConnection->DevCtxHdr;
    brb->Hdr.ClientContext[1] = CompletionContext;

    //
    // Set channel properties
    // 
    brb->BtAddress = ClientConnection->RemoteAddress;
    brb->ChannelHandle = ClientConnection->HidControlChannel.ChannelHandle;
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_OUT;
    brb->BufferMDL = NULL;
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbAsync(
        ClientConnection->DevCtxHdr->IoTarget,
        brbAsyncRequest,
        (PBRB)brb,
        sizeof(*brb),
        CompletionRoutine,
        brb
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_L2CAP,
            "BthPS3_SendBrbAsync failed with status %!STATUS!", status);

        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
        WdfObjectDelete(brbAsyncRequest);
    }

    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendControlTransferSync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    PVOID Buffer,
    size_t BufferLength
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;
    WDFREQUEST brbSyncRequest = NULL;

    //
    // Allocate request
    // 
    status = WdfRequestCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        ClientConnection->DevCtxHdr->IoTarget,
        &brbSyncRequest);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "WdfRequestCreate failed with status %!STATUS!",
            status
        );

        return status;
    }

    //
    // Allocate BRB
    // 
    brb = (struct _BRB_L2CA_ACL_TRANSFER*)
        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
            BRB_L2CA_ACL_TRANSFER,
            POOLTAG_BTHPS3
        );

    if (brb == NULL)
    {
        WdfObjectDelete(brbSyncRequest);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Set channel properties
    // 
    brb->BtAddress = ClientConnection->RemoteAddress;
    brb->ChannelHandle = ClientConnection->HidControlChannel.ChannelHandle;
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_OUT;
    brb->BufferMDL = NULL;
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbSynchronously(
        ClientConnection->DevCtxHdr->IoTarget,
        brbSyncRequest,
        (PBRB)brb,
        sizeof(*brb)
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_L2CAP,
            "BthPS3_SendBrbSynchronously failed with status %!STATUS!", status);
    }

    ClientConnection->DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfObjectDelete(brbSyncRequest);

    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendInterruptTransferSync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    PVOID Buffer,
    size_t BufferLength
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;
    WDFREQUEST brbSyncRequest = NULL;

    //
    // Allocate request
    // 
    status = WdfRequestCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        ClientConnection->DevCtxHdr->IoTarget,
        &brbSyncRequest);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "WdfRequestCreate failed with status %!STATUS!",
            status
        );

        return status;
    }

    //
    // Allocate BRB
    // 
    brb = (struct _BRB_L2CA_ACL_TRANSFER*)
        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
            BRB_L2CA_ACL_TRANSFER,
            POOLTAG_BTHPS3
        );

    if (brb == NULL)
    {
        WdfObjectDelete(brbSyncRequest);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Set channel properties
    // 
    brb->BtAddress = ClientConnection->RemoteAddress;
    brb->ChannelHandle = ClientConnection->HidInterruptChannel.ChannelHandle;
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_OUT;
    brb->BufferMDL = NULL;
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbSynchronously(
        ClientConnection->DevCtxHdr->IoTarget,
        brbSyncRequest,
        (PBRB)brb,
        sizeof(*brb)
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_L2CAP,
            "BthPS3_SendBrbSynchronously failed with status %!STATUS!", status);
    }

    ClientConnection->DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfObjectDelete(brbSyncRequest);

    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ReadInterruptTransferSync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    PVOID Buffer,
    size_t BufferLength,
    PULONG_PTR BytesReturned
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;
    WDFREQUEST brbSyncRequest = NULL;

    //
    // Allocate request
    // 
    status = WdfRequestCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        ClientConnection->DevCtxHdr->IoTarget,
        &brbSyncRequest);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "WdfRequestCreate failed with status %!STATUS!",
            status
        );

        return status;
    }

    //
    // Allocate BRB
    // 
    brb = (struct _BRB_L2CA_ACL_TRANSFER*)
        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
            BRB_L2CA_ACL_TRANSFER,
            POOLTAG_BTHPS3
        );

    if (brb == NULL)
    {
        WdfObjectDelete(brbSyncRequest);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Set channel properties
    // 
    brb->BtAddress = ClientConnection->RemoteAddress;
    brb->ChannelHandle = ClientConnection->HidInterruptChannel.ChannelHandle;
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_IN | ACL_TRANSFER_TIMEOUT;
    brb->Timeout = 200; // should be high enough
    brb->BufferMDL = NULL;
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbSynchronously(
        ClientConnection->DevCtxHdr->IoTarget,
        brbSyncRequest,
        (PBRB)brb,
        sizeof(*brb)
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_L2CAP,
            "BthPS3_SendBrbSynchronously failed with status %!STATUS!", status);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_L2CAP,
        "brb->BufferSize: %d, brb->RemainingBufferSize: %d",
        brb->BufferSize,
        brb->RemainingBufferSize
    );

    TraceDumpBuffer(brb->Buffer, brb->BufferSize);

    *BytesReturned = brb->BufferSize;
    ClientConnection->DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfObjectDelete(brbSyncRequest);

    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ReadInterruptTransferAsync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine,
    WDFCONTEXT CompletionContext
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;
    WDFREQUEST brbAsyncRequest = NULL;

    //
    // Allocate request
    // 
    status = WdfRequestCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        ClientConnection->DevCtxHdr->IoTarget,
        &brbAsyncRequest);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "WdfRequestCreate failed with status %!STATUS!",
            status
        );

        return status;
    }

    //
    // Allocate BRB
    // 
    brb = (struct _BRB_L2CA_ACL_TRANSFER*)
        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
            BRB_L2CA_ACL_TRANSFER,
            POOLTAG_BTHPS3
        );

    if (brb == NULL)
    {
        WdfObjectDelete(brbAsyncRequest);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Used in completion routine to free BRB
    // 
    brb->Hdr.ClientContext[0] = ClientConnection->DevCtxHdr;
    brb->Hdr.ClientContext[1] = CompletionContext;

    //
    // Set channel properties
    // 
    brb->BtAddress = ClientConnection->RemoteAddress;
    brb->ChannelHandle = ClientConnection->HidControlChannel.ChannelHandle;
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_IN | ACL_SHORT_TRANSFER_OK;
    brb->BufferMDL = NULL;
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbAsync(
        ClientConnection->DevCtxHdr->IoTarget,
        brbAsyncRequest,
        (PBRB)brb,
        sizeof(*brb),
        CompletionRoutine,
        brb
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_L2CAP,
            "BthPS3_SendBrbAsync failed with status %!STATUS!", status);

        ClientConnection->DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
        WdfObjectDelete(brbAsyncRequest);
    }

    return status;
}

void
L2CAP_PS3_ReadInterruptTransferCompleted(
    _In_ WDFREQUEST  Request,
    _In_ WDFIOTARGET  Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS  Params,
    _In_ WDFCONTEXT  Context
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;
    PBTHPS3_DEVICE_CONTEXT_HEADER deviceCtxHdr = NULL;
    //WDFREQUEST pdoRequest = NULL;

    UNREFERENCED_PARAMETER(Target);

    status = Params->IoStatus.Status;

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_L2CAP,
        "Interrupt IN transfer request completed with status %!STATUS!",
        status
    );

    brb = (struct _BRB_L2CA_ACL_TRANSFER*)Context;
    deviceCtxHdr = (PBTHPS3_DEVICE_CONTEXT_HEADER)brb->Hdr.ClientContext[0];
    //pdoRequest = (WDFREQUEST)brb->Hdr.ClientContext[1];

    //WdfRequestCompleteWithInformation(pdoRequest, status, (brb->BufferSize - brb->RemainingBufferSize));
    deviceCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfObjectDelete(Request);
}

//
// Request sent to HID control channel got completed
// 
void
L2CAP_PS3_ControlTransferCompleted(
    _In_ WDFREQUEST  Request,
    _In_ WDFIOTARGET  Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS  Params,
    _In_ WDFCONTEXT  Context
)
{
    struct _BRB_L2CA_ACL_TRANSFER* brb =
        (struct _BRB_L2CA_ACL_TRANSFER*)Context;
    PBTHPS3_DEVICE_CONTEXT_HEADER deviceCtxHdr =
        (PBTHPS3_DEVICE_CONTEXT_HEADER)brb->Hdr.ClientContext[0];

    UNREFERENCED_PARAMETER(Target);

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_L2CAP,
        "Control transfer request completed with status %!STATUS!",
        Params->IoStatus.Status
    );

    ExFreePoolWithTag(brb->Buffer, POOLTAG_BTHPS3);
    deviceCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfObjectDelete(Request);
}
