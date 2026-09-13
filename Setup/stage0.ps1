#Requires -Version 7.0
Param(
    [Parameter(Mandatory = $true)]
    [string] $RunId,
    [Parameter(Mandatory = $false)]
    [string] $Path = './artifacts'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($RunId -notmatch '^\d+$') {
    throw "RunId must be a numeric GitHub Actions run ID. Got: '$RunId'."
}

$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    & .\build.cmd DownloadCiArtifacts --run-id $RunId --artifacts-path $Path
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}

Write-Output "Staged GitHub Actions run $RunId into $Path"
Write-Output "Helper product name: BthPS3 $(Get-Date -Format 'dd.MM.yyyy')"
