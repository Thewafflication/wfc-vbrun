<#
.SYNOPSIS
Runs the wfc-vbrun canonical full test stage.
.DESCRIPTION
Runs every registered CTest test of the standalone build, including negative
tests. Zero registered tests is an error. The library must already be built
(scripts/Invoke-Build.ps1).
.PARAMETER Config
Build configuration to test, Debug by default.
.PARAMETER JUnitPath
Optional path for a JUnit XML result file (used by CI evidence).
.EXAMPLE
pwsh -NoProfile -File scripts/Invoke-Tests.ps1
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug',
    [string]$JUnitPath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'WfcChecks.psm1') -Force

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $arguments = @('--test-dir', 'out/build', '-C', $Config,
        '--output-on-failure', '--no-tests=error')
    if ($JUnitPath) {
        $arguments += @('--output-junit', $JUnitPath)
    }
    Write-Stage "[tests] ctest $Config"
    Invoke-CheckedCommand ctest $arguments
    Write-Output "[PASS] tests $Config"
}
finally {
    Pop-Location
}
