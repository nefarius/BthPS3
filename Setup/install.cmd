@echo off
@setlocal

set MYDIR=%~dp0
pushd "%MYDIR%"

type NUL > install.log

BthPS3Util.exe --enable-service >> install.log 2>&1

BthPS3Util.exe --install-driver --inf-path ".\BthPS3.inf" --force >> install.log 2>&1

BthPS3Util.exe --install-driver --inf-path ".\BthPS3PSM_Filter.inf" --force >> install.log 2>&1

BthPS3Util.exe --enable-filter >> install.log 2>&1

BthPS3Util.exe --restart-host-device >> install.log 2>&1

BthPS3Util.exe --enable-psm-patch --device-index 0 >> install.log 2>&1

popd
endlocal
