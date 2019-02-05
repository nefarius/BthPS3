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

DEFINE_GUID (GUID_DEVINTERFACE_BlyatStorm,
    0xdb3c850f,0xa482,0x4ac0,0x9f,0x5c,0x01,0x12,0x71,0x87,0x69,0xb4);
// {db3c850f-a482-4ac0-9f5c-0112718769b4}
