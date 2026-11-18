<#
.SYNOPSIS
Builds the wfc-vbrun WPM package from the Release DLL of each architecture.

.DESCRIPTION
Reads <InputRoot>/<arch>/wfcvbrun.dll for x86, x64 and arm64 and writes
wfc-vbrun-any-<version>.zip, the version being the one in VERSION. The
package holds one folder per architecture, this README, the WFC license and
the WFC library install scripts (Install-WfcLibrary.ps1, WfcInstallCommon.ps1
from the WFC repository, given with -WfcRoot): WPM's install.cmd hands over to
them, and they copy the DLL to lib\wfcvbrun\<version>\<arch> of the WFC
installation and register it with Windows.

Nothing is signed unless -SigningKey is given (CI passes the key from the
protected release environment), and nothing is published.

.PARAMETER WfcRoot
A checkout of the WFC repository (its scripts and LICENSE).

.PARAMETER InputRoot
The folder with one subfolder per architecture.

.PARAMETER Output
Folder for the package.

.PARAMETER Wpm
Path of wpm.exe.

.PARAMETER SigningKey
Optional WPM private key file; the package is unsigned without it.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$WfcRoot,
    [string]$InputRoot = 'out/package-input',
    [string]$Output = 'out/packages',
    [string]$Wpm = 'wpm.exe',
    [string]$SigningKey
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$version = (Get-Content (Join-Path $root 'VERSION') -TotalCount 1).Trim()
$revision = (git -C $root rev-parse HEAD).Trim()
$WfcRoot = (Resolve-Path $WfcRoot).Path

$staging = Join-Path $root 'out/wpm-staging/wfc-vbrun-any'
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path $staging, $Output | Out-Null

foreach ($architecture in 'x86', 'x64', 'arm64') {
    $dll = Join-Path $InputRoot "$architecture/wfcvbrun.dll"
    if (-not (Test-Path $dll)) { throw "Missing $dll" }
    $target = Join-Path $staging $architecture
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Copy-Item $dll $target
}
Copy-Item (Join-Path $WfcRoot 'LICENSE') $staging
Copy-Item (Join-Path $root 'README.md') $staging
Copy-Item (Join-Path $WfcRoot 'scripts/Install-WfcLibrary.ps1'),
    (Join-Path $WfcRoot 'scripts/WfcInstallCommon.ps1') $staging

New-Item -ItemType Directory -Force -Path (Join-Path $staging '.wpm') |
    Out-Null
$script = '"%~dp0..\Install-WfcLibrary.ps1"'
Set-Content (Join-Path $staging '.wpm/install.cmd') -Encoding ascii -Value @(
    '@echo off'
    "powershell -NoProfile -ExecutionPolicy Bypass -File $script " +
        '-Source "%~dp0.." -Library wfcvbrun || exit /b 1'
    'exit /b 0'
)
Set-Content (Join-Path $staging '.wpm/remove.cmd') -Encoding ascii -Value @(
    '@echo off'
    "powershell -NoProfile -ExecutionPolicy Bypass -File $script " +
        '-Source "%~dp0.." -Library wfcvbrun -Uninstall || exit /b 1'
    'exit /b 0'
)
@(
    'name=wfc-vbrun'
    "version=$version"
    'arch=any'
    'debug=false'
    "description=WFC vbrun library $version (registered with Windows)"
    'maintainer=Jordan Waughtal'
    'homepage=https://github.com/Thewafflication/wfc-vbrun'
    'repository=https://github.com/Thewafflication/wfc-vbrun'
    'license=GPL-3.0-or-later'
    "source-version=$version"
    "source-revision=$revision"
) | Set-Content (Join-Path $staging '.wpm/package.txt') -Encoding ascii

$arguments = @('build', $staging, $Output)
if ($SigningKey) {
    $arguments += @('--sign', (Resolve-Path $SigningKey).Path)
}
& $Wpm @arguments
if ($LASTEXITCODE -ne 0) { throw 'wpm build failed for wfc-vbrun' }
$package = Join-Path $Output "wfc-vbrun-any-$version.zip"
if (-not (Test-Path $package)) { throw "Missing package: $package" }
$package
