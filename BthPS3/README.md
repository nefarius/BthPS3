# BthPS3

PlayStation(R) 3 Peripherals Bluetooth Profile and Bus Driver

## About

The `BthPS3.sys` kernel-mode driver plays multiple roles, depending on which angle it's viewed on within the device stack.

Once the `bthenum.sys` bus exposes the service PDO with hardware ID `BTHENUM\{1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}`, the driver attaches as the function driver for this PDO. Upon power-up it registers an L2CAP server listening for the "artificial" PSMs `0x5053` for HID Control and `0x5055` for HID Interrupt channels effectively bypassing the reserved PSMs `0x11` and `0x13` gracefully without disturbing regular standard-compliant operation. Once a remote device connection is coming in, the reported remote name of the device is queried from the Bluetooth radio and used to distinguish the different supported device types (SIXAXIS/DS3, Navigation, Motion or DS4 device). When successfully identified, the driver serves as a bus driver exposing the newly arrived device as its own independent PDO which in turn exposes the established control and interrupt channels to either another function driver or user-land, depending on certain registry settings. If no function driver (like a HID-minidriver) for the child PDOs is present on the system, the [`BthPS3_PDO_NULL_Device.inf`](./BthPS3_PDO_NULL_Device.inf) "NULL" driver can be installed to properly name the new "driverless" devices. This is a cosmetic fix only and has no impact on any user-land operation other than dropping the first connection attempt of the remote device because it gets power-cycled (and therefore disconnected) once.

Since child device exposure isn't protocol-agnostic and only requires four simple I/O control codes (HID control read/write & HID interrupt read/write) designing a function driver or simple user-land process communicating with the wireless devices can be achieved with little to no knowledge about the whole Bluetooth connection procedure at all. Think of this driver as providing the pipelines from and to the wireless controller devices, the content traveling through those pipes is completely transparent and of no interest to the profile/bus driver, similar to USB bulk or interrupt endpoints.

The driver handles the whole L2CAP channel (dis-)connection state machine, reacts to (surprise-)removal events of the host radio and drops connections automatically on reaching defined I/O idle timeouts to avoid leaking memory (data pending in every channel has to be consumed or it will keep allocating non-paged memory) and conserve remote device battery usage.

Additionally the driver will open the `\\DosDevices\\BthPS3PSMControl` remote I/O target and instruct the filter to enable or disable its L2CAP patching capabilities if necessary.

## Supported remote devices

The following set of models/revisions of PS3 peripherals (and more) are currently supported by the driver:

### SIXAXIS/DualShock 3

### PS Move Navigation Controller

### PS Move Motion Controller

### Wireless Controller/DualShock 4
