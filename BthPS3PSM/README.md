# BthPS3PSM

Protocol/Service Multiplexer Proxy Filter Driver for `BTHUSB`

## About

The `BthPS3PSM.sys` kernel-mode filter driver performs patching the [PSM](https://stackoverflow.com/a/55386042) values in the incoming L2CAP (USB bulk pipe) packets before they reach the `bthusb.sys` function driver.

The driver is meant to be loaded as a lower filter for `GUID_DEVCLASS_BLUETOOTH` class devices which run under the `USB` enumerator. This effectively targets a wide rage of inherently compatible Bluetooth host radio devices attached via USB (common nano dongles, integrated cards, ...) eliminating the necessity of crafting a custom `*.inf` including explicit hardware IDs.

Although it's currently not supported by Windows to run multiple Bluetooth host radios at the same time, the driver has been designed with multiple radios in mind and can handle this case, should it ever be pushed to production by Microsoft. The driver aborts initialization if it's attached to a device stack not running under the supported `USB` enumerator.

The PS3 peripherals lack a standard-compliant [SDP](https://www.bluetooth.com/specifications/assigned-numbers/service-discovery/) record and try to directly establish the two L2CAP channels HID Control (PSM `0x11`) and HID Interrupt/Data (PSM `0x13`) with the Bluetooth host radio, a procedure [which is not allowed in the Windows Bluetooth DDIs](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/bthddi/ns-bthddi-_brb_psm#remarks). The Microsoft provided `bthport.sys` driver holds authority over [all system PSMs](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/bthddi/ns-bthddi-_brb_psm#members) and - in vanilla operation - immediately denies connection attempts for those with status `ConnectionRefusedPsmNotSupported = 0x0002`. A way around this limitation is [patching the internal function involved in matching against these PSM values](https://nadavrub.wordpress.com/2015/07/17/simulate-hid-device-with-windows-desktop/), although it's highly recommended to avoid this approach to not put kernel integrity and stability at risk.

To be continued...
