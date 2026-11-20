<#
.SYNOPSIS
Runs the wfc-vbrun canonical full test stage.
.DESCRIPTION
Runs every registered CTest test of the standalone build, including negative
tests. Zero registered tests is an error. The library must already be built
(scripts/Invoke-Build.ps1). Every line ctest prints is also written to
out/test-progress.log, which scripts/Show-TestProgress.ps1 follows in the
always-on-top test progress window.
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
    # The log is followed live by scripts/Show-TestProgress.ps1.
    $progressLog = Join-Path $root 'out/test-progress.log'
    New-Item -ItemType Directory -Force -Path (Split-Path $progressLog) |
        Out-Null
    $arguments = @('--test-dir', 'out/build', '-C', $Config,
        '--output-on-failure', '--no-tests=error')
    if ($JUnitPath) {
        $arguments += @('--output-junit', $JUnitPath)
    }
    Write-Stage "[tests] ctest $Config"
    # Each line goes to the log as soon as ctest prints it, in the form
    # `12/45 Test  #12: TC-0012-regcall ... Passed` the window parses.
    [IO.File]::WriteAllText($progressLog, '')
    & ctest @arguments 2>&1 | ForEach-Object {
        $line = "$_"
        [IO.File]::AppendAllText($progressLog, $line + "`n")
        $line
    }
    if ($LASTEXITCODE -ne 0) {
        throw "ctest $($arguments -join ' ') failed ($LASTEXITCODE)."
    }
    Write-Output "[PASS] tests $Config"
}
finally {
    Pop-Location
}
