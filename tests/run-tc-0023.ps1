<#
.SYNOPSIS
Runs TC-0023-cases-strings through CTest.
.DESCRIPTION
Specification: tests/tc-0023-cases-strings.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0023' -Config $Config
exit $LASTEXITCODE
