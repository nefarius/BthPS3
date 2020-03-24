# v1.1.3 BthPS3 changelog

- Fixed incorrect use of executable non-pageable memory which can trigger a BSOD under Driver Verifier
- Fixed issue where disabling RAW mode wouldn't boot the PDO at all
- Fixed incorrect assignment of MTU configuration values
- Fixed a couple of incorrect trace flags
- Fixed waiting at DISPATCH_LEVEL, with a timeout different than zero
- Profile driver now refuses to load if attached to a host radio with unsupported firmware version
- Motion device support disabled by default (insert link)
- Wireless device support disabled by default (insert link)
