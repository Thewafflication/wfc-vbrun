<#
.SYNOPSIS
Runs TC-0024-cases-variants through CTest.
.DESCRIPTION
Specification: tests/tc-0024-cases-variants.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0024' -Config $Config
exit $LASTEXITCODE
