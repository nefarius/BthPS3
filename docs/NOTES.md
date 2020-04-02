# v1.1.4 (setup-v1.2.4) BthPS3 changelog

- Restored support for Bluetooth 2.0 + EDR host radios

## Portal details

Job names:

- BthPS3 x86 v1.1.4.0 02.04.2020
- BthPS3 x64 v1.1.4.0 02.04.2020

# v1.1.3 (setup-v1.2.3) BthPS3 changelog

- Fixed incorrect use of executable non-pageable memory which can trigger a bug check when Code Integrity is enforced
- Fixed issue where disabling RAW mode wouldn't boot the PDO at all
- Fixed incorrect assignment of MTU configuration values
- Fixed a couple of incorrect trace flags
- Fixed waiting at DISPATCH_LEVEL with a timeout different than zero, causing a bug check
- Profile driver now refuses to load if attached to a host radio with unsupported firmware version
- Motion device support disabled by default. This option needs to be turned off to not break compatibility with the [PSMoveService](https://github.com/psmoveservice/PSMoveService) project
- Wireless device support disabled by default. This option needs to be turned off to not break compatibility with the [DS4Windows](https://github.com/Ryochan7/DS4Windows) project

## Portal details

Job names:

- BthPS3 x86 v1.1.3.0 31.03.2020
- BthPS3 x64 v1.1.3.0 31.03.2020
