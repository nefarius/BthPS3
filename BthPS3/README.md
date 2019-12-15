# BthPS3

PlayStation(R) 3 Peripherals Bluetooth Profile and Bus Driver

## About

The `BthPS3.sys` kernel-mode driver plays multiple roles, depending on which angle its viewed on within the device stack.

Once the `bthenum.sys` bus exposes the service PDO with hardware ID `BTHENUM\{1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}`, the driver attaches as the function driver for this PDO. Upon power-up it registers an L2CAP server listening for the "artificial" PSMs `0x5053` for HID Control and `0x5055` for HID Interrupt channels effectively bypassing the reserved PSMs `0x11` and `0x13` gracefully without disturbing regular standard-compliant operation. Once a remote device connection is coming in, the reported remote name of the device is queried from the Bluetooth radio and used to distinguish the different supported device types (SIXAXIS/DS3, Navigation, Motion or DS4 device). When successfully identified, the driver serves as a bus driver exposing the newly arrived device as its own independent PDO which in turn exposes the established control and interrupt channels to either another function driver or user-land, depending on certain registry settings.
