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

DEFINE_GUID (GUID_DEVINTERFACE_BthPS3,
    0x1cb831ea,0x79cd,0x4508,0xb0,0xfc,0x85,0xf7,0xc8,0x5a,0xe8,0xe0);
// {1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}
