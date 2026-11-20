<#
.SYNOPSIS
Runs TC-0025-cases-convert through CTest.
.DESCRIPTION
Specification: tests/tc-0025-cases-convert.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0025' -Config $Config
exit $LASTEXITCODE
