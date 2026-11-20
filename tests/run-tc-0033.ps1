<#
.SYNOPSIS
Runs TC-0033-cases-random through CTest.
.DESCRIPTION
Specification: tests/tc-0033-cases-random.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0033' -Config $Config
exit $LASTEXITCODE
