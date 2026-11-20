<#
.SYNOPSIS
Runs TC-0026-cases-varops through CTest.
.DESCRIPTION
Specification: tests/tc-0026-cases-varops.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0026' -Config $Config
exit $LASTEXITCODE
