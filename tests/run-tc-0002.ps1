<#
.SYNOPSIS
Runs TC-0002-helpers-memory through CTest.
.DESCRIPTION
Specification: tests/tc-0002-helpers-memory.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0002' -Config $Config
exit $LASTEXITCODE
