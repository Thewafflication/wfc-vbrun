<#
.SYNOPSIS
Runs TC-0021-midstmt through CTest.
.DESCRIPTION
Specification: tests/tc-0021-midstmt.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0021' -Config $Config
exit $LASTEXITCODE
