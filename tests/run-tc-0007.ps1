<#
.SYNOPSIS
Runs TC-0007-late through CTest.
.DESCRIPTION
Specification: tests/tc-0007-late.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0007' -Config $Config
exit $LASTEXITCODE
