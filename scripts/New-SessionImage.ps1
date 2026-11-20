<#
.SYNOPSIS
Renders the real x64 ctest session of the library as a terminal-style PNG.
.DESCRIPTION
Runs the registered tests of the build that scripts/Invoke-Build.ps1 made
(the same ctest call as scripts/Invoke-Tests.ps1) and draws its result lines
as a card in the WFC house style with scripts/render_card.py. (The Linux
images come from CI and scripts/render_terminal.py, which every WFC library
keeps identical.) Every test name, result and time on the card is a line ctest
printed.
.PARAMETER Config
Build configuration, Debug by default.
.PARAMETER Output
The PNG to write, docs/images/windows-x64.png by default.
.EXAMPLE
pwsh -NoProfile -File scripts/New-SessionImage.ps1
#>
[CmdletBinding()]
param(
    [string]$Config = 'Debug',
    [string]$Output = 'docs/images/windows-x64.png'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $session = Join-Path $root 'out/session.txt'
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add("`$ ctest --test-dir out/build -C $Config")
    $text = & ctest --test-dir out/build -C $Config 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ctest failed ($LASTEXITCODE); no image written."
    }
    foreach ($line in $text) {
        $line = "$line"
        if ($line -match '^\s+Start\s+\d+:') { continue }
        if ($line -match '^Test project ') { $line = 'Test project out/build' }
        $lines.Add($line)
    }
    [IO.File]::WriteAllLines($session, $lines)
    $title = "wfc-vbrun test results"
    $subtitle = "Windows x64 $Config - ctest of the standalone build"
    & python scripts/render_card.py $session $Output $title $subtitle
    if ($LASTEXITCODE -ne 0) {
        throw "render_card.py failed ($LASTEXITCODE)."
    }
    Write-Output "[PASS] wrote $Output"
}
finally {
    Pop-Location
}
