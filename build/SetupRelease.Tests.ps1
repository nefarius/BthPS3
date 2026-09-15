#Requires -Version 7.0
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $here 'SetupRelease.ps1')

function Assert-Equal($Actual, $Expected, [string] $Name) {
    if ($Actual -cne $Expected) {
        throw "FAIL ${Name}: expected '${Expected}', got '${Actual}'"
    }
    Write-Output "PASS $Name"
}

function Assert-True([bool] $Condition, [string] $Name) {
    if (-not $Condition) {
        throw "FAIL $Name"
    }
    Write-Output "PASS $Name"
}

function Assert-Throws([scriptblock] $Action, [string] $Name) {
    $threw = $false
    try {
        & $Action
    }
    catch {
        $threw = $true
    }

    if (-not $threw) {
        throw "FAIL ${Name}: expected an exception"
    }

    Write-Output "PASS $Name"
}

function New-TestDriverPackage {
    param([string] $Root)

    New-Item -ItemType Directory -Force -Path @(
        (Join-Path $Root 'BthPS3\x64')
        (Join-Path $Root 'BthPS3\ARM64')
        (Join-Path $Root 'BthPS3PSM\x64')
        (Join-Path $Root 'BthPS3PSM\ARM64')
    ) | Out-Null
    Set-Content -LiteralPath (Join-Path $Root 'BthPS3\BthPS3.inf') -Value 'inf' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $Root 'BthPS3\BthPS3_PDO_NULL_Device.inf') -Value 'null' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $Root 'BthPS3\x64\BthPS3.sys') -Value 'sys' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $Root 'BthPS3\ARM64\BthPS3.sys') -Value 'sys' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $Root 'BthPS3PSM\BthPS3PSM.inf') -Value 'inf' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $Root 'BthPS3PSM\x64\BthPS3PSM.sys') -Value 'sys' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $Root 'BthPS3PSM\ARM64\BthPS3PSM.sys') -Value 'sys' -Encoding utf8
    return $Root
}

$identity = ConvertTo-BthPS3SetupReleaseIdentity -DriverTag 'v2.12.0'
Assert-Equal $identity.DriverTag 'v2.12.0' 'identity tag'
Assert-Equal $identity.SetupVersion '2.12.0' 'identity setup version'
Assert-Equal $identity.SetupTagBase 'setup-v2.12.0' 'identity setup tag base'

$fromRef = ConvertTo-BthPS3SetupReleaseIdentity -DriverTag 'refs/tags/v2.12.0'
Assert-Equal $fromRef.SetupVersion '2.12.0' 'identity strips refs/tags prefix'

Assert-Throws { ConvertTo-BthPS3SetupReleaseIdentity -DriverTag '2.12.0' } 'bare version rejected'
Assert-Throws { ConvertTo-BthPS3SetupReleaseIdentity -DriverTag 'setup-v2.12.0' } 'setup tag rejected as driver tag'
Assert-Throws { ConvertTo-BthPS3SetupReleaseIdentity -DriverTag 'v2.12.0.1' } 'four-part tag rejected'
Assert-Throws { ConvertTo-BthPS3SetupReleaseIdentity -DriverTag 'v2.12.0-pre1' } 'prerelease tag rejected'
Assert-Throws { ConvertTo-BthPS3SetupReleaseIdentity -DriverTag 'v2.12.0-r1' } 'revision suffix rejected as driver tag'

Assert-Equal (Get-BthPS3NextSetupReleaseTag -SetupVersion '2.12.0' -ExistingTags @()) 'setup-v2.12.0' 'first setup tag'
Assert-Equal (Get-BthPS3NextSetupReleaseTag -SetupVersion '2.12.0' -ExistingTags @('setup-v2.12.0')) 'setup-v2.12.0-r1' 'first re-spin'
Assert-Equal (Get-BthPS3NextSetupReleaseTag -SetupVersion '2.12.0' -ExistingTags @('setup-v2.12.0', 'setup-v2.12.0-r1')) 'setup-v2.12.0-r2' 'second re-spin'
Assert-Equal (Get-BthPS3NextSetupReleaseTag -SetupVersion '2.12.0' -ExistingTags @('setup-v2.12.0', 'setup-v2.12.0-r2')) 'setup-v2.12.0-r1' 'fills revision gap'
Assert-Equal (Get-BthPS3NextSetupReleaseTag -SetupVersion '2.12.0' -ExistingTags @('setup-v2.12.1', 'v2.12.0', 'setup-v2.12.0-beta')) 'setup-v2.12.0' 'unrelated tags ignored'
Assert-Equal (Get-BthPS3NextSetupReleaseTag -SetupVersion '2.12.0' -ExistingTags @('refs/tags/setup-v2.12.0')) 'setup-v2.12.0-r1' 'refs/tags prefix occupies base tag'
Assert-Throws { Get-BthPS3NextSetupReleaseTag -SetupVersion '2.12' -ExistingTags @() } 'short setup version rejected'

$metadata = [pscustomobject]@{
    tag           = 'v2.12.0'
    setupVersion  = '2.12.0'
    driverVersion = '2.12.0.2012'
    commit        = '0123456789abcdef0123456789abcdef01234567'
}
Assert-BthPS3ReleaseMetadataMatchesTag -Metadata $metadata -DriverTag 'v2.12.0'
Write-Output 'PASS metadata matches driver tag'

$badTag = [pscustomobject]@{
    tag           = 'v2.11.0'
    setupVersion  = '2.11.0'
    driverVersion = '2.11.0.2012'
    commit        = $metadata.commit
}
Assert-Throws { Assert-BthPS3ReleaseMetadataMatchesTag -Metadata $badTag -DriverTag 'v2.12.0' } 'metadata tag mismatch'

$badDriver = [pscustomobject]@{
    tag           = 'v2.12.0'
    setupVersion  = '2.12.0'
    driverVersion = '2.11.0.2012'
    commit        = $metadata.commit
}
Assert-Throws { Assert-BthPS3ReleaseMetadataMatchesTag -Metadata $badDriver -DriverTag 'v2.12.0' } 'metadata driver version mismatch'

$badCommit = [pscustomobject]@{
    tag           = 'v2.12.0'
    setupVersion  = '2.12.0'
    driverVersion = '2.12.0.2012'
    commit        = 'abc'
}
Assert-Throws { Assert-BthPS3ReleaseMetadataMatchesTag -Metadata $badCommit -DriverTag 'v2.12.0' } 'short commit rejected'

Assert-Equal (Get-BthPS3SetupMsiFileName -SetupVersion '2.12.0') 'Nefarius_BthPS3_Drivers_x64_arm64_v2.12.0.msi' 'msi file name'

$provenance = New-BthPS3SetupProvenance `
    -DriverTag 'v2.12.0' `
    -SetupVersion '2.12.0' `
    -SetupTag 'setup-v2.12.0-r1' `
    -DriverVersion '2.12.0.2012' `
    -DriverCommit $metadata.commit `
    -SetupCommit 'fedcba9876543210fedcba9876543210fedcba98' `
    -BuildRunId 11 `
    -DriversRunId 22 `
    -Repository 'nefarius/BthPS3' `
    -MsiName 'Nefarius_BthPS3_Drivers_x64_arm64_v2.12.0.msi' `
    -MsiSha256 ('a' * 64)
Assert-Equal $provenance.setupTag 'setup-v2.12.0-r1' 'provenance setup tag'
Assert-Equal $provenance.files.msi.sha256 ('a' * 64) 'provenance msi hash'
try {
    New-BthPS3SetupProvenance `
        -DriverTag 'v2.12.0' `
        -SetupVersion '2.12.0' `
        -SetupTag 'setup-v2.12.1' `
        -DriverVersion '2.12.0.2012' `
        -DriverCommit $metadata.commit `
        -SetupCommit $metadata.commit `
        -BuildRunId 11 `
        -DriversRunId 22 `
        -Repository 'nefarius/BthPS3' `
        -MsiName 'Nefarius_BthPS3_Drivers_x64_arm64_v2.12.0.msi' `
        -MsiSha256 ('a' * 64)
    throw 'FAIL provenance rejects foreign setup tag: expected an exception'
}
catch {
    if ($_.Exception.Message -eq 'FAIL provenance rejects foreign setup tag: expected an exception') {
        throw
    }
    if ($_.Exception.Message -cne "Setup tag 'setup-v2.12.1' is not setup-v2.12.0 or a -rN re-spin.") {
        throw "FAIL provenance interpolates SetupTagBase: $($_.Exception.Message)"
    }
    Write-Output 'PASS provenance rejects foreign setup tag'
    Write-Output 'PASS provenance interpolates SetupTagBase'
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("bthps3-setup-tests-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
try {
    $jsonPath = Join-Path $tempRoot 'setup-metadata.json'
    Write-BthPS3Utf8NoBomJson -Object $provenance -Path $jsonPath
    $bytes = [IO.File]::ReadAllBytes($jsonPath)
    Assert-True ($bytes.Length -gt 0 -and $bytes[0] -ne 0xEF) 'provenance json has no BOM'
    $roundTrip = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
    Assert-Equal $roundTrip.setupTag 'setup-v2.12.0-r1' 'provenance json round trip'

    $drivers = New-TestDriverPackage (Join-Path $tempRoot 'drivers-src')
    $tools = Join-Path $tempRoot 'tools-src\bin'
    New-Item -ItemType Directory -Force -Path $tools | Out-Null
    Set-Content -LiteralPath (Join-Path $tools 'BthPS3CfgUI.exe') -Value 'ui' -Encoding utf8
    $setup = Join-Path $tempRoot 'Setup'
    Copy-BthPS3SetupPayload -DriversSource $drivers -ToolsSource (Join-Path $tempRoot 'tools-src') -SetupRoot $setup
    Assert-BthPS3SetupPayload -SetupRoot $setup
    Write-Output 'PASS staged payload layout'

    Remove-Item -LiteralPath (Join-Path $setup 'drivers\BthPS3\x64\BthPS3.sys') -Force
    Assert-Throws { Assert-BthPS3SetupPayload -SetupRoot $setup } 'incomplete payload rejected'

    $nested = Join-Path $tempRoot 'nested\bthps3-microsoft-drivers'
    New-TestDriverPackage $nested | Out-Null
    $flatTools = Join-Path $tempRoot 'flat-tools'
    New-Item -ItemType Directory -Force -Path $flatTools | Out-Null
    Set-Content -LiteralPath (Join-Path $flatTools 'BthPS3CfgUI.exe') -Value 'ui' -Encoding utf8
    $setup2 = Join-Path $tempRoot 'Setup2'
    Copy-BthPS3SetupPayload -DriversSource (Split-Path -Parent $nested) -ToolsSource $flatTools -SetupRoot $setup2
    Assert-BthPS3SetupPayload -SetupRoot $setup2
    Write-Output 'PASS nested drivers and flat tools'

    $emptyTools = Join-Path $tempRoot 'empty-tools'
    New-Item -ItemType Directory -Force -Path $emptyTools | Out-Null
    Assert-Throws {
        Copy-BthPS3SetupPayload -DriversSource $drivers -ToolsSource $emptyTools -SetupRoot (Join-Path $tempRoot 'Setup3')
    } 'missing tools rejected'
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

Write-Output 'SetupRelease tests passed'
