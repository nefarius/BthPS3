# BthPS3

Windows kernel-mode Bluetooth Profile & Filter Drivers for PS3 peripherals.

[![Discord](https://img.shields.io/discord/346756263763378176.svg)](https://discord.vigem.org) [![Website](https://img.shields.io/website-up-down-green-red/https/vigem.org.svg?label=ViGEm.org)](https://vigem.org/) [![PayPal Donate](https://img.shields.io/badge/paypal-donate-blue.svg)](<https://paypal.me/NefariusMaximus>) [![Support on Patreon](https://img.shields.io/badge/patreon-donate-orange.svg)](<https://www.patreon.com/nefarius>) [![GitHub followers](https://img.shields.io/github/followers/nefarius.svg?style=social&label=Follow)](https://github.com/nefarius) [![Twitter Follow](https://img.shields.io/twitter/follow/nefariusmaximus.svg?style=social&label=Follow)](https://twitter.com/nefariusmaximus)

## About

TBD

The solution consists of the following individual projects:

- [`BthPS3`](/BthPS3) - Multi-purpose kernel-mode driver. Function driver for `BTHENUM` PDO, Bluetooth profile (L2CAP server service) and bus driver for PS3 wireless peripherals.
- [`BthPS3PSM`](/BthPS3PSM) - Lower filter driver for `BTHUSB`, patching L2CAP packets. Required for profile driver to receive L2CAP traffic.
- [`BthPS3Util`](/BthPS3Util) - User-land command-line utility for managing driver installation tasks and configuration changes.

## How to build

### Prerequisites

- [Step 1: Install Visual Studio 2019](<https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk#download-icon-step-1-install-visual-studio-2019>)
- [Step 2: Install WDK for Windows 10, version 1903](<https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk#download-icon-step-2-install-wdk-for-windows-10-version-1903>)
- From the Visual Studio Installer, add the Individual component `MSVC v142 - VS 2019 C++ x64/x86 Spectre-mitigated libs`
- Install [.NET Core 2.1](<https://dotnet.microsoft.com/download/dotnet-core/2.1>) (optional, required for command-line build) 

You can build individual projects of the solution within Visual Studio or run the `.\build.ps1` script in PowerShell.

## Device tree

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