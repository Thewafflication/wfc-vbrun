<#
.SYNOPSIS
Runs TC-0006-systemmisc through CTest.
.DESCRIPTION
Specification: tests/tc-0006-systemmisc.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0006' -Config $Config
exit $LASTEXITCODE
