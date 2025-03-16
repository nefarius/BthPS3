# BthPS3Util

Driver and service management utility

## About

This command line utility handles interacting with device drivers and Bluetooth stack settings relevant to this solution. It's the recommended way to change driver parameters instead of dealing with the registry directly. The tool is compatible with Windows 7 up to 10.

## Usage

Run `.\BthPS3Util.exe` without any arguments to get the help page. Every action of the tool requires administrative privileges, therefore its manifest requests elevated execution. Run within elevated shell to observe the output returned.

## Examples

### Enable local service

```
.\BthPS3Util.exe --enable-service
```

This enables the Bluetooth Enumerator (BthEnum.sys) to enumerate a device for `BTHPS3_SERVICE_GUID` service and create a PDO for the `BthPS3` profile driver to attach to. On a fresh installation this will cause a new unknown device to appear in the device tree.

### Install profile driver

```
.\BthPS3Util.exe --install-driver --inf-path "C:\Wherever\BthPS3\BthPS3.inf" --force
```

Invokes installation of the `BthPS3` profile/bus driver which will attach to the (previously) created PDO.

### Install filter driver

```
.\BthPS3Util.exe --install-driver --inf-path "C:\Wherever\BthPS3PSM\BthPS3PSM_Filter.inf" --force
```

Invokes installation of the `BthPS3PSM` lower filter driver service which will load on demand under the Bluetooth host radio USB device.

### Enable lower filter

```
.\BthPS3Util.exe --enable-filter
```

Enables the `BthPS3PSM` filter driver as lower filter for all USB device class devices. The filter driver will unload itself on non-filter-worthy devices automatically upon startup.

## 3rd party credits

This project uses the following 3rd party resources:

- [Argh! A minimalist argument handler](https://github.com/adishavit/argh)
- [Scoped coloring of Windows console output](https://github.com/jrebacz/colorwin)
- [Convenient high-level C++ wrappers around Windows Registry Win32 APIs](https://github.com/GiovanniDicanio/WinReg)
