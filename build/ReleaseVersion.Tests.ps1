#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $here 'ReleaseVersion.ps1')

function Assert-Equal($Actual, $Expected, [string] $Name) {
    if ($Actual -cne $Expected) {
        throw "FAIL ${Name}: expected '${Expected}', got '${Actual}'"
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

$release = ConvertTo-BthPS3ReleaseVersion -Ref 'refs/tags/v2.12.0' -RunNumber 12 -BuildVersionOffset 2000
Assert-Equal $release.IsRelease $true 'tag is release'
Assert-Equal $release.Tag 'v2.12.0' 'tag name'
Assert-Equal $release.SetupVersion '2.12.0' 'setup version'
Assert-Equal $release.DriverVersion '2.12.0.2012' 'driver version uses offset plus run number'
Assert-Equal $release.Revision 2012 'revision'

$bare = ConvertTo-BthPS3ReleaseVersion -Ref 'v2.12.0' -RunNumber 1 -BuildVersionOffset 2000
Assert-Equal $bare.DriverVersion '2.12.0.2001' 'bare tag ref'

$resumed = ConvertTo-BthPS3ReleaseVersion -Ref 'refs/tags/v2.11.0' -RunNumber 23 -BuildVersionOffset 2000
Assert-Equal $resumed.IsRelease $true 'resumed tag is release'
Assert-Equal $resumed.SetupVersion '2.11.0' 'resumed setup version'
Assert-Equal $resumed.DriverVersion '2.11.0.2023' 'resumed driver version uses source run number'

$ci = ConvertTo-BthPS3ReleaseVersion -Ref 'refs/heads/master' -RunNumber 12 -BuildVersionOffset 2000
Assert-Equal $ci.IsRelease $false 'master is not a release'
Assert-Equal $ci.SetupVersion '' 'master has no setup version'
Assert-Equal $ci.DriverVersion '0.0.0.2012' 'master uses CI-only version'

Assert-Throws { ConvertTo-BthPS3ReleaseVersion -Ref 'refs/tags/v2.12.0.1' -RunNumber 1 -BuildVersionOffset 2000 } 'four-part tag rejected'
Assert-Throws { ConvertTo-BthPS3ReleaseVersion -Ref 'refs/tags/v2.12.0-pre1' -RunNumber 1 -BuildVersionOffset 2000 } 'prerelease tag rejected'
Assert-Throws { ConvertTo-BthPS3ReleaseVersion -Ref 'refs/tags/setup-v2.12.0' -RunNumber 1 -BuildVersionOffset 2000 } 'setup tag rejected'
Assert-Throws { ConvertTo-BthPS3ReleaseVersion -Ref 'refs/tags/v2.12.0' -RunNumber 0 -BuildVersionOffset 2000 } 'run number must be positive'
Assert-Throws { ConvertTo-BthPS3ReleaseVersion -Ref 'refs/tags/v2.12.0' -RunNumber 1 -BuildVersionOffset -1 } 'negative offset rejected'
Assert-Throws { ConvertTo-BthPS3ReleaseVersion -Ref 'refs/tags/v2.12.0' -RunNumber 1 -BuildVersionOffset 70000 } 'offset range rejected'
Assert-Throws { ConvertTo-BthPS3ReleaseVersion -Ref 'refs/tags/v2.12.0' -RunNumber 50000 -BuildVersionOffset 20000 } 'revision overflow rejected'

$output = Join-Path ([IO.Path]::GetTempPath()) ("bthps3-version-" + [guid]::NewGuid().ToString('N') + ".txt")
try {
    $null = Write-BthPS3ReleaseVersionOutputs -Version $release -GitHubOutput $output
    $text = [IO.File]::ReadAllText($output)
    if ($text -notmatch 'is-release=true') { throw 'FAIL github output is-release' }
    if ($text -notmatch 'driver-version=2.12.0.2012') { throw 'FAIL github output driver-version' }
    if ($text -notmatch 'setup-version=2.12.0') { throw 'FAIL github output setup-version' }
    Write-Output 'PASS github output file'
}
finally {
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Force }
}

Write-Output 'ReleaseVersion tests passed'
