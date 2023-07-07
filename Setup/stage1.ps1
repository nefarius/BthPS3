$signTool = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\signtool.exe"
$timestampUrl = "http://timestamp.digicert.com"
$certName = "Nefarius Software Solutions e.U."

$files =    "`".\drivers\BthPS3_ARM64\*.sys`" " +
            "`".\drivers\BthPS3_x64\*.sys`" " +
			"`".\drivers\BthPS3PSM_ARM64\*.sys`" " +
            "`".\drivers\BthPS3PSM_x64\*.sys`" "

# sign with adding to existing certificates
Invoke-Expression "& `"$signTool`" sign /v /as /n `"$certName`" /tr $timestampUrl /fd sha256 /td sha256 $files"
