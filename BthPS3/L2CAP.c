/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2022, Nefarius Software Solutions e.U.                      *
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
#include "L2CAP.tmh"


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


    FuncEntry(TRACE_L2CAP);

    //
    // (Try to) refresh settings from registry
    // 
    (void)BthPS3_SettingsContextInit(DevCtx);

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
            TraceInformation(
                TRACE_L2CAP,
                "Device %012llX name: %s",
                ConnectParams->BtAddress,
                remoteName
            );
        }
        else
        {
            TraceError(
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

            TraceInformation(
                TRACE_L2CAP,
                "Device %012llX identified as SIXAXIS compatible",
                ConnectParams->BtAddress
            );
        }

        //
        // Check if Navigation Controller
        // 
        if (DevCtx->Settings.IsNAVIGATIONSupported
            && StringUtil_BthNameIsInCollection(remoteName, DevCtx->Settings.NAVIGATIONSupportedNames)) {
            deviceType = DS_DEVICE_TYPE_NAVIGATION;

            TraceInformation(
                TRACE_L2CAP,
                "Device %012llX identified as NAVIGATION compatible",
                ConnectParams->BtAddress
            );
        }

        //
        // Check if Motion Controller
        // 
        if (DevCtx->Settings.IsMOTIONSupported
            && StringUtil_BthNameIsInCollection(remoteName, DevCtx->Settings.MOTIONSupportedNames)) {
            deviceType = DS_DEVICE_TYPE_MOTION;

            TraceInformation(
                TRACE_L2CAP,
                "Device %012llX identified as MOTION compatible",
                ConnectParams->BtAddress
            );
        }

        //
        // Check if Wireless Controller
        // 
        if (DevCtx->Settings.IsWIRELESSSupported
            && StringUtil_BthNameIsInCollection(remoteName, DevCtx->Settings.WIRELESSSupportedNames)) {
            deviceType = DS_DEVICE_TYPE_WIRELESS;

            TraceInformation(
                TRACE_L2CAP,
                "Device %012llX identified as WIRELESS compatible",
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
                "Device %012llX not identified or denied, dropping connection",
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
                    TraceError(
                        TRACE_L2CAP,
                        "BthPS3PSM_DisablePatchSync failed with status %!STATUS!", status);
                }
                else
                {
                    TraceInformation(
                        TRACE_L2CAP,
                        "Filter disabled"
                    );

                    //
                    // Fire off re-enable timer
                    // 
                    if (DevCtx->Settings.AutoEnableFilter)
                    {
                        TraceInformation(
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
            TraceError(
                TRACE_L2CAP,
                "ClientConnections_CreateAndInsert failed with status %!STATUS!", status);
            goto exit;
        }

        //
        // Store device type (required to later spawn the right PDO)
        // 
        clientConnection->DeviceType = deviceType;

    	//
    	// Store remote name in connection context
    	// 
        status = RtlUnicodeStringPrintf(&clientConnection->RemoteName, L"%hs", remoteName);

        if (!NT_SUCCESS(status)) {
            TraceError(
                TRACE_L2CAP,
                "RtlUnicodeStringPrintf failed with status %!STATUS!", status);
            goto exit;
        }
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
    brb->ConfigIn.Mtu.Preferred = brb->ConfigOut.Mtu.Preferred;

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
    if (!NT_SUCCESS(status = BthPS3_SendBrbAsync(
        DevCtx->Header.IoTarget,
        brbAsyncRequest,
        (PBRB)brb,
        sizeof(*brb),
        completionRoutine,
        brb
    )))
    {
        TraceError( 
            TRACE_L2CAP,
            "BthPS3_SendBrbAsync failed with status %!STATUS!", 
            status
        );
    }

exit:

    //
    // TODO: handle intermediate disconnects
    // 
    if (!NT_SUCCESS(status) && clientConnection)
    {
        ClientConnections_RemoveAndDestroy(&DevCtx->Header, clientConnection);
    }

    FuncExit(TRACE_L2CAP, "status=%!STATUS!", status);

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

    FuncEntry(TRACE_L2CAP);

    connectCtx = GetRemoteConnectContext(WorkItem);

    (void)L2CAP_PS3_HandleRemoteConnect(
        connectCtx->ServerContext,
        &connectCtx->IndicationParameters
    );

    WdfObjectDelete(WorkItem);

    FuncExitNoReturn(TRACE_L2CAP);
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
    
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);

    status = Params->IoStatus.Status;

    FuncEntryArguments(TRACE_L2CAP, "status=%!STATUS!", status);

	
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

        TraceInformation(
            TRACE_L2CAP,
            "HID Control Channel connection established"
        );
    }
    else
    {
        TraceError(
            TRACE_L2CAP,
            "HID Control Channel connection failed with status %!STATUS!",
			status
        );

    	ClientConnections_RemoveAndDestroy(clientConnection->DevCtxHdr, clientConnection);
    }

    FuncExitNoReturn(TRACE_L2CAP);
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

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);
	
	status = Params->IoStatus.Status;
	
    FuncEntryArguments(TRACE_L2CAP, "status=%!STATUS!", status);
        

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

        TraceInformation(
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
            TraceError(
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

    FuncExitNoReturn(TRACE_L2CAP);

    return;

failedDrop:

    TraceError(
        TRACE_L2CAP,
        "Connection failed with status %!STATUS!",
		status
    );

	ClientConnections_RemoveAndDestroy(clientConnection->DevCtxHdr, clientConnection);

    FuncExitNoReturn(TRACE_L2CAP);
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


    FuncEntry(TRACE_L2CAP);

    if (!NT_SUCCESS(status = WdfRequestCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        DevCtx->Header.IoTarget,
        &brbAsyncRequest)))
    {
        TraceError(
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

        TraceError(
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
    if (!NT_SUCCESS(status = BthPS3_SendBrbAsync(
        DevCtx->Header.IoTarget,
        brbAsyncRequest,
        (PBRB)brb,
        sizeof(*brb),
        L2CAP_PS3_DenyRemoteConnectCompleted,
        brb
    )))
    {
        TraceError( 
            TRACE_L2CAP,
            "BthPS3_SendBrbAsync failed with status %!STATUS!",
            status
        );

        DevCtx->Header.ProfileDrvInterface.BthFreeBrb((PBRB)brb);
        WdfObjectDelete(brbAsyncRequest);
    }

    FuncExit(TRACE_L2CAP, "status=%!STATUS!", status);

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


    FuncEntryArguments(TRACE_L2CAP, "status=%!STATUS!", Params->IoStatus.Status);

    brb = (struct _BRB_L2CA_OPEN_CHANNEL*)Context;
    deviceCtx = (PBTHPS3_SERVER_CONTEXT)brb->Hdr.ClientContext[0];
    deviceCtx->Header.ProfileDrvInterface.BthFreeBrb((PBRB)brb);
    WdfObjectDelete(Request);

    FuncExitNoReturn(TRACE_L2CAP);
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

	//
	// TODO: queue work item to lower IRQL instead of this!
	// 

	LARGE_INTEGER timeout;
    timeout.QuadPart = 0;

    FuncEntryArguments(TRACE_L2CAP, "Indication=0x%X, Context=0x%p", 
        Indication, Context);
	

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

        TraceVerbose(
            TRACE_L2CAP,
            "IndicationRemoteDisconnect [0x%p]",
            Parameters->ConnectionHandle);

        connection = (PBTHPS3_CLIENT_CONNECTION)Context;
        deviceCtx = GetServerDeviceContext(connection->DevCtxHdr->Device);

        //
        // HID Control Channel disconnected
        // 
        if (Parameters->ConnectionHandle ==
            connection->HidControlChannel.ChannelHandle)
        {
            TraceVerbose(
                TRACE_L2CAP,
                "HID Control Channel 0x%p disconnected",
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
            TraceVerbose(
                TRACE_L2CAP,
                "HID Interrupt Channel 0x%p disconnected",
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
            TraceVerbose(
                TRACE_L2CAP,
                "Both channels are gone, awaiting clean-up"
            );

            status = KeWaitForSingleObject(
                &connection->HidControlChannel.DisconnectEvent,
                Executive,
                KernelMode,
                FALSE,
                &timeout
            );
        	if(!NT_SUCCESS(status))
        	{
                TraceError(
                    TRACE_L2CAP,
                    "HID Control - KeWaitForSingleObject failed with status %!STATUS!",
                    status
                );

        		//
        		// TODO: react properly to this!
        		// 
        	}

            status = KeWaitForSingleObject(
                &connection->HidInterruptChannel.DisconnectEvent,
                Executive,
                KernelMode,
                FALSE,
                &timeout
            );
            if (!NT_SUCCESS(status))
            {
                TraceError(
                    TRACE_L2CAP,
                    "HID Interrupt - KeWaitForSingleObject failed with status %!STATUS!",
                    status
                );

                //
                // TODO: react properly to this!
                // 
            }

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
                TraceError(
                    TRACE_CONNECTION,
                    "WdfChildListUpdateChildDescriptionAsMissing failed with status %!STATUS!",
                    status);
            }

            ClientConnections_RemoveAndDestroy(&deviceCtx->Header, connection);
        }

        break;

    case IndicationRemoteConfigRequest:

        TraceInformation(
            TRACE_L2CAP,
            "%!FUNC! ++ IndicationRemoteConfigRequest");

        //
        // This catches QOS configuration request and inherently succeeds it
        // 

        break;

    case IndicationRemoteConfigResponse:

        TraceInformation(
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

    FuncExitNoReturn(TRACE_L2CAP);
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

    FuncEntry(TRACE_L2CAP);

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
        TraceError(
            TRACE_L2CAP,
            "WdfChildListAddOrUpdateChildDescriptionAsPresent failed with status %!STATUS!",
            status);
        return status;
    }

    FuncExit(TRACE_L2CAP, "status=%!STATUS!", status);

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

    FuncEntry(TRACE_L2CAP);
	
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
        FuncExit(TRACE_L2CAP, "returns=FALSE");
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

    FuncExit(TRACE_L2CAP, "returns=TRUE");
	
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

    FuncEntryArguments(TRACE_L2CAP, "status=%!STATUS!", Params->IoStatus.Status);
	
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

    FuncExitNoReturn(TRACE_L2CAP);
}

#pragma endregion
