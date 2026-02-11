# BthPS3 Config UI

Graphical configuration utility for the BthPS3 driver.

## About

`BthPS3CfgUI` is a .NET WPF application that lets you view and change BthPS3 driver registry settings in a safe, controlled way.  
It is intended for **end users** and **power users** who want to adjust driver behavior without manually editing the registry.

Typical use cases include:

- **Tuning timeouts and power behavior** for wireless controllers.
- **Enabling or disabling features** exposed by the BthPS3 driver.
- **Resetting configuration** back to sane defaults when experimentation went wrong.

All changes are validated before being written and only affect the BthPS3-related configuration keys.

## Requirements

- Windows 10 or 11, x64 or ARM64 (same as the BthPS3 driver).  
- BthPS3 driver installed and working.  
- .NET desktop runtime components as provided by the main BthPS3 installer.

## Getting it

The recommended way to obtain `BthPS3CfgUI` is via the **BthPS3 installer** from the  
[latest release page](https://github.com/nefarius/BthPS3/releases/latest).  
Standalone binaries, if published, will be listed alongside the installer on that page.

## Usage

1. **Start the tool**
   - Launch `BthPS3 Config UI` from the Start menu or from the folder where it was installed.
   - Run it with administrative rights if you are prompted or if settings fail to apply.
2. **Review current settings**
   - Navigate through the tabs/pages to see the currently active BthPS3 configuration.
   - Hover over labels or info icons to read short explanations of each option when available.
3. **Change options**
   - Adjust only the settings you understand; some values can affect stability or connectivity.  
   - Invalid or out-of-range values are rejected or highlighted.
4. **Apply and restart**
   - Click **Apply** or **Save** (depending on the UI version) to commit changes.  
   - You may need to **reconnect the controller** or **restart Windows** for certain options to take effect.
5. **Revert to defaults (if available)**
   - Use the *Reset* / *Defaults* controls when present to quickly return to a known‑good configuration.

For detailed descriptions of individual options, refer to the  
[BthPS3 documentation](https://docs.nefarius.at/projects/BthPS3/).

## Screenshots

![BthPS3CfgUI_NdHsCLmLvT.png](../docs/BthPS3CfgUI_NdHsCLmLvT.png)

![BthPS3CfgUI_omClUJooX8.png](../docs/BthPS3CfgUI_omClUJooX8.png)

![BthPS3CfgUI_qay8Nn6lCm.png](../docs/BthPS3CfgUI_qay8Nn6lCm.png)
