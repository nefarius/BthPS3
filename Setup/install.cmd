@echo off
@setlocal

set MYDIR=%~dp0
pushd "%MYDIR%"

type NUL > install.log

rem Install NULL device driver
BthPS3Util.exe --install-driver --inf-path ".\BthPS3_PDO_NULL_Device.inf" --force >> install.log 2>&1

rem Enable profile service (L2CAP server)
BthPS3Util.exe --enable-service >> install.log 2>&1

rem Create "BthPS3PSM" driver service
BthPS3Util.exe --create-filter-service --bin-path ".\BthPS3PSM.sys" >> install.log 2>&1

rem Enable lower filter for BTHUSB
BthPS3Util.exe --enable-filter >> install.log 2>&1

rem Restart host device (causes BthPS3PSM to load)
BthPS3Util.exe --restart-host-device >> install.log 2>&1

rem Install profile/bus driver
BthPS3Util.exe --install-driver --inf-path ".\BthPS3.inf" --force >> install.log 2>&1

rem Instruct filter to enable patch
BthPS3Util.exe --enable-psm-patch --device-index 0 >> install.log 2>&1

rem Disable DS4 "PS4 mode" support until fully implemented
rem see https://forums.vigem.org/post/1677
reg add HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\BthPS3\Parameters /v IsWIRELESSSupported /t REG_DWORD /d 0 /f >> install.log 2>&1

rem Disable PSMove MOTION support to ensure compatibility with PSMoveService
rem see https://forums.vigem.org/topic/384/bthps3-incompatible-with-psmoveservice/
reg add HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\BthPS3\Parameters /v IsMOTIONSupported /t REG_DWORD /d 0 /f >> install.log 2>&1

popd
endlocal
