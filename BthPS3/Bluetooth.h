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


#pragma once

#include <bthdef.h>
#include <bthguid.h>
#include <bthioctl.h>
#include <sdpnode.h>
#include <bthddi.h>
#include <bthsdpddi.h>
#include <bthsdpdef.h>

#define POOLTAG_BTHPS3                  '3SPB'
#define BTH_DEVICE_INFO_MAX_COUNT       0x0A
#define BTH_DEVICE_INFO_MAX_RETRIES     0x05

typedef struct _BTHPS3_DEVICE_CONTEXT_HEADER
{
    //
    // Framework device this context is associated with
    //
    WDFDEVICE Device;

    //
    // Default I/O target
    //
    WDFIOTARGET IoTarget;

    //
    // Profile driver interface which contains profile driver DDI
    //
    BTH_PROFILE_DRIVER_INTERFACE ProfileDrvInterface;

    //
    // Local Bluetooth Address
    //
    BTH_ADDR LocalBthAddr;

    //
    // Preallocated request to be reused during initialization/deinitialization phase
    // Access to this request is not synchronized
    //
    WDFREQUEST HostInitRequest;

} BTHPS3_DEVICE_CONTEXT_HEADER, *PBTHPS3_DEVICE_CONTEXT_HEADER;

typedef struct _BTHPS3_SERVER_CONTEXT
{
    //
    // Context common to client and server
    //
    BTHPS3_DEVICE_CONTEXT_HEADER Header;

    //
    // Artificial HID Control PSM
    // 
    USHORT PsmHidControl;

    //
    // Artificial HID Interrupt PSM
    // 
    USHORT PsmHidInterrupt;

    //
    // Handle obtained by registering L2CAP server
    //
    L2CAP_SERVER_HANDLE L2CAPServerHandle;

    //
    // BRB used for server and PSM register and unregister
    //
    // Server and PSM register and unregister must be done
    // sequentially since access to this brb is not
    // synchronized.
    //
    struct _BRB RegisterUnregisterBrb;

    //
    // Collection of state information about 
    // currently established connections
    // 
    WDFCOLLECTION ClientConnections;

    //
    // Lock for ClientConnections collection
    // 
    WDFSPINLOCK ClientConnectionsLock;

    struct
    {
        //
        // Remote I/O target (PSM filter driver)
        // 
        WDFIOTARGET IoTarget;

        //
        // Delayed action to re-enable filter patch
        // 
        WDFTIMER AutoResetTimer;

        //
        // Request object used to asynchronously enable the patch
        // 
        WDFREQUEST AsyncRequest;

    } PsmFilter;

} BTHPS3_SERVER_CONTEXT, *PBTHPS3_SERVER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_SERVER_CONTEXT, GetServerDeviceContext)

typedef struct _BTHPS3_REMOTE_CONNECT_CONTEXT
{
    PBTHPS3_SERVER_CONTEXT ServerContext;

    INDICATION_PARAMETERS IndicationParameters;

} BTHPS3_REMOTE_CONNECT_CONTEXT, *PBTHPS3_REMOTE_CONNECT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BTHPS3_REMOTE_CONNECT_CONTEXT, GetRemoteConnectContext)

EVT_WDF_TIMER BthPS3_EnablePatchEvtWdfTimer;


_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RetrieveLocalInfo(
    _In_ PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr
);

#pragma region PSM Registration

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RegisterPSM(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3_UnregisterPSM(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

#pragma endregion

#pragma region L2CAP Server

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RegisterL2CAPServer(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BthPS3_UnregisterL2CAPServer(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

#pragma endregion

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3_DeviceContextHeaderInit(
    PBTHPS3_DEVICE_CONTEXT_HEADER Header,
    WDFDEVICE Device
);

NTSTATUS
FORCEINLINE
BTHPS3_SERVER_CONTEXT_INIT(
    PBTHPS3_SERVER_CONTEXT Context,
    WDFDEVICE Device
)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_TIMER_CONFIG timerCfg;

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

exit:
    return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_QueryInterfaces(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_Initialize(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx
);

_IRQL_requires_max_(DISPATCH_LEVEL)
void
BthPS3_IndicationCallback(
    _In_ PVOID Context,
    _In_ INDICATION_CODE Indication,
    _In_ PINDICATION_PARAMETERS Parameters
);

#pragma region BRB Request submission

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_SendBrbSynchronously(
    _In_ WDFIOTARGET IoTarget,
    _In_ WDFREQUEST Request,
    _In_ PBRB Brb,
    _In_ ULONG BrbSize
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3_SendBrbAsync(
    _In_ WDFIOTARGET IoTarget,
    _In_ WDFREQUEST Request,
    _In_ PBRB Brb,
    _In_ size_t BrbSize,
    _In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE ComplRoutine,
    _In_opt_ WDFCONTEXT Context
);

#pragma endregion

//
// Request remote device friendly name from radio
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FORCEINLINE
BTHPS3_GET_DEVICE_NAME(
    WDFIOTARGET IoTarget,
    BTH_ADDR RemoteAddress,
    PCHAR Name
)
{
    NTSTATUS status = STATUS_INVALID_BUFFER_SIZE;
    ULONG index = 0;
    WDF_MEMORY_DESCRIPTOR MemoryDescriptor;
    WDFMEMORY MemoryHandle = NULL;
    PBTH_DEVICE_INFO_LIST pDeviceInfoList = NULL;
    ULONG maxDevices = BTH_DEVICE_INFO_MAX_COUNT;
    ULONG retryCount = 0;

    //
    // Retry increasing the buffer a few times if _a lot_ of devices
    // are cached and the allocated memory can't store them all.
    // 
    for (retryCount = 0; (retryCount <= BTH_DEVICE_INFO_MAX_RETRIES
        && status == STATUS_INVALID_BUFFER_SIZE); retryCount++)
    {
        if (MemoryHandle != NULL) {
            WdfObjectDelete(MemoryHandle);
        }

        status = WdfMemoryCreate(NULL,
            NonPagedPool,
            POOLTAG_BTHPS3,
            sizeof(BTH_DEVICE_INFO_LIST) + (sizeof(BTH_DEVICE_INFO) * maxDevices),
            &MemoryHandle,
            NULL);

        if (!NT_SUCCESS(status)) {
            return status;
        }

        WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(
            &MemoryDescriptor,
            MemoryHandle,
            NULL
        );

        status = WdfIoTargetSendIoctlSynchronously(
            IoTarget,
            NULL,
            IOCTL_BTH_GET_DEVICE_INFO,
            &MemoryDescriptor,
            &MemoryDescriptor,
            NULL,
            NULL
        );

        //
        // Increase memory to allocate
        // 
        maxDevices += BTH_DEVICE_INFO_MAX_COUNT;
    }

    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(MemoryHandle);
        return status;
    }

    pDeviceInfoList = WdfMemoryGetBuffer(MemoryHandle, NULL);
    status = STATUS_NOT_FOUND;

    for (index = 0; index < pDeviceInfoList->numOfDevices; index++)
    {
        PBTH_DEVICE_INFO pDeviceInfo = &pDeviceInfoList->deviceList[index];

        if (pDeviceInfo->address == RemoteAddress)
        {
            if (strlen(pDeviceInfo->name) == 0)
            {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            strcpy_s(Name, BTH_MAX_NAME_SIZE, pDeviceInfo->name);
            status = STATUS_SUCCESS;
            break;
        }
    }

    WdfObjectDelete(MemoryHandle);
    return status;
}
