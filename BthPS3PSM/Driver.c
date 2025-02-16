/**********************************************************************************
 *                                                                                *
 * BthPS3PSM - Windows kernel-mode BTHUSB lower filter driver                     *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2025, Nefarius Software Solutions e.U.                      *
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


#include "driver.h"
#include "driver.tmh"
#include "BthPS3PSMETW.h"

#ifdef BTHPS3PSM_WITH_CONTROL_DEVICE
extern WDFCOLLECTION FilterDeviceCollection;
extern WDFWAITLOCK FilterDeviceCollectionLock;
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, BthPS3PSM_EvtDeviceAdd)
#pragma alloc_text (PAGE, BthPS3PSM_EvtDriverContextCleanup)
#endif


_Use_decl_annotations_
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    //
    // Initialize WPP Tracing
    //
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    EventRegisterNefarius_Bluetooth_PS_Filter_Service();

    FuncEntry(TRACE_DRIVER);

    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = BthPS3PSM_EvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config,
                           BthPS3PSM_EvtDeviceAdd
    );

    if (!NT_SUCCESS(status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &attributes,
        &config,
        WDF_NO_HANDLE
    )))
    {
        TraceError(
            TRACE_DRIVER,
            "WdfDriverCreate failed with status %!STATUS!",
            status
        );
        EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfDriverCreate", status);
        EventUnregisterNefarius_Bluetooth_PS_Filter_Service();
        WPP_CLEANUP(DriverObject);
        return status;
    }

#ifdef BTHPS3PSM_WITH_CONTROL_DEVICE

    if (!NT_SUCCESS(status = WdfCollectionCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        &FilterDeviceCollection
    )))
    {
        TraceError(
            TRACE_DRIVER,
            "WdfCollectionCreate failed with %!STATUS!",
            status
        );
        EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfCollectionCreate", status);
        EventUnregisterNefarius_Bluetooth_PS_Filter_Service();
        WPP_CLEANUP(DriverObject);
        return status;
    }

    //
    // The wait-lock object has the driver object as a default parent.
    //

    if (!NT_SUCCESS(status = WdfWaitLockCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        &FilterDeviceCollectionLock
    )))
    {
        TraceError(
            TRACE_DRIVER,
            "WdfWaitLockCreate failed with %!STATUS!",
            status
        );
        EventWriteFailedWithNTStatus(NULL, __FUNCTION__, L"WdfWaitLockCreate", status);
        EventUnregisterNefarius_Bluetooth_PS_Filter_Service();
        WPP_CLEANUP(DriverObject);
        return status;
    }

#endif

    EventWriteStartEvent(NULL, DriverObject, status);

    FuncExit(TRACE_DRIVER, "status=%!STATUS!", status);

    return status;
}

_Use_decl_annotations_
NTSTATUS
BthPS3PSM_EvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    FuncEntry(TRACE_DRIVER);

    NTSTATUS status = BthPS3PSM_CreateDevice(DeviceInit);

    FuncExit(TRACE_DRIVER, "status=%!STATUS!", status);

    return status;
}

_Use_decl_annotations_
VOID
BthPS3PSM_EvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    FuncEntry(TRACE_DRIVER);

    EventWriteUnloadEvent(NULL, DriverObject);

    EventUnregisterNefarius_Bluetooth_PS_Filter_Service();

    //
    // Stop WPP Tracing
    //
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}
