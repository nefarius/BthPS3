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
    PPDO_IDENTIFICATION_DESCRIPTION pDesc;

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

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BUSLOGIC, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}
