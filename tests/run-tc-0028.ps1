<#
.SYNOPSIS
Runs TC-0028-cases-varfuncs through CTest.
.DESCRIPTION
Specification: tests/tc-0028-cases-varfuncs.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0028' -Config $Config
exit $LASTEXITCODE
