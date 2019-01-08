/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that apps can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_BthPS3PSM,
    0x1e1f8b68,0xeaa2,0x4d19,0x8b,0x02,0xe8,0xb0,0x91,0x6c,0x77,0xdb);
// {1e1f8b68-eaa2-4d19-8b02-e8b0916c77db}
