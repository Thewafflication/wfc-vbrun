<#
.SYNOPSIS
Runs TC-0038-cases-dates2 through CTest.
.DESCRIPTION
Specification: tests/tc-0038-cases-dates2.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0038' -Config $Config
exit $LASTEXITCODE
