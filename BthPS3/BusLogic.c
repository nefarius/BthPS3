#include "Driver.h"
#include "buslogic.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3_EvtWdfChildListCreateDevice)
#endif

_Use_decl_annotations_
NTSTATUS
BthPS3_EvtWdfChildListCreateDevice(
    WDFCHILDLIST ChildList,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER IdentificationDescription,
    PWDFDEVICE_INIT ChildInit
)
{
    NTSTATUS                            status = STATUS_UNSUCCESSFUL;
    PPDO_IDENTIFICATION_DESCRIPTION     pDesc;
    UNICODE_STRING                      guidString;
    WDFDEVICE                           hChild = NULL;

    DECLARE_UNICODE_STRING_SIZE(deviceId, MAX_DEVICE_ID_LEN);
    DECLARE_UNICODE_STRING_SIZE(hardwareId, MAX_DEVICE_ID_LEN);
    DECLARE_UNICODE_STRING_SIZE(instanceId, 12);

    UNREFERENCED_PARAMETER(ChildList);

    PAGED_CODE();


    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BUSLOGIC, "%!FUNC! Entry");

    pDesc = CONTAINING_RECORD(
        IdentificationDescription,
        PDO_IDENTIFICATION_DESCRIPTION,
        Header);

    //
    // PDO features
    // 
    WdfDeviceInitSetDeviceType(ChildInit, FILE_DEVICE_BUS_EXTENDER);
    WdfPdoInitAllowForwardingRequestToParent(ChildInit);

    //
    // Adjust properties depending on device type
    // 
    switch (pDesc->DeviceType)
    {
    case DS_DEVICE_TYPE_SIXAXIS:
        status = RtlStringFromGUID(&BTHPS3_BUSENUM_SIXAXIS,
            &guidString
        );
        break;
    case DS_DEVICE_TYPE_NAVIGATION:
        status = RtlStringFromGUID(&BTHPS3_BUSENUM_NAVIGATION,
            &guidString
        );
        break;
    case DS_DEVICE_TYPE_MOTION:
        status = RtlStringFromGUID(&BTHPS3_BUSENUM_MOTION,
            &guidString
        );
        break;
    case DS_DEVICE_TYPE_WIRELESS:
        status = RtlStringFromGUID(&BTHPS3_BUSENUM_WIRELESS,
            &guidString
        );
        break;
    default:
        // Doesn't happen
        return status;
    }

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_BUSLOGIC,
            "RtlStringFromGUID failed with status %!STATUS!",
            status
        );
        return status;
    }

#pragma region Build DeviceID

    status = RtlUnicodeStringPrintf(
        &deviceId,
        L"%ws\\%wZ",
        BthPS3BusEnumeratorName,
        guidString
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_BUSLOGIC,
            "RtlUnicodeStringPrintf failed for deviceId with status %!STATUS!",
            status
        );
        goto freeAndExit;
    }

    status = WdfPdoInitAssignDeviceID(ChildInit, &deviceId);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_BUSLOGIC,
            "WdfPdoInitAssignDeviceID failed with status %!STATUS!",
            status);
        goto freeAndExit;
    }

#pragma endregion

#pragma region Build HardwareID

    status = RtlUnicodeStringPrintf(
        &hardwareId,
        L"%ws\\%wZ",
        BthPS3BusEnumeratorName,
        guidString
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_BUSLOGIC,
            "RtlUnicodeStringPrintf failed for hardwareId with status %!STATUS!",
            status
        );
        goto freeAndExit;
    }

    status = WdfPdoInitAddHardwareID(ChildInit, &deviceId);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_BUSLOGIC,
            "WdfPdoInitAddHardwareID failed with status %!STATUS!",
            status);
        goto freeAndExit;
    }

#pragma endregion

#pragma region Build InstanceID

    status = RtlUnicodeStringPrintf(
        &instanceId,
        L"%I64X",
        pDesc->RemoteAddress
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_BUSLOGIC,
            "RtlUnicodeStringPrintf failed for instanceId with status %!STATUS!",
            status
        );
        goto freeAndExit;
    }

    status = WdfPdoInitAssignInstanceID(ChildInit, &instanceId);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_BUSLOGIC,
            "WdfPdoInitAssignInstanceID failed with status %!STATUS!",
            status);
        goto freeAndExit;
    }

#pragma endregion

#pragma region Child device creation

    status = WdfDeviceCreate(
        &ChildInit, 
        WDF_NO_OBJECT_ATTRIBUTES, 
        &hChild
    );
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_BUSLOGIC,
            "WdfDeviceCreate failed with status %!STATUS!",
            status);
        goto freeAndExit;
    }

#pragma endregion

    freeAndExit:

               RtlFreeUnicodeString(&guidString);

               TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BUSLOGIC, "%!FUNC! Exit");

               return status;
}
