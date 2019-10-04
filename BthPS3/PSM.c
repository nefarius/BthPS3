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
