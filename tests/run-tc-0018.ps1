<#
.SYNOPSIS
Runs TC-0018-owner through CTest.
.DESCRIPTION
Specification: tests/tc-0018-owner.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0018' -Config $Config
exit $LASTEXITCODE
