@echo off
@setlocal

rem x64
verpatch ..\bin\x64\BthPS3\BthPS3.sys "1.0.0.0" /va /s product "BthPS3" /s description "PlayStation(R) 3 Peripherals Bluetooth Profile and Bus Driver" /s company "Nefarius Software Solutions e.U." /s copyright "(c) 2010 Nefarius Software Solutions e.U."
verpatch ..\bin\x64\BthPS3PSM\BthPS3PSM.sys "1.0.0.0" /va /s product "BthPS3PSM" /s description "Protocol/Service Multiplexer Proxy Filter Driver" /s company "Nefarius Software Solutions e.U." /s copyright "(c) 2010 Nefarius Software Solutions e.U."

rem x86
verpatch ..\bin\x86\BthPS3\BthPS3.sys "1.0.0.0" /va /s product "BthPS3" /s description "PlayStation(R) 3 Peripherals Bluetooth Profile and Bus Driver" /s company "Nefarius Software Solutions e.U." /s copyright "(c) 2010 Nefarius Software Solutions e.U."
verpatch ..\bin\x86\BthPS3PSM\BthPS3PSM.sys "1.0.0.0" /va /s product "BthPS3PSM" /s description "Protocol/Service Multiplexer Proxy Filter Driver" /s company "Nefarius Software Solutions e.U." /s copyright "(c) 2010 Nefarius Software Solutions e.U."

endlocal
