@echo off
@setlocal

set MYDIR=%~dp0
pushd "%MYDIR%"

BthPS3Util.exe --disable-service

BthPS3Util.exe --disable-filter

BthPS3Util.exe --restart-host-device

popd
endlocal
