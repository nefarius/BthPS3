#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$script = Join-Path $here 'New-PartnerSubmissionInf.ps1'
$temp = Join-Path ([IO.Path]::GetTempPath()) ('bthps3-inf-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp | Out-Null

function Assert-True([bool] $Condition, [string] $Name) {
    if (-not $Condition) {
        throw "FAIL $Name"
    }
    Write-Output "PASS $Name"
}

function Assert-Throws([scriptblock] $Action, [string] $Name) {
    $threw = $false
    try { & $Action } catch { $threw = $true }
    if (-not $threw) { throw "FAIL ${Name}: expected an exception" }
    Write-Output "PASS $Name"
}

try {
    $profile = @'
[Version]
Signature="$WINDOWS NT$"
DriverVer=09/14/2026,2.12.0.2012

[SourceDisksNames]
1 = %DiskName%,,,""

[SourceDisksFiles]
BthPS3.sys  = 1,,

[Manufacturer]
%ManufacturerName%=BthPS3,NTAMD64

[BthPS3.NTAMD64]
%BthPS3.DeviceDesc%=BthPS3_Device, BTHENUM\{1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}

[BthPS3_Device.NT]
CopyFiles=Drivers_Dir

[Drivers_Dir]
BthPS3.sys

[Strings]
ManufacturerName="Nefarius Software Solutions e.U."
DiskName = "BthPS3 Installation Disk"
BthPS3.DeviceDesc = "Nefarius Bluetooth PS Enumerator"
'@

    $profileIn = Join-Path $temp 'BthPS3.inf'
    $profileOut = Join-Path $temp 'partner-BthPS3.inf'
    [IO.File]::WriteAllText($profileIn, $profile)
    & $script -InputInf $profileIn -OutputInf $profileOut
    $text = [IO.File]::ReadAllText($profileOut)
    Assert-True ($text -match '(?m)^%ManufacturerName%=BthPS3,NTAMD64,NTARM64\s*$') 'profile manufacturer decorations'
    Assert-True ($text -notmatch '(?m)^%ManufacturerName%=BthPS3,NTAMD64,BthPS3,') 'profile does not repeat models name'
    Assert-True ($text -match '(?m)^\[BthPS3\.NTARM64\]') 'profile clones model section'
    Assert-True ($text -match '(?m)^\[SourceDisksFiles\.amd64\]') 'profile amd64 disks'
    Assert-True ($text -match '(?m)^\[SourceDisksFiles\.arm64\]') 'profile arm64 disks'
    Assert-True ($text -match '(?im)^BthPS3\.sys\s*=\s*1,x64') 'profile x64 sys path'
    Assert-True ($text -match '(?im)^BthPS3\.sys\s*=\s*1,ARM64') 'profile ARM64 sys path'

    $mixed = @'
[Version]
Signature="$WINDOWS NT$"
DriverVer=09/14/2026,2.12.0.2012

[Manufacturer]
%ManufacturerName%=BthPS3,NTAMD64,NTARM64
%OtherName%=Other,NTAMD64

[BthPS3.NTAMD64]
%BthPS3.DeviceDesc%=BthPS3_Device, BTHENUM\{1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}

[Other.NTAMD64]
%Other.DeviceDesc%=Other_Device, BTHENUM\{00000000-0000-0000-0000-000000000000}

[Strings]
ManufacturerName="Nefarius Software Solutions e.U."
OtherName="Other"
BthPS3.DeviceDesc = "Nefarius Bluetooth PS Enumerator"
Other.DeviceDesc = "Other"
'@

    $mixedIn = Join-Path $temp 'mixed.inf'
    $mixedOut = Join-Path $temp 'partner-mixed.inf'
    [IO.File]::WriteAllText($mixedIn, $mixed)
    & $script -InputInf $mixedIn -OutputInf $mixedOut
    $mixedText = [IO.File]::ReadAllText($mixedOut)
    Assert-True ($mixedText -match '(?m)^%ManufacturerName%=BthPS3,NTAMD64,NTARM64\s*$') 'mixed keeps existing dual-arch entry'
    Assert-True ($mixedText -match '(?m)^%OtherName%=Other,NTAMD64,NTARM64\s*$') 'mixed appends ARM64 to x64-only entry'
    Assert-True ($mixedText -match '(?m)^\[Other\.NTARM64\]') 'mixed clones x64-only model section'

    $filter = @'
[Version]
Signature = "$WINDOWS NT$"
DriverPackageType = ClassFilter
DriverVer = 09/14/2026,2.12.0.2012

[SourceDisksFiles]
BthPS3PSM.sys = 1,,

[DefaultInstall.NTAMD64]
CopyFiles = BthPS3PSM.DriverFiles

[DefaultUninstall.NTAMD64]
LegacyUninstall = 1

[BthPS3PSM.DriverFiles]
BthPS3PSM.sys

[DefaultInstall.NTAMD64.Services]
AddService = BthPS3PSM,,BthPS3PSM.Service

[Strings]
ManufacturerName = "Nefarius Software Solutions e.U."
'@

    $filterIn = Join-Path $temp 'BthPS3PSM.inf'
    $filterOut = Join-Path $temp 'partner-BthPS3PSM.inf'
    [IO.File]::WriteAllText($filterIn, $filter)
    & $script -InputInf $filterIn -OutputInf $filterOut
    $filterText = [IO.File]::ReadAllText($filterOut)
    Assert-True ($filterText -match '(?m)^\[DefaultInstall\.NTARM64\]') 'filter clones DefaultInstall'
    Assert-True ($filterText -match '(?m)^\[DefaultInstall\.NTARM64\.Services\]') 'filter clones services'
    Assert-True ($filterText -match '(?m)^\[DefaultUninstall\.NTARM64\]') 'filter clones uninstall'
    Assert-True ($filterText -match '(?im)^BthPS3PSM\.sys\s*=\s*1,x64') 'filter x64 sys path'
    Assert-True ($filterText -match '(?im)^BthPS3PSM\.sys\s*=\s*1,ARM64') 'filter ARM64 sys path'

    $nullPdo = @'
[Version]
Signature="$WINDOWS NT$"
DriverVer=09/14/2026,2.12.0.2012

[Manufacturer]
%ManufacturerName% = BthPS3_NULL_PDO,NTAMD64

[BthPS3_NULL_PDO.NTAMD64]
%SIXAXIS.DeviceDesc% = SIXAXIS, BTHPS3BUS\{53F88889-1AAF-4353-A047-556B69EC6DA6}

[SIXAXIS]
; NULL driver

[Strings]
ManufacturerName="Nefarius Software Solutions e.U."
SIXAXIS.DeviceDesc = "DS3 Compatible Bluetooth Device"
'@

    $nullIn = Join-Path $temp 'BthPS3_PDO_NULL_Device.inf'
    $nullOut = Join-Path $temp 'partner-null.inf'
    [IO.File]::WriteAllText($nullIn, $nullPdo)
    & $script -InputInf $nullIn -OutputInf $nullOut
    $nullText = [IO.File]::ReadAllText($nullOut)
    Assert-True ($nullText -match '(?m)^%ManufacturerName% = BthPS3_NULL_PDO,NTAMD64,NTARM64\s*$') 'null PDO manufacturer decorations'
    Assert-True ($nullText -match '(?m)^\[BthPS3_NULL_PDO\.NTARM64\]') 'null PDO clones models'
    Assert-True ($nullText -notmatch '(?m)^\[SourceDisksFiles\.amd64\]') 'null PDO has no source disks rewrite'

    $raw = Join-Path $temp 'raw.inf'
    [IO.File]::WriteAllText($raw, "[Version]`r`nDriverVer=`r`n[Manufacturer]`r`n%X%=Y,NT`$ARCH`$`r`n")
    Assert-Throws { & $script -InputInf $raw -OutputInf (Join-Path $temp 'bad.inf') } 'unexpanded ARCH rejected'
}
finally {
    Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Output 'New-PartnerSubmissionInf tests passed'
