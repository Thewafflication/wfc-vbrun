<#
.SYNOPSIS
Runs TC-0014-x87math through CTest.
.DESCRIPTION
Specification: tests/tc-0014-x87math.tex.
Runs through Invoke-TestCase.ps1.
.PARAMETER Config
Build configuration, Debug by default.
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug'
)

$testCase = Join-Path $PSScriptRoot 'Invoke-TestCase.ps1'
& $testCase -Id 'TC-0014' -Config $Config
exit $LASTEXITCODE
