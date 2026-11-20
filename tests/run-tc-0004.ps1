<#
.SYNOPSIS
Runs TC-0004-openfiles through CTest.
.DESCRIPTION
Specification: tests/tc-0004-openfiles.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0004' -Config $Config
exit $LASTEXITCODE
