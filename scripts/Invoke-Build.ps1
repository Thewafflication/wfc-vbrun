<#
.SYNOPSIS
Runs the wfc-vbrun canonical build stage.
.DESCRIPTION
Configures and builds the standalone library with the Visual Studio
generator. The local commit gate uses x64 Debug; CI builds the other
architectures.
.PARAMETER Platform
Visual Studio platform: x64 (default), Win32 or ARM64.
.PARAMETER Config
Build configuration, Debug by default.
.EXAMPLE
pwsh -NoProfile -File scripts/Invoke-Build.ps1
#>
[CmdletBinding()]
param(
    [ValidateSet('x64', 'Win32', 'ARM64')]
    [string]$Platform = 'x64',
    [string]$Config = 'Debug'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'WfcChecks.psm1') -Force

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    Write-Stage "[build] configure $Platform"
    Invoke-CheckedCommand cmake @('-S', '.', '-B', 'out/build', '-A',
        $Platform)
    Write-Stage "[build] compile $Platform $Config"
    Invoke-CheckedCommand cmake @('--build', 'out/build', '--config',
        $Config)
    Write-Output "[PASS] build $Platform $Config"
}
finally {
    Pop-Location
}
