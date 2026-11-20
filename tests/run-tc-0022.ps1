<#
.SYNOPSIS
Runs TC-0022-filemisc through CTest.
.DESCRIPTION
Specification: tests/tc-0022-filemisc.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0022' -Config $Config
exit $LASTEXITCODE
