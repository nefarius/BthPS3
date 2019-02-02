#include "Driver.h"

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
    UNREFERENCED_PARAMETER(ChildList);
    UNREFERENCED_PARAMETER(IdentificationDescription);
    UNREFERENCED_PARAMETER(ChildInit);

    PAGED_CODE();

    return STATUS_SUCCESS;
}
