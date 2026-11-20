<#
.SYNOPSIS
Runs TC-0043-cases-files through CTest.
.DESCRIPTION
Specification: tests/tc-0043-cases-files.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0043' -Config $Config
exit $LASTEXITCODE
