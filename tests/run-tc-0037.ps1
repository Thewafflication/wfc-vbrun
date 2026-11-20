<#
.SYNOPSIS
Runs TC-0037-cases-dates through CTest.
.DESCRIPTION
Specification: tests/tc-0037-cases-dates.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0037' -Config $Config
exit $LASTEXITCODE
