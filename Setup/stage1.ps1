#Requires -Version 7.0
Param(
    [Parameter(Mandatory = $false)]
    [string] $Path = './artifacts'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$drivers = Join-Path $repoRoot (Join-Path $Path 'drivers')
if (-not (Test-Path -LiteralPath $drivers)) {
    throw "Microsoft-attested drivers were not found at $drivers. Run stage0.ps1 against a completed Partner Center run."
}

$required = @(
    'BthPS3\BthPS3.inf'
    'BthPS3\BthPS3_PDO_NULL_Device.inf'
    'BthPS3\x64\BthPS3.sys'
    'BthPS3\ARM64\BthPS3.sys'
    'BthPS3PSM\BthPS3PSM.inf'
    'BthPS3PSM\x64\BthPS3PSM.sys'
    'BthPS3PSM\ARM64\BthPS3PSM.sys'
)

foreach ($relative in $required) {
    $file = Join-Path $drivers $relative
    if (-not (Test-Path -LiteralPath $file)) {
        throw "Missing attested driver file: $file"
    }
}

$binaries = @(
    'BthPS3\x64\BthPS3.sys'
    'BthPS3\ARM64\BthPS3.sys'
    'BthPS3PSM\x64\BthPS3PSM.sys'
    'BthPS3PSM\ARM64\BthPS3PSM.sys'
)

foreach ($relative in $binaries) {
    $file = Join-Path $drivers $relative
    $sig = Get-AuthenticodeSignature -LiteralPath $file
    if ($sig.Status -ne 'Valid' -or -not $sig.SignerCertificate) {
        throw "Attested binary is not validly signed: $file (Status=$($sig.Status))."
    }

    $subject = $sig.SignerCertificate.Subject
    if ($subject -notmatch 'Nefarius Software Solutions e\.U\.' -and $subject -notmatch 'Microsoft') {
        throw "Unexpected signer on $file : $subject"
    }

    Write-Output "Valid: $file ($subject)"
}

$setupDrivers = Join-Path $PSScriptRoot 'drivers'
if (Test-Path -LiteralPath $setupDrivers) {
    Remove-Item -LiteralPath $setupDrivers -Recurse -Force
}

Copy-Item -LiteralPath $drivers -Destination $setupDrivers -Recurse
Write-Output "Copied attested drivers to $setupDrivers"
Write-Output 'Do not append another publisher signature after Microsoft attestation.'
