#Requires -Version 7.0
<#
.SYNOPSIS
    Resolves setup tags, stages attested payloads, and writes setup provenance.

.DESCRIPTION
    Driver tags stay vMAJOR.MINOR.PATCH. The first setup tag for that payload is
    setup-vMAJOR.MINOR.PATCH; later setup-only re-spins use -r1, -r2, and so on.
    The MSI product version remains MAJOR.MINOR.PATCH.
#>
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-BthPS3RequiredAttestedDriverFiles {
    @(
        'BthPS3\BthPS3.inf'
        'BthPS3\BthPS3_PDO_NULL_Device.inf'
        'BthPS3\x64\BthPS3.sys'
        'BthPS3\ARM64\BthPS3.sys'
        'BthPS3PSM\BthPS3PSM.inf'
        'BthPS3PSM\x64\BthPS3PSM.sys'
        'BthPS3PSM\ARM64\BthPS3PSM.sys'
    )
}

function Get-BthPS3RequiredAttestedDriverBinaries {
    @(
        'BthPS3\x64\BthPS3.sys'
        'BthPS3\ARM64\BthPS3.sys'
        'BthPS3PSM\x64\BthPS3PSM.sys'
        'BthPS3PSM\ARM64\BthPS3PSM.sys'
    )
}

function Get-BthPS3SetupMsiFileName {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $SetupVersion
    )

    if ($SetupVersion -notmatch '^\d+\.\d+\.\d+$') {
        throw "SetupVersion must be MAJOR.MINOR.PATCH. Got: '$SetupVersion'."
    }

    "Nefarius_BthPS3_Drivers_x64_arm64_v$SetupVersion.msi"
}

function ConvertTo-BthPS3SetupReleaseIdentity {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [AllowEmptyString()]
        [string] $DriverTag
    )

    $tag = $DriverTag.Trim()
    if ($tag -match '^refs/tags/(.+)$') {
        $tag = $Matches[1]
    }

    if ($tag -notmatch '^v(\d+)\.(\d+)\.(\d+)$') {
        throw "Driver tags must be vMAJOR.MINOR.PATCH (example: v2.12.0). Got: '$DriverTag'."
    }

    $setup = $tag.Substring(1)
    return [pscustomobject]@{
        DriverTag     = $tag
        SetupVersion  = $setup
        SetupTagBase  = "setup-v$setup"
    }
}

function Get-BthPS3NextSetupReleaseTag {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $SetupVersion,

        [AllowEmptyCollection()]
        [AllowNull()]
        [string[]] $ExistingTags = @()
    )

    if ($SetupVersion -notmatch '^\d+\.\d+\.\d+$') {
        throw "SetupVersion must be MAJOR.MINOR.PATCH. Got: '$SetupVersion'."
    }

    if ($null -eq $ExistingTags) {
        $ExistingTags = @()
    }

    $base = "setup-v$SetupVersion"
    $escaped = [regex]::Escape($SetupVersion)
    $occupied = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($raw in $ExistingTags) {
        if ([string]::IsNullOrWhiteSpace($raw)) {
            continue
        }

        $name = $raw.Trim()
        if ($name -match '^refs/tags/(.+)$') {
            $name = $Matches[1]
        }

        if ($name -eq $base -or $name -match "^setup-v$escaped-r([1-9][0-9]*)$") {
            [void]$occupied.Add($name)
        }
    }

    if (-not $occupied.Contains($base)) {
        return $base
    }

    $revision = 1
    while ($occupied.Contains("$base-r$revision")) {
        $revision++
    }

    return "$base-r$revision"
}

function Assert-BthPS3ReleaseMetadataMatchesTag {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        $Metadata,

        [Parameter(Mandatory)]
        [string] $DriverTag
    )

    $identity = ConvertTo-BthPS3SetupReleaseIdentity -DriverTag $DriverTag
    $tag = [string]$Metadata.tag
    $setup = [string]$Metadata.setupVersion
    $driver = [string]$Metadata.driverVersion
    $commit = [string]$Metadata.commit

    if ($tag -ne $identity.DriverTag) {
        throw "Release metadata tag '$tag' does not match requested driver tag '$($identity.DriverTag)'."
    }

    if ($setup -ne $identity.SetupVersion) {
        throw "Release metadata setupVersion '$setup' must be '$($identity.SetupVersion)'."
    }

    if ($driver -notmatch '^\d+\.\d+\.\d+\.\d+$') {
        throw "Release metadata driverVersion is invalid: '$driver'."
    }

    if (-not $driver.StartsWith("$($identity.SetupVersion).", [StringComparison]::Ordinal)) {
        throw "Release metadata driverVersion '$driver' is not derived from setupVersion '$($identity.SetupVersion)'."
    }

    if ($commit -notmatch '^[0-9a-f]{40}$') {
        throw "Release metadata commit is missing or not a full SHA: '$commit'."
    }
}

function Find-BthPS3ExistingFile {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $Root,

        [Parameter(Mandatory)]
        [string[]] $RelativeCandidates
    )

    foreach ($relative in $RelativeCandidates) {
        $path = Join-Path $Root $relative
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }

    if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
        return $null
    }

    foreach ($relative in $RelativeCandidates) {
        $leaf = Split-Path -Leaf $relative
        $normalized = $relative.Replace('\', '/')
        $match = Get-ChildItem -LiteralPath $Root -Recurse -File -Filter $leaf -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName.Replace('\', '/').EndsWith($normalized, [StringComparison]::OrdinalIgnoreCase)
            } |
            Select-Object -First 1
        if ($match) {
            return $match.FullName
        }
    }

    return $null
}

function Resolve-BthPS3AttestedDriversRoot {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $Source
    )

    if (Test-Path -LiteralPath (Join-Path $Source 'BthPS3\BthPS3.inf') -PathType Leaf) {
        return (Resolve-Path -LiteralPath $Source).Path
    }

    $inf = Find-BthPS3ExistingFile -Root $Source -RelativeCandidates @('BthPS3\BthPS3.inf')
    if (-not $inf) {
        throw "Microsoft-attested drivers were not found under $Source."
    }

    $package = Split-Path -Parent $inf
    $root = Split-Path -Parent $package
    if (-not (Test-Path -LiteralPath (Join-Path $root 'BthPS3PSM\BthPS3PSM.inf') -PathType Leaf)) {
        throw "Could not find both BthPS3 and BthPS3PSM packages under $Source."
    }

    return $root
}

function Assert-BthPS3DriverLayout {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $DriversRoot
    )

    $missing = @()
    foreach ($relative in Get-BthPS3RequiredAttestedDriverFiles) {
        $path = Join-Path $DriversRoot $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            $missing += $path
        }
    }

    if ($missing.Count -gt 0) {
        throw "Driver package is missing required files:`n$($missing -join [Environment]::NewLine)"
    }
}

function Copy-BthPS3SetupPayload {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $DriversSource,

        [Parameter(Mandatory)]
        [string] $ToolsSource,

        [Parameter(Mandatory)]
        [string] $SetupRoot
    )

    $driversRoot = Resolve-BthPS3AttestedDriversRoot -Source $DriversSource
    Assert-BthPS3DriverLayout -DriversRoot $driversRoot

    $cfgUi = Find-BthPS3ExistingFile -Root $ToolsSource -RelativeCandidates @(
        'bin\BthPS3CfgUI.exe'
        'BthPS3CfgUI.exe'
    )
    if (-not $cfgUi) {
        throw "Downloaded tools are missing BthPS3CfgUI.exe under $ToolsSource."
    }

    $setupDrivers = Join-Path $SetupRoot 'drivers'
    $setupBin = Join-Path $SetupRoot 'artifacts\bin'
    foreach ($path in @($setupDrivers, $setupBin)) {
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $setupBin) | Out-Null
    Copy-Item -LiteralPath $driversRoot -Destination $setupDrivers -Recurse

    $binSource = Split-Path -Parent $cfgUi
    if ((Split-Path -Leaf $binSource) -eq 'bin') {
        Copy-Item -LiteralPath $binSource -Destination $setupBin -Recurse
    }
    else {
        New-Item -ItemType Directory -Force -Path $setupBin | Out-Null
        Copy-Item -LiteralPath $cfgUi -Destination (Join-Path $setupBin 'BthPS3CfgUI.exe')
    }

    Assert-BthPS3SetupPayload -SetupRoot $SetupRoot
}

function Assert-BthPS3SetupPayload {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $SetupRoot
    )

    $required = @(
        (Get-BthPS3RequiredAttestedDriverFiles | ForEach-Object { Join-Path 'drivers' $_ })
        'artifacts\bin\BthPS3CfgUI.exe'
    )
    $missing = @()
    foreach ($relative in $required) {
        $path = Join-Path $SetupRoot $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            $missing += $path
        }
    }

    if ($missing.Count -gt 0) {
        throw "Setup payload is incomplete, missing:`n$($missing -join [Environment]::NewLine)"
    }
}

function Assert-BthPS3AttestedDriverSignatures {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $DriversRoot
    )

    foreach ($relative in Get-BthPS3RequiredAttestedDriverBinaries) {
        $file = Join-Path $DriversRoot $relative
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
            throw "Missing attested driver file: $file"
        }

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
}

function New-BthPS3SetupProvenance {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $DriverTag,

        [Parameter(Mandatory)]
        [string] $SetupVersion,

        [Parameter(Mandatory)]
        [string] $SetupTag,

        [Parameter(Mandatory)]
        [string] $DriverVersion,

        [Parameter(Mandatory)]
        [string] $DriverCommit,

        [Parameter(Mandatory)]
        [string] $SetupCommit,

        [Parameter(Mandatory)]
        [long] $BuildRunId,

        [Parameter(Mandatory)]
        [long] $DriversRunId,

        [Parameter(Mandatory)]
        [string] $Repository,

        [Parameter(Mandatory)]
        [string] $MsiName,

        [Parameter(Mandatory)]
        [string] $MsiSha256
    )

    $identity = ConvertTo-BthPS3SetupReleaseIdentity -DriverTag $DriverTag
    if ($SetupVersion -ne $identity.SetupVersion) {
        throw "SetupVersion '$SetupVersion' does not match driver tag '$($identity.DriverTag)'."
    }

    $escaped = [regex]::Escape($identity.SetupVersion)
    if ($SetupTag -ne $identity.SetupTagBase -and $SetupTag -notmatch "^setup-v$escaped-r([1-9][0-9]*)$") {
        throw "Setup tag '$SetupTag' is not $($identity.SetupTagBase) or a -rN re-spin."
    }

    if ($DriverVersion -notmatch '^\d+\.\d+\.\d+\.\d+$' -or
        -not $DriverVersion.StartsWith("$($identity.SetupVersion).", [StringComparison]::Ordinal)) {
        throw "DriverVersion '$DriverVersion' is not derived from '$($identity.SetupVersion)'."
    }

    foreach ($name in @('DriverCommit', 'SetupCommit')) {
        $value = Get-Variable $name -ValueOnly
        if ($value -notmatch '^[0-9a-f]{40}$') {
            throw "$name is missing or not a full SHA: '$value'."
        }
    }

    if ($BuildRunId -le 0 -or $DriversRunId -le 0) {
        throw 'BuildRunId and DriversRunId must be positive.'
    }

    if ($MsiName -ne (Get-BthPS3SetupMsiFileName -SetupVersion $identity.SetupVersion)) {
        throw "MSI name '$MsiName' does not match setup version '$($identity.SetupVersion)'."
    }

    if ($MsiSha256 -notmatch '^[0-9a-f]{64}$') {
        throw "MSI SHA-256 is invalid: '$MsiSha256'."
    }

    [ordered]@{
        schemaVersion = 1
        driverTag     = $identity.DriverTag
        setupVersion  = $identity.SetupVersion
        setupTag      = $SetupTag
        driverVersion = $DriverVersion
        driverCommit  = $DriverCommit
        setupCommit   = $SetupCommit
        buildRunId    = $BuildRunId
        driversRunId  = $DriversRunId
        repository    = $Repository
        files         = [ordered]@{
            msi = [ordered]@{
                name   = $MsiName
                sha256 = $MsiSha256
            }
        }
    }
}

function Write-BthPS3Utf8NoBomJson {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        $Object,

        [Parameter(Mandatory)]
        [string] $Path
    )

    $directory = Split-Path -Parent $Path
    if ($directory -and -not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }

    $json = $Object | ConvertTo-Json -Depth 8
    [IO.File]::WriteAllText($Path, $json, [Text.UTF8Encoding]::new($false))
}

function Write-BthPS3GitHubOutput {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [hashtable] $Values,

        [Parameter(Mandatory)]
        [string] $GitHubOutput
    )

    $directory = Split-Path -Parent $GitHubOutput
    if ($directory -and -not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }

    $lines = foreach ($key in $Values.Keys) {
        "$key=$($Values[$key])"
    }

    Add-Content -LiteralPath $GitHubOutput -Value $lines -Encoding utf8
    return $lines
}

function Get-BthPS3GitHubArtifactNames {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $Repository,

        [Parameter(Mandatory)]
        [long] $RunId
    )

    $payload = gh api "repos/$Repository/actions/runs/$RunId/artifacts" | ConvertFrom-Json
    if ($LASTEXITCODE) {
        throw "Failed to list artifacts for run $RunId."
    }

    @($payload.artifacts | Where-Object { -not $_.expired } | ForEach-Object { [string]$_.name })
}

function Save-BthPS3GitHubArtifact {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $Repository,

        [Parameter(Mandatory)]
        [long] $RunId,

        [Parameter(Mandatory)]
        [string] $Name,

        [Parameter(Mandatory)]
        [string] $Directory
    )

    if (Test-Path -LiteralPath $Directory) {
        Remove-Item -LiteralPath $Directory -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    gh run download $RunId --repo $Repository --name $Name --dir $Directory *>$null
    if ($LASTEXITCODE) {
        throw "Failed to download artifact '$Name' from run $RunId."
    }
}

function Read-BthPS3ReleaseMetadataFile {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $Path
    )

    $file = Get-ChildItem -LiteralPath $Path -Recurse -File -Filter 'release-metadata.json' |
        Select-Object -First 1
    if (-not $file) {
        throw "release-metadata.json was not found under $Path."
    }

    Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
}

function Get-BthPS3ExistingSetupReleaseTags {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $Repository,

        [Parameter(Mandatory)]
        [string] $SetupVersion
    )

    if ($SetupVersion -notmatch '^\d+\.\d+\.\d+$') {
        throw "SetupVersion must be MAJOR.MINOR.PATCH. Got: '$SetupVersion'."
    }

    $prefix = "setup-v$SetupVersion"
    $refs = gh api "repos/$Repository/git/matching-refs/tags/$prefix" | ConvertFrom-Json
    if ($LASTEXITCODE) {
        throw "Failed to list tags matching $prefix."
    }

    @($refs | ForEach-Object { [string]$_.ref -replace '^refs/tags/', '' })
}

function Resolve-BthPS3SetupArtifactRuns {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $Repository,

        [Parameter(Mandatory)]
        [string] $DriverTag
    )

    $identity = ConvertTo-BthPS3SetupReleaseIdentity -DriverTag $DriverTag
    $seen = [System.Collections.Generic.HashSet[long]]::new()
    $candidates = [System.Collections.Generic.List[object]]::new()

    # Call gh directly. A PowerShell array-of-arrays flattens, and splatting a
    # string then invokes `gh r u n` ("unknown command r").
    $jsonFields = 'databaseId,headSha,createdAt,event,headBranch,url'
    $listedJson = [System.Collections.Generic.List[string]]::new()
    $listedJson.Add([string](gh run list --repo $Repository --workflow build.yml --branch $identity.DriverTag --status success --limit 20 --json $jsonFields))
    if ($LASTEXITCODE) {
        throw "Failed to list Build workflow runs for $($identity.DriverTag)."
    }

    $listedJson.Add([string](gh run list --repo $Repository --workflow build.yml --event workflow_dispatch --status success --limit 30 --json $jsonFields))
    if ($LASTEXITCODE) {
        throw "Failed to list Build workflow runs for $($identity.DriverTag)."
    }

    foreach ($json in $listedJson) {
        $runs = $json | ConvertFrom-Json
        foreach ($run in @($runs)) {
            $id = [int64]$run.databaseId
            if ($seen.Add($id)) {
                $candidates.Add($run)
            }
        }
    }

    $candidates = @($candidates | Sort-Object { Get-Date $_.createdAt } -Descending)
    $stage = Join-Path ([IO.Path]::GetTempPath()) ("bthps3-setup-resolve-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $stage | Out-Null

    try {
        foreach ($run in $candidates) {
            $runId = [int64]$run.databaseId
            $names = Get-BthPS3GitHubArtifactNames -Repository $Repository -RunId $runId
            if ($names -notcontains 'release-metadata' -or $names -notcontains 'bthps3-tools') {
                Write-Output "Skipping Build run $runId; missing release-metadata or bthps3-tools."
                continue
            }

            $metaDir = Join-Path $stage "build-$runId"
            Save-BthPS3GitHubArtifact -Repository $Repository -RunId $runId -Name 'release-metadata' -Directory $metaDir
            $metadata = Read-BthPS3ReleaseMetadataFile -Path $metaDir
            try {
                Assert-BthPS3ReleaseMetadataMatchesTag -Metadata $metadata -DriverTag $identity.DriverTag
            }
            catch {
                Write-Output "Skipping Build run ${runId}: $($_.Exception.Message)"
                continue
            }

            $driversRunId = $null
            if ($names -contains 'bthps3-microsoft-drivers') {
                $driversRunId = $runId
            }
            else {
                $driversRunId = Find-BthPS3MicrosoftDriversRun -Repository $Repository -DriverTag $identity.DriverTag -Stage $stage
            }

            if (-not $driversRunId) {
                throw "Build run $runId has tools for $($identity.DriverTag), but bthps3-microsoft-drivers is missing. Wait for Partner Center signing to finish, then re-dispatch."
            }

            return [pscustomobject]@{
                BuildRunId    = $runId
                DriversRunId  = [int64]$driversRunId
                DriverVersion = [string]$metadata.driverVersion
                DriverCommit  = [string]$metadata.commit
                Metadata      = $metadata
            }
        }
    }
    finally {
        if (Test-Path -LiteralPath $stage) {
            Remove-Item -LiteralPath $stage -Recurse -Force
        }
    }

    throw "No successful Build run with release-metadata and bthps3-tools was found for $($identity.DriverTag)."
}

function Find-BthPS3MicrosoftDriversRun {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $Repository,

        [Parameter(Mandatory)]
        [string] $DriverTag,

        [Parameter(Mandatory)]
        [string] $Stage
    )

    $identity = ConvertTo-BthPS3SetupReleaseIdentity -DriverTag $DriverTag
    $partnerRuns = gh run list --repo $Repository --workflow partner-signing.yml --status success --limit 50 --json databaseId,createdAt |
        ConvertFrom-Json
    if ($LASTEXITCODE) {
        throw 'Failed to list Partner Center signing runs.'
    }

    foreach ($run in @($partnerRuns | Sort-Object { Get-Date $_.createdAt } -Descending)) {
        $runId = [int64]$run.databaseId
        $names = Get-BthPS3GitHubArtifactNames -Repository $Repository -RunId $runId
        if ($names -notcontains 'bthps3-microsoft-drivers') {
            continue
        }

        if ($names -notcontains 'release-metadata') {
            Write-Host "Partner Center run $runId has attested drivers but no release-metadata; skipping identity check fallback."
            continue
        }

        $metaDir = Join-Path $Stage "partner-$runId"
        Save-BthPS3GitHubArtifact -Repository $Repository -RunId $runId -Name 'release-metadata' -Directory $metaDir
        $metadata = Read-BthPS3ReleaseMetadataFile -Path $metaDir
        try {
            Assert-BthPS3ReleaseMetadataMatchesTag -Metadata $metadata -DriverTag $identity.DriverTag
        }
        catch {
            Write-Host "Skipping Partner Center run ${runId}: $($_.Exception.Message)"
            continue
        }

        return $runId
    }

    return $null
}

function New-BthPS3SetupGitTag {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $Repository,

        [Parameter(Mandatory)]
        [string] $Tag,

        [Parameter(Mandatory)]
        [string] $Sha
    )

    if ($Sha -notmatch '^[0-9a-f]{40}$') {
        throw "Tag target SHA is missing or not a full commit: '$Sha'."
    }

    if ($Tag -notmatch '^setup-v\d+\.\d+\.\d+$' -and $Tag -notmatch '^setup-v\d+\.\d+\.\d+-r[1-9][0-9]*$') {
        throw "Refusing to create unexpected setup tag '$Tag'."
    }

    gh api -X POST "repos/$Repository/git/refs" -f ref="refs/tags/$Tag" -f sha=$Sha
    if ($LASTEXITCODE) {
        throw "Failed to create tag $Tag at $Sha."
    }

    return $Tag
}
