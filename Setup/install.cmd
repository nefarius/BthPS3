@echo off
@setlocal

set MYDIR=%~dp0
pushd "%MYDIR%"

BthPS3Util.exe --enable-service

BthPS3Util.exe --install-driver --inf-path ".\BthPS3.inf" --force

BthPS3Util.exe --install-driver --inf-path ".\BthPS3PSM_Filter.inf" --force

BthPS3Util.exe --enable-filter

BthPS3Util.exe --restart-host-device

popd
endlocal
