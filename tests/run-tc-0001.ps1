<#
.SYNOPSIS
Runs TC-0001-library through CTest.
.DESCRIPTION
Specification: tests/tc-0001-library.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0001' -Config $Config -RequiresEvidence
exit $LASTEXITCODE
