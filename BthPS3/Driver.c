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
#include "Driver.tmh"
#include "BthPS3ETW.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, BthPS3EvtDeviceAdd)
#pragma alloc_text (PAGE, BthPS3EvtDriverContextCleanup)
#endif

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT  DriverObject,
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

	EventRegisterNefarius_BthPS3_Profile_Driver();

	FuncEntry(TRACE_DRIVER);

	ExInitializeDriverRuntime(DrvRtPoolNxOptIn);

	//
	// Register a cleanup callback so that we can call WPP_CLEANUP when
	// the framework driver object is deleted during driver unload.
	//
	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.EvtCleanupCallback = BthPS3EvtDriverContextCleanup;

	WDF_DRIVER_CONFIG_INIT(&config,
		BthPS3EvtDeviceAdd
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
			"WdfDriverCreate failed %!STATUS!",
			status
		);
		WPP_CLEANUP(DriverObject);
		return status;
	}

	EventWriteStartEvent(NULL, DriverObject, status);

	FuncExit(TRACE_DRIVER, "status=%!STATUS!", status);

	return status;
}

NTSTATUS
BthPS3EvtDeviceAdd(
	_In_    WDFDRIVER       Driver,
	_Inout_ PWDFDEVICE_INIT DeviceInit
)
{
	NTSTATUS status;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	FuncEntry(TRACE_DRIVER);

	status = BthPS3_CreateDevice(DeviceInit);

	FuncExit(TRACE_DRIVER, "status=%!STATUS!", status);

	return status;
}

VOID
BthPS3EvtDriverContextCleanup(
	_In_ WDFOBJECT DriverObject
)
{
	PAGED_CODE();

	FuncEntry(TRACE_DRIVER);

	EventWriteUnloadEvent(NULL, DriverObject);

	EventUnregisterNefarius_BthPS3_Profile_Driver();

	//
	// Stop WPP Tracing
	//
	WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}

//
// This satisfies the linker when looking for the import we patched out of "wpprecorder.lib"
//
VOID
imp_WppRecorderReplay(
	_In_ PVOID       WppCb,
	_In_ TRACEHANDLE WppTraceHandle,
	_In_ ULONG       EnableFlags,
	_In_ UCHAR       EnableLevel
)
{
	UNREFERENCED_PARAMETER(WppCb);
	UNREFERENCED_PARAMETER(WppTraceHandle);
	UNREFERENCED_PARAMETER(EnableFlags);
	UNREFERENCED_PARAMETER(EnableLevel);
}
