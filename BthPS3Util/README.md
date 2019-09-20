# BthPS3Util

Driver and service management utility

## About

This command line utility handles interacting with device drivers and Bluetooth stack settings relevant to this solution. It's the recommended way to change driver parameters instead of dealing with the registry directly. The tool is compatible with Windows 7 up to 10.

## Usage

Run `.\BthPS3Util.exe` without any arguments to get the help page. Every action of the tool requires administrative privileges, therefore its manifest requests elevated execution. Run within elevated shell to observe the output returned.

## 3rd party credits

This project uses the following 3rd party resources:

- [Argh! A minimalist argument handler](https://github.com/adishavit/argh)
- [Scoped coloring of Windows console output](https://github.com/jrebacz/colorwin)
- [Convenient high-level C++ wrappers around Windows Registry Win32 APIs](https://github.com/GiovanniDicanio/WinReg)
