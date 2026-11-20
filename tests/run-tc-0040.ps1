<#
.SYNOPSIS
Runs TC-0040-cases-arrays through CTest.
.DESCRIPTION
Specification: tests/tc-0040-cases-arrays.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0040' -Config $Config
exit $LASTEXITCODE
