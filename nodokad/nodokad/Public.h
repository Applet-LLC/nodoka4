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
// Define an Interface Guid so that app can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_nodokad,
    0xa70fe181,0xbfb6,0x4686,0xb6,0x38,0x86,0xdc,0xa1,0xba,0xea,0x6c);
// {a70fe181-bfb6-4686-b638-86dca1baea6c}
