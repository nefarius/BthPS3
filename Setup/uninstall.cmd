@echo off
@setlocal

set MYDIR=%~dp0
pushd "%MYDIR%"

type NUL > "%TEMP%\uninstall.log"

BthPS3Util.exe --disable-service >> "%TEMP%\uninstall.log" 2>&1

BthPS3Util.exe --disable-filter >> "%TEMP%\uninstall.log" 2>&1

BthPS3Util.exe --restart-host-device >> "%TEMP%\uninstall.log" 2>&1

popd
endlocal
