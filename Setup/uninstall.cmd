@echo off
@setlocal

set MYDIR=%~dp0
pushd "%MYDIR%"

type NUL > uninstall.log

BthPS3Util.exe --disable-service >> uninstall.log 2>&1

BthPS3Util.exe --disable-filter >> uninstall.log 2>&1

BthPS3Util.exe --restart-host-device >> uninstall.log 2>&1

popd
endlocal
