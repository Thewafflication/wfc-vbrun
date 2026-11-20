<#
.SYNOPSIS
Runs TC-0030-cases-scalars through CTest.
.DESCRIPTION
Specification: tests/tc-0030-cases-scalars.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0030' -Config $Config
exit $LASTEXITCODE
