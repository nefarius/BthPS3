# BthPS3PSM

Protocol/Service Multiplexer proxy filter driver for `BTHUSB`.

## About

The `BthPS3PSM.sys` kernel-mode filter driver patches the [PSM](https://stackoverflow.com/a/55386042) values in incoming L2CAP (USB bulk pipe) packets before they reach the `bthusb.sys` function driver.

The driver is meant to be loaded as a lower filter for `GUID_DEVCLASS_BLUETOOTH` class devices that run under the `USB` enumerator. This effectively targets a wide range of inherently compatible Bluetooth host radio devices attached via USB (common nano dongles, integrated cards, etc.), eliminating the necessity of crafting a custom `*.inf` that includes explicit hardware IDs.

Although it is currently not supported by Windows to run multiple Bluetooth host radios at the same time, the driver has been designed with multiple radios in mind and can handle this case should it ever be pushed to production by Microsoft. The driver aborts initialization if it is attached to a device stack that does not run under the supported `USB` enumerator.

### Why

PS3 peripherals lack a standard-compliant [SDP](https://www.bluetooth.com/specifications/assigned-numbers/service-discovery/) record and try to directly establish the two L2CAP channels HID Control (PSM `0x11`) and HID Interrupt/Data (PSM `0x13`) with the Bluetooth host radio, a procedure [that is not allowed in the Windows Bluetooth DDIs](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/bthddi/ns-bthddi-_brb_psm#remarks). The Microsoft-provided `bthport.sys` driver holds authority over [all system PSMs](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/bthddi/ns-bthddi-_brb_psm#members) and—in vanilla operation—immediately denies connection attempts for those with status `ConnectionRefusedPsmNotSupported = 0x0002`. One way around this limitation is [patching the internal function involved in matching against these PSM values](https://nadavrub.wordpress.com/2015/07/17/simulate-hid-device-with-windows-desktop/), although it is strongly recommended to avoid this approach to not put kernel integrity and stability at risk.

### How

To accomplish its goal, the filter driver intercepts `IRP_MJ_INTERNAL_DEVICE_CONTROL` requests traveling down from `BTHUSB.SYS` towards the USB subsystem, looks for the `IOCTL_INTERNAL_USB_SUBMIT_URB` I/O control code, and attaches a completion routine when the `URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER` function is requested, matching the bulk-in endpoint (where L2CAP traffic is expected). The completion routine identifies requests of type `L2CAP_Connection_Request`, checks the buffer for values of `PSM_HID_CONTROL` or `PSM_HID_INTERRUPT`, and overwrites them with the values that the `BthPS3.sys` profile driver listens on. This modification happens before `bthport.sys!BthIsSystemPSM` is called, thereby avoiding the need to modify or hook this function.

### Pitfalls

This method can cause unintended side effects for other devices attempting to directly connect via the “forbidden” PSMs. Therefore, the driver exposes a simple API that allows the profile driver (and elevated userland processes) to temporarily disable its patching capabilities, effectively restoring standard-compliant operation of the entire Bluetooth stack without needing to unload the filter or power-cycle the host radio.
