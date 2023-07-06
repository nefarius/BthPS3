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
#include "L2CAP.Connect.tmh"
#include "BthPS3ETW.h"

 //
 // Incoming connection request, prepare and send response
 // 
_IRQL_requires_max_(PASSIVE_LEVEL)
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
    PBTHPS3_PDO_CONTEXT pPdoCtx = NULL;
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
    status = BthPS3_PDO_RetrieveByBthAddr(
        DevCtx,
        ConnectParams->BtAddress,
        &pPdoCtx
    );

    //
    // This device apparently isn't connected, allocate new object
    // 
    if (status == STATUS_NOT_FOUND)
    {
        //
        // Request remote name from radio for device identification
        // 
        if (NT_SUCCESS(status = BthPS3_GetDeviceName(
            DevCtx->Header.IoTarget,
            ConnectParams->BtAddress,
            remoteName
        )))
        {
            TraceInformation(
                TRACE_L2CAP,
                "Device %012llX name: %s",
                ConnectParams->BtAddress,
                remoteName
            );

            EventWriteRemoteDeviceName(NULL, ConnectParams->BtAddress, remoteName);
        }
        else
        {
            TraceError(
                TRACE_L2CAP,
                "BthPS3_GetDeviceName failed with status %!STATUS!, dropping connection",
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
            && StringUtil_BthNameIsInCollection(remoteName, DevCtx->Settings.SIXAXISSupportedNames)) 
        {
            deviceType = DS_DEVICE_TYPE_SIXAXIS;

            TraceInformation(
                TRACE_L2CAP,
                "Device %012llX identified as SIXAXIS compatible",
                ConnectParams->BtAddress
            );

            EventWriteRemoteDeviceIdentified(NULL, ConnectParams->BtAddress, L"SIXAXIS");

            goto deviceIdentified;
        }

        //
        // Check if Navigation Controller
        // 
        if (DevCtx->Settings.IsNAVIGATIONSupported
            && StringUtil_BthNameIsInCollection(remoteName, DevCtx->Settings.NAVIGATIONSupportedNames)) 
        {
            deviceType = DS_DEVICE_TYPE_NAVIGATION;

            TraceInformation(
                TRACE_L2CAP,
                "Device %012llX identified as NAVIGATION compatible",
                ConnectParams->BtAddress
            );

            EventWriteRemoteDeviceIdentified(NULL, ConnectParams->BtAddress, L"NAVIGATION");

            goto deviceIdentified;
        }

        //
        // Check if Motion Controller
        // 
        if (DevCtx->Settings.IsMOTIONSupported
            && StringUtil_BthNameIsInCollection(remoteName, DevCtx->Settings.MOTIONSupportedNames)) 
        {
            deviceType = DS_DEVICE_TYPE_MOTION;

            TraceInformation(
                TRACE_L2CAP,
                "Device %012llX identified as MOTION compatible",
                ConnectParams->BtAddress
            );

            EventWriteRemoteDeviceIdentified(NULL, ConnectParams->BtAddress, L"MOTION");

            goto deviceIdentified;
        }

        //
        // Check if Wireless Controller
        // 
        if (DevCtx->Settings.IsWIRELESSSupported
            && StringUtil_BthNameIsInCollection(remoteName, DevCtx->Settings.WIRELESSSupportedNames))
        {
            deviceType = DS_DEVICE_TYPE_WIRELESS;

            TraceInformation(
                TRACE_L2CAP,
                "Device %012llX identified as WIRELESS compatible",
                ConnectParams->BtAddress
            );

            EventWriteRemoteDeviceIdentified(NULL, ConnectParams->BtAddress, L"WIRELESS");

            goto deviceIdentified;
        }

    deviceIdentified:

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

            EventWriteRemoteDeviceNotIdentified(NULL, ConnectParams->BtAddress);

            //
            // Filter re-routed potentially unsupported device, disable
            // 
            if (DevCtx->Settings.AutoDisableFilter)
            {
                if (!NT_SUCCESS(status = BthPS3PSM_DisablePatchSync(
                    DevCtx->PsmFilter.IoTarget,
                    0
                )))
                {
                    TraceError(
                        TRACE_L2CAP,
                        "BthPS3PSM_DisablePatchSync failed with status %!STATUS!",
                        status
                    );
                }
                else
                {
                    TraceInformation(
                        TRACE_L2CAP,
                        "Filter disabled"
                    );

                    EventWriteAutoDisableFilter(NULL);

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

                        EventWriteAutoEnableFilter(NULL, DevCtx->Settings.AutoEnableFilterDelay);

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
        if (!NT_SUCCESS(status = BthPS3_PDO_Create(
            DevCtx,
            ConnectParams->BtAddress,
            deviceType,
            remoteName,
            &pPdoCtx
        )))
        {
            TraceError(
                TRACE_L2CAP,
                "ClientConnections_CreateAndInsert failed with status %!STATUS!", 
                status
            );
            goto exit;
        }
    }

    if (pPdoCtx == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    //
    // Adjust control flow depending on PSM
    // 
    switch (psm)
    {
    case PSM_DS3_HID_CONTROL:
        completionRoutine = L2CAP_PS3_ControlConnectResponseCompleted;
        pPdoCtx->HidControlChannel.ChannelHandle = ConnectParams->ConnectionHandle;
        brbAsyncRequest = pPdoCtx->HidControlChannel.ConnectDisconnectRequest;
        brb = (struct _BRB_L2CA_OPEN_CHANNEL*)&(pPdoCtx->HidControlChannel.ConnectDisconnectBrb);
        break;
    case PSM_DS3_HID_INTERRUPT:
        completionRoutine = L2CAP_PS3_InterruptConnectResponseCompleted;
        pPdoCtx->HidInterruptChannel.ChannelHandle = ConnectParams->ConnectionHandle;
        brbAsyncRequest = pPdoCtx->HidInterruptChannel.ConnectDisconnectRequest;
        brb = (struct _BRB_L2CA_OPEN_CHANNEL*)&(pPdoCtx->HidInterruptChannel.ConnectDisconnectBrb);
        break;
    default:
        status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    CLIENT_CONNECTION_REQUEST_REUSE(brbAsyncRequest);
    DevCtx->Header.ProfileDrvInterface.BthReuseBrb((PBRB)brb, BRB_L2CA_OPEN_CHANNEL_RESPONSE);

    //
    // Pass connection object along as context
    // 
    brb->Hdr.ClientContext[0] = pPdoCtx;

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
    brb->CallbackContext = pPdoCtx;
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

    if (!NT_SUCCESS(status) && pPdoCtx)
    {
        BthPS3_PDO_Destroy(&DevCtx->Header, pPdoCtx);
    }

    if (!NT_SUCCESS(status))
    {
        EventWriteL2CAPRemoteConnectFailed(NULL, psm, status);
    }

    FuncExit(TRACE_L2CAP, "status=%!STATUS!", status);

    return status;
}
