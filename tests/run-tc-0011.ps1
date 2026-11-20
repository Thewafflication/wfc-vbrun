<#
.SYNOPSIS
Runs TC-0011-errobj through CTest.
.DESCRIPTION
Specification: tests/tc-0011-errobj.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0011' -Config $Config
exit $LASTEXITCODE
