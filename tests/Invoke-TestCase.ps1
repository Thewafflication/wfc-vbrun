<#
.SYNOPSIS
Runs the CTest test of one TC-NNNN test case.
.DESCRIPTION
The library names each CTest test TC-NNNN-<short-name>; this runs the one
whose number is given, from the build that scripts/Invoke-Build.ps1 made.
The per-case scripts tests/run-tc-NNNN.ps1 call it, which is the
implementation each test specification (tests/tc-NNNN-*.tex) is traced to by
wsp/tools/Test-Traceability.ps1.
.PARAMETER Id
The test case identifier, for example TC-0004.
.PARAMETER Config
Build configuration, Debug by default.
.PARAMETER RequiresEvidence
The test is registered only when the retained MSVBVM60 inventory exists (a
WFC checkout, or WFC_VBRUN_EVIDENCE names it); without it the run is skipped.
.EXAMPLE
pwsh -NoProfile -File tests/run-tc-0004.ps1
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^TC-\d{4}$')]
    [string]$Id,
    [string]$Config = 'Debug',
    [switch]$RequiresEvidence
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
if ($RequiresEvidence) {
    $evidence = $env:WFC_VBRUN_EVIDENCE
    if (-not $evidence) {
        $evidence = Join-Path $root `
            'evidence/reference/msvbvm60-6.0-runtime.json'
    }
    if (-not (Test-Path -LiteralPath $evidence)) {
        Write-Output "[SKIP] $Id needs the WFC evidence ($evidence)"
        exit 0
    }
}
Push-Location $root
try {
    & ctest --test-dir out/build -C $Config --output-on-failure `
        --no-tests=error -R "^$Id-"
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
