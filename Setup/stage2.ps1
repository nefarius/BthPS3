Param(
    [Parameter(Mandatory=$true)]
    [string]$SetupVersion
) #end param

$signTool = "$(wdkwhere)\signtool.exe"
$timestampUrl = "http://timestamp.digicert.com"
$certName = "Nefarius Software Solutions e.U."

dotnet build -c:Release -p:SetupVersion=$SetupVersion ..\BthPS3Installer\BthPS3Installer.csproj

Invoke-Expression (
    "& `"$signTool`" sign /v /n `"$certName`" /a " +
    "/tr `"$timestampUrl`" /fd sha256 /td sha256 " +
    "`"..\BthPS3Installer\Nefarius_BthPS3_Drivers_x64_arm64_v$SetupVersion.msi`""
)
