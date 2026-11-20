<#
.SYNOPSIS
Runs TC-0042-cases-system through CTest.
.DESCRIPTION
Specification: tests/tc-0042-cases-system.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0042' -Config $Config
exit $LASTEXITCODE
