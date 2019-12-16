# BthPS3

Windows kernel-mode Bluetooth Profile & Filter Drivers for PS3 peripherals.

[![Discord](https://img.shields.io/discord/346756263763378176.svg)](https://discord.vigem.org) [![Website](https://img.shields.io/website-up-down-green-red/https/vigem.org.svg?label=ViGEm.org)](https://vigem.org/) [![PayPal Donate](https://img.shields.io/badge/paypal-donate-blue.svg)](<https://paypal.me/NefariusMaximus>) [![Support on Patreon](https://img.shields.io/badge/patreon-donate-orange.svg)](<https://www.patreon.com/nefarius>) [![GitHub followers](https://img.shields.io/github/followers/nefarius.svg?style=social&label=Follow)](https://github.com/nefarius) [![Twitter Follow](https://img.shields.io/twitter/follow/nefariusmaximus.svg?style=social&label=Follow)](https://twitter.com/nefariusmaximus)

## About

**TL;DR:** these drivers allow popular PlayStation(R) 3 gaming peripherals (SIXAXIS/DualShock 3, PS Move Navigation & Motion Controllers) to connect to Windows via Bluetooth without losing any standard functionality. 😊

This set of Windows kernel-mode drivers enhances the standard (a.k.a. vanilla) Bluetooth stack (Microsoft/Broadcom/Toshiba/Intel/...) with an additional L2CAP server service (profile driver) and a USB lower filter driver [gracefully working around the reserved PSMs issue](https://nadavrub.wordpress.com/2015/07/17/simulate-hid-device-with-windows-desktop/) causing the PS3 peripherals connections to get denied on the default Windows stack. The profile driver attempts to distinguish the incoming device types based on their reported remote names and exposes their HID Control and HID Interrupt channels via simple bus child devices (a.k.a PDOs). The profile/bus driver supports both "regular" operation modes (requiring a proper function driver like a HID-minidriver) and "raw" mode (powering the PDO up without a function driver and exposing it to user-land) for maximum flexibility and future-proofing. The PSM filter only attaches to Bluetooth class devices and unloads itself if the underlying enumerator isn't USB.

The solution consists of the following individual projects:

- [`BthPS3`](/BthPS3) - Multi-purpose kernel-mode driver. Function driver for service PDO exposed by `BTHENUM` (Microsoft), Bluetooth profile (L2CAP server service) and bus driver for PS3 wireless peripherals.
- [`BthPS3PSM`](/BthPS3PSM) - Lower filter driver for `BTHUSB`, patching L2CAP packets. Required for profile driver to receive L2CAP traffic.
- [`BthPS3Util`](/BthPS3Util) - User-land command-line utility for managing driver installation tasks and configuration changes.

## How to build

### Prerequisites

- [Step 1: Install Visual Studio 2019](<https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk#download-icon-step-1-install-visual-studio-2019>)
  - From the Visual Studio Installer, add the Individual component `MSVC v142 - VS 2019 C++ x64/x86 Spectre-mitigated libs`
- [Step 2: Install WDK for Windows 10, version 1903](<https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk#download-icon-step-2-install-wdk-for-windows-10-version-1903>)
- Install [.NET Core 3.0](<https://dotnet.microsoft.com/download/dotnet-core/3.0>) (optional, required for command-line build)

You can build individual projects of the solution within Visual Studio or run the `.\build.ps1` script in PowerShell.

## Device tree

Below representation attempts to visualize the relationships between the drivers and devices involved (near host hardware on the bottom, towards exposed child devices on top):

```text
     +----------------------+                                +----------------------+
     |    Navigation PDO    +<----------+         +--------->+      Motion PDO      |
     +----------------------+           |         |          +----------------------+
                                        |         |
                                        |         |
                                        |         |
                                        |         |
                                        |         |
+----------------------+          +-----+---------+------+         +----------------------+
|      SIXAXIS PDO     +<---------+ Profile & Bus Driver +-------->+     Wireless PDO     |
+----------------------+          |     (BthPS3.sys)     |         +----------------------+
                                  +----------+-----------+
                                             ^
                                             |
                                             v
                                  +----------+-----------+
                                  | Bluetooth Enumerator |
                                  |    (bthenum.sys)     |
                                  +----------+-----------+
                                             ^
                                             |
                                             v
                                  +----------+-----------+
                                  |     bthport.sys      |
                                  +----------+-----------+
                                             ^
                                             |
                                             v
                                  +----------+-----------+
                                  |      bthusb.sys      |
                                  +----------+-----------+
                                             ^
                                             |
                                             v
                                  +----------+-----------+
                                  | BthPS3PSM.sys filter |
                                  +----------+-----------+
                                             ^
                                             |
                                             v
                                  +----------+-----------+
                                  |       USB Stack      |
                                  +----------+-----------+
                                             ^
                                             |
                                             v
                                  +----------+-----------+
                                  | USB Bluetooth dongle |
                                  +----------------------+

```

## Sources & 3rd party credits

- [ViGEm Forums - Bluetooth Filter Driver for DS3-compatibility - research notes](https://forums.vigem.org/topic/242/bluetooth-filter-driver-for-ds3-compatibility-research-notes)
- [Arduino - felis/USB_Host_Shield_2.0 - PS3 Information](https://github.com/felis/USB_Host_Shield_2.0/wiki/PS3-Information#Bluetooth)
- [Emulate HID Device with Windows Desktop](https://nadavrub.wordpress.com/2015/07/17/simulate-hid-device-with-windows-desktop/)
- [microsoft/Windows-driver-samples - Bluetooth Echo L2CAP Profile Driver](https://github.com/Microsoft/Windows-driver-samples/tree/master/bluetooth/bthecho)
- [Microsoft Bluetooth DDI - Reserved PSMs](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/bthddi/ns-bthddi-_brb_psm#members)
- [Eleccelerator Wiki - DualShock 3](http://eleccelerator.com/wiki/index.php?title=DualShock_3)
