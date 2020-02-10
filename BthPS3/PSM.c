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
#include "psm.tmh"


//
// Request filter driver to disable PSM patching (PASSIVE_LEVEL only)
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3PSM_DisablePatchSync(
	WDFIOTARGET IoTarget,
	ULONG DeviceIndex
)
{
	WDF_MEMORY_DESCRIPTOR MemoryDescriptor;
	BTHPS3PSM_DISABLE_PSM_PATCHING payload;

	payload.DeviceIndex = DeviceIndex;

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
		&MemoryDescriptor,
		(PVOID)& payload,
		sizeof(payload)
	);

	return WdfIoTargetSendIoctlSynchronously(
		IoTarget,
		NULL,
		IOCTL_BTHPS3PSM_DISABLE_PSM_PATCHING,
		&MemoryDescriptor,
		NULL,
		NULL,
		NULL
	);
}

//
// Request filter driver to enable PSM patching (PASSIVE_LEVEL only)
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3PSM_EnablePatchSync(
	WDFIOTARGET IoTarget,
	ULONG DeviceIndex
)
{
	WDF_MEMORY_DESCRIPTOR MemoryDescriptor;
	BTHPS3PSM_ENABLE_PSM_PATCHING payload;

	payload.DeviceIndex = DeviceIndex;

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
		&MemoryDescriptor,
		(PVOID)& payload,
		sizeof(payload)
	);

	return WdfIoTargetSendIoctlSynchronously(
		IoTarget,
		NULL,
		IOCTL_BTHPS3PSM_ENABLE_PSM_PATCHING,
		&MemoryDescriptor,
		NULL,
		NULL,
		NULL
	);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3PSM_EnablePatchAsync(
	WDFIOTARGET IoTarget,
	ULONG DeviceIndex
)
{
	NTSTATUS                        status = STATUS_UNSUCCESSFUL;
    WDFREQUEST                      request;
	WDF_OBJECT_ATTRIBUTES           attribs;
    PBTHPS3PSM_ENABLE_PSM_PATCHING  pPayload;
    WDFMEMORY                       memory;

	UNREFERENCED_PARAMETER(IoTarget);
	UNREFERENCED_PARAMETER(DeviceIndex);

    WDF_OBJECT_ATTRIBUTES_INIT(&attribs);

    //
    // Create new request (this is fired infrequently so no reuse)
    // 
    status = WdfRequestCreate(&attribs,
        IoTarget,
        &request);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Associate payload with request
    // 
    WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
    attribs.ParentObject = request;

    //
    // Create payload memory
    // 
    status = WdfMemoryCreate(&attribs,
        NonPagedPool,
        POOLTAG_BTHPS3,
        sizeof(BTHPS3PSM_ENABLE_PSM_PATCHING),
        &memory,
        (PVOID)&pPayload);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    pPayload->DeviceIndex = DeviceIndex;

    //
    // Format async request
    // 
    status = WdfIoTargetFormatRequestForIoctl(
        IoTarget,
        request,
        IOCTL_BTHPS3PSM_ENABLE_PSM_PATCHING,
        memory,
        NULL,
        NULL,
        NULL
    );
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Assign a fixed completion routine
    // 
    WdfRequestSetCompletionRoutine(
        request,
        BthPS3PSM_FilterRequestCompletionRoutine,
        NULL
    );

    //
    // Send it
    // 
    if (WdfRequestSend(request,
        IoTarget,
        NULL) == FALSE)
    {
        return WdfRequestGetStatus(request);
    }

    return status;
}

//
// Async filter enable request has completed
// 
void BthPS3PSM_FilterRequestCompletionRoutine(
    WDFREQUEST Request,
    WDFIOTARGET Target,
    PWDF_REQUEST_COMPLETION_PARAMS Params,
    WDFCONTEXT Context
)
{
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);

    TraceEvents(TRACE_LEVEL_ERROR, 
        TRACE_PSM,
        "PSM Filter enable request finished with status %!STATUS!",
        WdfRequestGetStatus(Request)
    );

    WdfObjectDelete(Request);
}
