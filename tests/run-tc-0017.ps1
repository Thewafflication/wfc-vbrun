<#
.SYNOPSIS
Runs TC-0017-frames through CTest.
.DESCRIPTION
Specification: tests/tc-0017-frames.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0017' -Config $Config
exit $LASTEXITCODE
