<#
.SYNOPSIS
Runs TC-0035-cases-numeric through CTest.
.DESCRIPTION
Specification: tests/tc-0035-cases-numeric.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0035' -Config $Config
exit $LASTEXITCODE
