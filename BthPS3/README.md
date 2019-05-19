# BthPS3

PlayStation(R) 3 Peripherals Bluetooth Profile and Bus Driver

## About

WIP

## Danger Zone

The driver service supports the following registry values in `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\BthPS3\Parameters`. Don't create, remove or alter those if you have no clue of what you're doing 🤞

- `RawPDO` (DWORD) - Bring up bus children in Raw PDO mode allowing them to power up without a function driver. Exposes the PDO to user-land.
- `HidePDO` (DWORD) - Hides the bus children from Device Manager. This is a cosmetical fix so no Unknown Device shows up when in Raw Mode.
