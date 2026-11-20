<#
.SYNOPSIS
Runs TC-0013-x87 through CTest.
.DESCRIPTION
Specification: tests/tc-0013-x87.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0013' -Config $Config
exit $LASTEXITCODE
