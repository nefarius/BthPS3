@echo off
@setlocal

set MYDIR=%~dp0
pushd "%MYDIR%"

Inf2Cat.exe /driver:".\drivers\BthPS3_x64" /uselocaltime /os:7_X64,10_X64,Server10_X64,10_AU_X64,Server2016_X64,10_RS2_X64,ServerRS2_X64,10_RS3_X64,ServerRS3_X64,10_RS4_X64,ServerRS4_X64
Inf2Cat.exe /driver:".\drivers\BthPS3PSM_x64" /uselocaltime /os:7_X64,10_X64,Server10_X64,10_AU_X64,Server2016_X64,10_RS2_X64,ServerRS2_X64,10_RS3_X64,ServerRS3_X64,10_RS4_X64,ServerRS4_X64

Inf2Cat.exe /driver:".\drivers\BthPS3_x86" /uselocaltime /os:7_X86,10_X86,10_AU_X86,10_RS2_X86,10_RS3_X86,10_RS4_X86
Inf2Cat.exe /driver:".\drivers\BthPS3PSM_x86" /uselocaltime /os:7_X86,10_X86,10_AU_X86,10_RS2_X86,10_RS3_X86,10_RS4_X86

signtool sign /v /ac "C:\Program Files (x86)\Windows Kits\10\CrossCertificates\DigiCert_High_Assurance_EV_Root_CA.crt" /n "Nefarius Software Solutions e.U." /tr http://timestamp.digicert.com /fd sha256 /td sha256 .\drivers\BthPS3_x64\*.cat .\drivers\BthPS3PSM_x64\*.cat .\drivers\BthPS3_x86\*.cat .\drivers\BthPS3PSM_x86\*.cat

popd
endlocal
