<#
.SYNOPSIS
Runs TC-0029-cases-varstrings through CTest.
.DESCRIPTION
Specification: tests/tc-0029-cases-varstrings.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0029' -Config $Config
exit $LASTEXITCODE
