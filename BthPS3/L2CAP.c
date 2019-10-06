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


#pragma region L2CAP remote connection handling

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
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    struct _BRB_L2CA_OPEN_CHANNEL* brb = NULL;
    PFN_WDF_REQUEST_COMPLETION_ROUTINE completionRoutine = NULL;
    USHORT psm = ConnectParams->Parameters.Connect.Request.PSM;
    PBTHPS3_CLIENT_CONNECTION clientConnection = NULL;
    WDFREQUEST brbAsyncRequest = NULL;
    CHAR remoteName[BTH_MAX_NAME_SIZE];
    DS_DEVICE_TYPE deviceType = DS_DEVICE_TYPE_UNKNOWN;


    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

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
                "++ Device %012llX name: %s",
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

        //
        // Check if PLAYSTATION(R)3 Controller
        // 
        if (DevCtx->Settings.IsSIXAXISSupported
            && StringUtil_BthNameIsInCollection(remoteName, DevCtx->Settings.SIXAXISSupportedNames)) {
            deviceType = DS_DEVICE_TYPE_SIXAXIS;

            TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_L2CAP,
                "++ Device %012llX identified as SIXAXIS compatible",
                ConnectParams->BtAddress
            );
        }

        //
        // Check if Navigation Controller
        // 
        if (DevCtx->Settings.IsNAVIGATIONSupported
            && StringUtil_BthNameIsInCollection(remoteName, DevCtx->Settings.NAVIGATIONSupportedNames)) {
            deviceType = DS_DEVICE_TYPE_NAVIGATION;

            TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_L2CAP,
                "++ Device %012llX identified as NAVIGATION compatible",
                ConnectParams->BtAddress
            );
        }

        //
        // Check if Motion Controller
        // 
        if (DevCtx->Settings.IsMOTIONSupported
            && StringUtil_BthNameIsInCollection(remoteName, DevCtx->Settings.MOTIONSupportedNames)) {
            deviceType = DS_DEVICE_TYPE_MOTION;

            TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_L2CAP,
                "++ Device %012llX identified as MOTION compatible",
                ConnectParams->BtAddress
            );
        }

        //
        // Check if Wireless Controller
        // 
        if (DevCtx->Settings.IsWIRELESSSupported
            && StringUtil_BthNameIsInCollection(remoteName, DevCtx->Settings.WIRELESSSupportedNames)) {
            deviceType = DS_DEVICE_TYPE_WIRELESS;

            TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_L2CAP,
                "++ Device %012llX identified as WIRELESS compatible",
                ConnectParams->BtAddress
            );
        }

        //
        // We were not able to identify, drop it
        // 
        if (deviceType == DS_DEVICE_TYPE_UNKNOWN)
        {
            TraceEvents(TRACE_LEVEL_WARNING,
                TRACE_L2CAP,
                "!! Device %012llX not identified or denied, dropping connection",
                ConnectParams->BtAddress
            );

            //
            // Filter re-routed potentially unsupported device, disable
            // 
            if (DevCtx->Settings.AutoDisableFilter)
            {
                status = BthPS3PSM_DisablePatchSync(
                    DevCtx->PsmFilter.IoTarget,
                    0 // TODO: read from registry?
                );
                if (!NT_SUCCESS(status))
                {
                    TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_L2CAP,
                        "BthPS3PSM_DisablePatchSync failed with status %!STATUS!", status);
                }
                else
                {
                    TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_L2CAP,
                        "Filter disabled"
                    );

                    //
                    // Fire off re-enable timer
                    // 
                    if (DevCtx->Settings.AutoEnableFilter)
                    {
                        TraceEvents(TRACE_LEVEL_INFORMATION,
                            TRACE_L2CAP,
                            "Filter disabled, re-enabling in %d seconds",
                            DevCtx->Settings.AutoEnableFilterDelay
                        );

                        (void)WdfTimerStart(
                            DevCtx->PsmFilter.AutoResetTimer,
                            WDF_REL_TIMEOUT_IN_SEC(DevCtx->Settings.AutoEnableFilterDelay)
                        );
                    }
                }
            }

            //
            // Unsupported device, drop connection
            // 
            return L2CAP_PS3_DenyRemoteConnect(DevCtx, ConnectParams);
        }

        //
        // Allocate new connection object
        // 
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

        //
        // Store device type (required to later spawn the right PDO)
        // 
        clientConnection->DeviceType = deviceType;
    }

    //
    // Adjust control flow depending on PSM
    // 
    switch (psm)
    {
    case PSM_DS3_HID_CONTROL:
        completionRoutine = L2CAP_PS3_ControlConnectResponseCompleted;
        clientConnection->HidControlChannel.ChannelHandle = ConnectParams->ConnectionHandle;
        brbAsyncRequest = clientConnection->HidControlChannel.ConnectDisconnectRequest;
        brb = (struct _BRB_L2CA_OPEN_CHANNEL*) & (clientConnection->HidControlChannel.ConnectDisconnectBrb);
        break;
    case PSM_DS3_HID_INTERRUPT:
        completionRoutine = L2CAP_PS3_InterruptConnectResponseCompleted;
        clientConnection->HidInterruptChannel.ChannelHandle = ConnectParams->ConnectionHandle;
        brbAsyncRequest = clientConnection->HidInterruptChannel.ConnectDisconnectRequest;
        brb = (struct _BRB_L2CA_OPEN_CHANNEL*) & (clientConnection->HidInterruptChannel.ConnectDisconnectBrb);
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
    brb->Psm = psm;
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
    // Remaining L2CAP defaults
    // 
    brb->ConfigOut.FlushTO.Max = L2CAP_DEFAULT_FLUSHTO;
    brb->ConfigOut.FlushTO.Min = L2CAP_MIN_FLUSHTO;
    brb->ConfigOut.FlushTO.Preferred = L2CAP_DEFAULT_FLUSHTO;
    brb->ConfigOut.ExtraOptions = 0;
    brb->ConfigOut.NumExtraOptions = 0;
    brb->ConfigOut.LinkTO = 0;

    //
    // Max count of MTUs to stay buffered until discarded
    // 
    brb->IncomingQueueDepth = 10;

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

    //
    // TODO: handle intermediate disconnects
    // 
    if (!NT_SUCCESS(status) && clientConnection)
    {
        ClientConnections_RemoveAndDestroy(DevCtx, clientConnection);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit (%!STATUS!)", status);

    return status;
}

//
// Calls L2CAP_PS3_HandleRemoteConnect at PASSIVE_LEVEL
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
L2CAP_PS3_HandleRemoteConnectAsync(
    _In_ WDFWORKITEM WorkItem
)
{
    PBTHPS3_REMOTE_CONNECT_CONTEXT connectCtx = NULL;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

    connectCtx = GetRemoteConnectContext(WorkItem);

    (void)L2CAP_PS3_HandleRemoteConnect(
        connectCtx->ServerContext,
        &connectCtx->IndicationParameters
    );

    WdfObjectDelete(WorkItem);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");
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
    struct _BRB_L2CA_OPEN_CHANNEL* brb = NULL;
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

    brb = (struct _BRB_L2CA_OPEN_CHANNEL*) Context;
    clientConnection = brb->Hdr.ClientContext[0];

    //
    // Connection acceptance successful, channel ready to operate
    // 
    if (NT_SUCCESS(status))
    {
        WdfSpinLockAcquire(clientConnection->HidControlChannel.ConnectionStateLock);

        clientConnection->HidControlChannel.ConnectionState = ConnectionStateConnected;

        //
        // This will be set again once disconnect has occurred
        // 
        //KeClearEvent(&clientConnection->HidControlChannel.DisconnectEvent);

        WdfSpinLockRelease(clientConnection->HidControlChannel.ConnectionStateLock);

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_L2CAP,
            "HID Control Channel connection established"
        );
    }
    else
    {
        //
        // TODO: implement me!
        // 
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_L2CAP,
            "HID Control Channel connection failed (NOT IMPLEMENTED)"
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
    struct _BRB_L2CA_OPEN_CHANNEL* brb = NULL;
    PBTHPS3_CLIENT_CONNECTION clientConnection = NULL;
    BTHPS3_CONNECTION_STATE controlState;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);

    status = Params->IoStatus.Status;

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_L2CAP,
        "Connection completion, status: %!STATUS!",
        status
    );

    brb = (struct _BRB_L2CA_OPEN_CHANNEL*) Context;
    clientConnection = brb->Hdr.ClientContext[0];

    //
    // Connection acceptance successful, channel ready to operate
    // 
    if (NT_SUCCESS(status))
    {
        WdfSpinLockAcquire(clientConnection->HidInterruptChannel.ConnectionStateLock);

        clientConnection->HidInterruptChannel.ConnectionState = ConnectionStateConnected;

        //
        // This will be set again once disconnect has occurred
        // 
        //KeClearEvent(&clientConnection->HidInterruptChannel.DisconnectEvent);

        WdfSpinLockRelease(clientConnection->HidInterruptChannel.ConnectionStateLock);

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_L2CAP,
            "HID Interrupt Channel connection established"
        );

        //
        // Control channel is expected to be established by now
        // 
        WdfSpinLockAcquire(clientConnection->HidControlChannel.ConnectionStateLock);
        controlState = clientConnection->HidInterruptChannel.ConnectionState;
        WdfSpinLockRelease(clientConnection->HidControlChannel.ConnectionStateLock);

        if (controlState != ConnectionStateConnected)
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_L2CAP,
                "HID Control Channel in invalid state (0x%02X), dropping connection",
                controlState
            );

            goto failedDrop;
        }

        //
        // All expected channel ready to operate, continue child initialization
        // 
        L2CAP_PS3_ConnectionStateConnected(clientConnection);
    }
    else
    {
        goto failedDrop;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");

    return;

failedDrop:

    //
    // TODO: implement me!
    // 
    TraceEvents(TRACE_LEVEL_ERROR,
        TRACE_L2CAP,
        "%!FUNC! connection failed (NOT IMPLEMENTED)"
    );

    return;
}

#pragma endregion

#pragma region L2CAP deny connection request

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
    struct _BRB_L2CA_OPEN_CHANNEL* brb = NULL;


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
    struct _BRB_L2CA_OPEN_CHANNEL* brb = NULL;

    UNREFERENCED_PARAMETER(Target);


    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry (%!STATUS!)",
        Params->IoStatus.Status);

    brb = (struct _BRB_L2CA_OPEN_CHANNEL*)Context;
    deviceCtx = (PBTHPS3_SERVER_CONTEXT)brb->Hdr.ClientContext[0];
    deviceCtx->Header.ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfObjectDelete(Request);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");
}

#pragma endregion

#pragma region L2CAP channel state changes

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
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PBTHPS3_SERVER_CONTEXT deviceCtx = NULL;
    PBTHPS3_CLIENT_CONNECTION connection = NULL;
    PDO_IDENTIFICATION_DESCRIPTION pdoDesc;

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

            L2CAP_PS3_RemoteDisconnect(
                connection->DevCtxHdr,
                connection->RemoteAddress,
                &connection->HidControlChannel
            );
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

            L2CAP_PS3_RemoteDisconnect(
                connection->DevCtxHdr,
                connection->RemoteAddress,
                &connection->HidInterruptChannel
            );
        }

        //
        // Both channels are gone, invoke clean-up
        // 
        if (connection->HidControlChannel.ConnectionState == ConnectionStateDisconnected
            && connection->HidInterruptChannel.ConnectionState == ConnectionStateDisconnected)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_L2CAP,
                "++ Both channels are gone, awaiting clean-up"
            );

            KeWaitForSingleObject(
                &connection->HidControlChannel.DisconnectEvent,
                Executive,
                KernelMode,
                FALSE,
                NULL
            );

            KeWaitForSingleObject(
                &connection->HidInterruptChannel.DisconnectEvent,
                Executive,
                KernelMode,
                FALSE,
                NULL
            );

            WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(
                &pdoDesc.Header,
                sizeof(PDO_IDENTIFICATION_DESCRIPTION)
            );

            pdoDesc.ClientConnection = connection;

            //
            // Init PDO destruction
            // 
            status = WdfChildListUpdateChildDescriptionAsMissing(
                WdfFdoGetDefaultChildList(connection->DevCtxHdr->Device),
                &pdoDesc.Header
            );

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_CONNECTION,
                    "WdfChildListUpdateChildDescriptionAsMissing failed with status %!STATUS!",
                    status);
            }

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

    //
    // PDO creation kicked off from here async
    // 

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

#pragma endregion

#pragma region L2CAP remote disconnect

//
// Instructs a channel to disconnect
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
L2CAP_PS3_RemoteDisconnect(
    _In_ PBTHPS3_DEVICE_CONTEXT_HEADER CtxHdr,
    _In_ BTH_ADDR RemoteAddress,
    _In_ PBTHPS3_CLIENT_L2CAP_CHANNEL Channel
)
{
    struct _BRB_L2CA_CLOSE_CHANNEL* disconnectBrb = NULL;

    WdfSpinLockAcquire(Channel->ConnectionStateLock);

    if (Channel->ConnectionState == ConnectionStateConnecting)
    {
        //
        // If the connection is not completed yet set the state 
        // to disconnecting.
        // In such case we should send CLOSE_CHANNEL Brb down after 
        // we receive connect completion.
        //

        Channel->ConnectionState = ConnectionStateDisconnecting;

        //
        // Clear event to indicate that we are in disconnecting
        // state. It will be set when disconnect is completed
        //
        KeClearEvent(&Channel->DisconnectEvent);

        WdfSpinLockRelease(Channel->ConnectionStateLock);
        return TRUE;
    }

    if (Channel->ConnectionState != ConnectionStateConnected)
    {
        //
        // Do nothing if we are not connected
        //

        WdfSpinLockRelease(Channel->ConnectionStateLock);
        return FALSE;
    }

    Channel->ConnectionState = ConnectionStateDisconnecting;
    WdfSpinLockRelease(Channel->ConnectionStateLock);

    //
    // We are now sending the disconnect, so clear the event.
    //

    KeClearEvent(&Channel->DisconnectEvent);

    CLIENT_CONNECTION_REQUEST_REUSE(Channel->ConnectDisconnectRequest);
    CtxHdr->ProfileDrvInterface.BthReuseBrb(
        &Channel->ConnectDisconnectBrb,
        BRB_L2CA_CLOSE_CHANNEL
    );

    disconnectBrb = (struct _BRB_L2CA_CLOSE_CHANNEL*) & (Channel->ConnectDisconnectBrb);

    disconnectBrb->BtAddress = RemoteAddress;
    disconnectBrb->ChannelHandle = Channel->ChannelHandle;

    //
    // The BRB can fail with STATUS_DEVICE_DISCONNECT if the device is already
    // disconnected, hence we don't assert for success
    //
    (void)BthPS3_SendBrbAsync(
        CtxHdr->IoTarget,
        Channel->ConnectDisconnectRequest,
        (PBRB)disconnectBrb,
        sizeof(*disconnectBrb),
        L2CAP_PS3_ChannelDisconnectCompleted,
        Channel
    );

    return TRUE;
}

//
// Gets called once a channel disconnect request has been completed
// 
void
L2CAP_PS3_ChannelDisconnectCompleted(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
)
{
    PBTHPS3_CLIENT_L2CAP_CHANNEL channel = (PBTHPS3_CLIENT_L2CAP_CHANNEL)Context;

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Entry (%!STATUS!)",
        Params->IoStatus.Status);

    WdfSpinLockAcquire(channel->ConnectionStateLock);
    channel->ConnectionState = ConnectionStateDisconnected;
    WdfSpinLockRelease(channel->ConnectionStateLock);

    //
    // Disconnect complete, set the event
    //
    KeSetEvent(
        &channel->DisconnectEvent,
        0,
        FALSE
    );

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_L2CAP, "%!FUNC! Exit");
}

#pragma endregion

#pragma region L2CAP data transfer (incoming and outgoing)

//
// Submits an outgoing control request
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendControlTransferAsync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    WDFREQUEST Request,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;

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
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbAsync(
        ClientConnection->DevCtxHdr->IoTarget,
        Request,
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
    }

    return status;
}

//
// Submits an incoming control request
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ReadControlTransferAsync(
    PBTHPS3_CLIENT_CONNECTION ClientConnection,
    WDFREQUEST Request,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;

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
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_IN | ACL_SHORT_TRANSFER_OK;
    brb->BufferMDL = NULL;
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbAsync(
        ClientConnection->DevCtxHdr->IoTarget,
        Request,
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
    }

    return status;
}

//
// Submits an incoming interrupt request
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ReadInterruptTransferAsync(
    _In_ PBTHPS3_CLIENT_CONNECTION ClientConnection,
    _In_ WDFREQUEST Request,
    _In_ PVOID Buffer,
    _In_ size_t BufferLength,
    _In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;

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
    brb->ChannelHandle = ClientConnection->HidInterruptChannel.ChannelHandle;
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_IN;
    brb->BufferMDL = NULL;
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbAsync(
        ClientConnection->DevCtxHdr->IoTarget,
        Request,
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
    }

    return status;
}

//
// Submits an outgoing interrupt transfer
// 
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendInterruptTransferAsync(
    _In_ PBTHPS3_CLIENT_CONNECTION ClientConnection,
    _In_ WDFREQUEST Request,
    _In_ PVOID Buffer,
    _In_ size_t BufferLength,
    _In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;

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
    brb->ChannelHandle = ClientConnection->HidInterruptChannel.ChannelHandle;
    brb->TransferFlags = ACL_TRANSFER_DIRECTION_OUT;
    brb->BufferMDL = NULL;
    brb->Buffer = Buffer;
    brb->BufferSize = (ULONG)BufferLength;
    brb->Timeout = 0;
    brb->RemainingBufferSize = 0;

    //
    // Submit request
    // 
    status = BthPS3_SendBrbAsync(
        ClientConnection->DevCtxHdr->IoTarget,
        Request,
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
    }

    return status;
}

//
// Outgoing control transfer has been completed
// 
void
L2CAP_PS3_AsyncSendControlTransferCompleted(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
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

    deviceCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfRequestComplete(Request, Params->IoStatus.Status);
}

//
// Incoming control transfer has been completed
// 
void
L2CAP_PS3_AsyncReadControlTransferCompleted(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
)
{
    size_t length = 0;
    struct _BRB_L2CA_ACL_TRANSFER* brb =
        (struct _BRB_L2CA_ACL_TRANSFER*)Context;
    PBTHPS3_DEVICE_CONTEXT_HEADER deviceCtxHdr =
        (PBTHPS3_DEVICE_CONTEXT_HEADER)brb->Hdr.ClientContext[0];

    UNREFERENCED_PARAMETER(Target);

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_L2CAP,
        "Control read transfer request completed with status %!STATUS!",
        Params->IoStatus.Status
    );

    length = brb->BufferSize;
    deviceCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfRequestCompleteWithInformation(
        Request,
        Params->IoStatus.Status,
        length
    );
}

//
// Incoming interrupt transfer has been completed
// 
void
L2CAP_PS3_AsyncReadInterruptTransferCompleted(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
)
{
    size_t length = 0;
    struct _BRB_L2CA_ACL_TRANSFER* brb =
        (struct _BRB_L2CA_ACL_TRANSFER*)Context;
    PBTHPS3_DEVICE_CONTEXT_HEADER deviceCtxHdr =
        (PBTHPS3_DEVICE_CONTEXT_HEADER)brb->Hdr.ClientContext[0];

    UNREFERENCED_PARAMETER(Target);

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_L2CAP,
        "Interrupt read transfer request completed with status %!STATUS! (remaining: %d)",
        Params->IoStatus.Status,
        brb->RemainingBufferSize
    );

    length = brb->BufferSize;
    deviceCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfRequestCompleteWithInformation(
        Request,
        Params->IoStatus.Status,
        length
    );
}

//
// Outgoing interrupt transfer has been completed
// 
void
L2CAP_PS3_AsyncSendInterruptTransferCompleted(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
)
{
    struct _BRB_L2CA_ACL_TRANSFER* brb =
        (struct _BRB_L2CA_ACL_TRANSFER*)Context;
    PBTHPS3_DEVICE_CONTEXT_HEADER deviceCtxHdr =
        (PBTHPS3_DEVICE_CONTEXT_HEADER)brb->Hdr.ClientContext[0];

    UNREFERENCED_PARAMETER(Target);

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_L2CAP,
        "Interrupt OUT transfer request completed with status %!STATUS!",
        Params->IoStatus.Status
    );

    deviceCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfRequestComplete(Request, Params->IoStatus.Status);
}

#pragma endregion

//
// TODO: obsolete
// 

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendControlTransferSync(
    _In_ PBTHPS3_CLIENT_CONNECTION ClientConnection,
    _In_ WDFREQUEST Request,
    _In_ PVOID Buffer,
    _In_ size_t BufferLength
)
{
    NTSTATUS status;
    struct _BRB_L2CA_ACL_TRANSFER* brb = NULL;

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
        Request,
        (PBRB)brb,
        sizeof(*brb)
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_L2CAP,
            "BthPS3_SendBrbSynchronously failed with status %!STATUS!", status);
    }

    ClientConnection->DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);

    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendInterruptTransferSync(
    _In_ PBTHPS3_CLIENT_CONNECTION ClientConnection,
    _In_ PVOID Buffer,
    _In_ size_t BufferLength
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
