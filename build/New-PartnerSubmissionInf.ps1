#Requires -Version 5.1
<#
.SYNOPSIS
    Expands a stampinf-processed x64 INF into a dual-arch Partner Center INF.

.DESCRIPTION
    Partner Center requires every driver folder in a CAB to declare every requested
    architecture. This takes an already-stamped x64 INF (DriverVer filled) and emits
    NTARM64 decorations plus SourceDisksFiles.amd64/.arm64 so one package folder can
    carry both SYS binaries.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $InputInf,

    [Parameter(Mandatory)]
    [string] $OutputInf
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$InputInf = $PSCmdlet.GetUnresolvedProviderPathFromPSPath($InputInf)
$OutputInf = $PSCmdlet.GetUnresolvedProviderPathFromPSPath($OutputInf)

if (-not (Test-Path -LiteralPath $InputInf)) {
    throw "Input INF not found: $InputInf"
}

$text = [System.IO.File]::ReadAllText($InputInf)
if ([string]::IsNullOrWhiteSpace($text)) {
    throw "Input INF is empty: $InputInf"
}

if ($text -match '\$ARCH\$' -or $text -match 'NT\$ARCH\$') {
    throw "Input INF is not stampinf-processed (unexpanded `$ARCH`$ remains): $InputInf"
}

$newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }

$headerMatches = [regex]::Matches($text, '(?m)^(\[[^\]]+\])\r?\n')
if ($headerMatches.Count -eq 0) {
    throw "No INF sections found in $InputInf"
}

$preamble = $text.Substring(0, $headerMatches[0].Index)
$sections = [System.Collections.Generic.List[object]]::new()
for ($i = 0; $i -lt $headerMatches.Count; $i++) {
    $header = $headerMatches[$i].Groups[1].Value
    $bodyStart = $headerMatches[$i].Index + $headerMatches[$i].Length
    $bodyEnd = if ($i + 1 -lt $headerMatches.Count) { $headerMatches[$i + 1].Index } else { $text.Length }
    $sections.Add([pscustomobject]@{
            Header = $header
            Body   = $text.Substring($bodyStart, $bodyEnd - $bodyStart)
        })
}

function ConvertTo-Arm64Decoration([string] $Value) {
    return (($Value -creplace 'NTAMD64', 'NTARM64') -creplace 'NTamd64', 'NTarm64')
}

$outSections = [System.Collections.Generic.List[object]]::new()
$clonedSections = 0
$replacedSourceDisks = $false
$hasSourceDisks = $false

foreach ($section in $sections) {
    if ($section.Header -eq '[Manufacturer]') {
        if ($section.Body -notmatch 'NTAMD64' -and $section.Body -notmatch 'NTamd64') {
            throw "[Manufacturer] does not contain NTAMD64 decorations; expected a stamped x64 INF."
        }
        # %ManufacturerName%=ModelsName,NTamd64[,NTamd64.10.0...]
        # becomes ModelsName,NTamd64,NTarm64 — not ModelsName,NTamd64,ModelsName,NTarm64.
        $section.Body = [regex]::Replace($section.Body, '(?m)^(.+?=)([^,\r\n]+)(,.*)?$', {
                param($match)
                $prefix = $match.Groups[1].Value
                $models = $match.Groups[2].Value
                $osVersions = if ($match.Groups[3].Success) { $match.Groups[3].Value.TrimEnd() } else { '' }
                if ($osVersions -match 'NTARM64' -or $osVersions -match 'NTarm64') {
                    return $match.Value
                }
                if ($osVersions -notmatch 'NTAMD64' -and $osVersions -notmatch 'NTamd64') {
                    return $match.Value
                }
                $prefix + $models + $osVersions + (ConvertTo-Arm64Decoration $osVersions)
            })
        $outSections.Add($section)
        continue
    }

    if ($section.Header -eq '[SourceDisksFiles]') {
        $hasSourceDisks = $true
        $fileLines = [regex]::Matches($section.Body, '(?im)^([A-Za-z0-9_\.\-]+\.(sys|dll))\s*=\s*1(?:,,)?\s*$')
        if ($fileLines.Count -eq 0) {
            $outSections.Add($section)
            continue
        }

        $amdBody = [regex]::Replace($section.Body, '(?im)^([A-Za-z0-9_\.\-]+\.(sys|dll)\s*=\s*)1(?:,,)?(\s*)$', '${1}1,x64$3')
        $armBody = [regex]::Replace($section.Body, '(?im)^([A-Za-z0-9_\.\-]+\.(sys|dll)\s*=\s*)1(?:,,)?(\s*)$', '${1}1,ARM64$3')
        $outSections.Add([pscustomobject]@{ Header = '[SourceDisksFiles.amd64]'; Body = $amdBody })
        $outSections.Add([pscustomobject]@{ Header = '[SourceDisksFiles.arm64]'; Body = $armBody })
        $replacedSourceDisks = $true
        continue
    }

    if ($section.Header -match '^\[SourceDisksFiles\.') {
        throw "Input INF already has decorated $($section.Header); expected undecorated [SourceDisksFiles] from a per-arch WDK build."
    }

    $outSections.Add($section)

    if ($section.Header -match 'NTAMD64' -or $section.Header -match 'NTamd64') {
        $armHeader = ConvertTo-Arm64Decoration $section.Header
        $outSections.Add([pscustomobject]@{ Header = $armHeader; Body = $section.Body })
        $clonedSections++
    }
}

if ($hasSourceDisks -and -not $replacedSourceDisks -and $text -match '(?im)\.(sys|dll)\s*=') {
    throw "Input INF has [SourceDisksFiles] binaries but they were not rewritten for dual-arch."
}

if ($clonedSections -lt 1) {
    throw "Expected to clone at least one NTAMD64 section; cloned $clonedSections."
}

$builder = New-Object System.Text.StringBuilder
[void]$builder.Append($preamble)
foreach ($section in $outSections) {
    [void]$builder.Append($section.Header)
    [void]$builder.Append($newline)
    [void]$builder.Append($section.Body)
}

$result = $builder.ToString()

if ($result -match '\$ARCH\$') {
    throw "Generated INF still contains unexpanded tokens."
}

if ($replacedSourceDisks) {
    if (-not [regex]::IsMatch($result, '(?im)^\[SourceDisksFiles\.amd64\]\r?$')) {
        throw 'Generated INF is missing [SourceDisksFiles.amd64].'
    }
    if (-not [regex]::IsMatch($result, '(?im)^\[SourceDisksFiles\.arm64\]\r?$')) {
        throw 'Generated INF is missing [SourceDisksFiles.arm64].'
    }
    if ([regex]::IsMatch($result, '(?m)^\[SourceDisksFiles\]\s*$')) {
        throw 'Generated INF still has undecorated [SourceDisksFiles].'
    }
}

$outputDir = Split-Path -Parent $OutputInf
if ($outputDir -and -not (Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

$utf8 = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText($OutputInf, $result, $utf8)
Write-Output "Wrote dual-arch partner INF: $OutputInf"
