$signTool = "$(wdkwhere)\signtool.exe"
$timestampUrl = "http://timestamp.digicert.com"
$certName = "Nefarius Software Solutions e.U."

# List of files to sign
$files =    "`"..\bin\ARM64\*.msi`" " + 
            "`"..\bin\x64\*.msi`" "

# sign with only one certificate
Invoke-Expression "& `"$signTool`" sign /v /n `"$certName`" /tr $timestampUrl /fd sha256 /td sha256 $files"
