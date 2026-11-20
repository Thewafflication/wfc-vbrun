<#
.SYNOPSIS
Runs TC-0027-cases-language through CTest.
.DESCRIPTION
Specification: tests/tc-0027-cases-language.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0027' -Config $Config
exit $LASTEXITCODE
