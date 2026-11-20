<#
.SYNOPSIS
First step of the commit gate: allows one gate at a time and shows progress.
.DESCRIPTION
Two commits that run their gates at once corrupt each other's build and test
runs. This step takes wfc-commit-gate.lock in the temp folder, which names
the git process that
owns the gate; a second commit while that process is alive is refused. A lock
whose owner has gone is taken over. It then opens the test progress window
(scripts/Show-TestProgress.ps1), which keeps to one window by itself.
.EXAMPLE
git commit
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# One lock for the machine: gates of different checkouts load the machine
# just as much, and slow tests time out.
$lock = Join-Path ([IO.Path]::GetTempPath()) 'wfc-commit-gate.lock'

# The git process that started this hook is the owner of the gate.
function Get-OwnerId {
    $id = $PID
    for ($depth = 0; $depth -lt 12; $depth++) {
        $process = Get-CimInstance Win32_Process -Filter "ProcessId=$id"
        if (-not $process) { break }
        if ($process.Name -eq 'git.exe') { return [int]$process.ProcessId }
        $id = [int]$process.ParentProcessId
        if ($id -le 0) { break }
    }
    return $PID
}

$owner = Get-OwnerId
if (Test-Path $lock) {
    $held = 0
    [void][int]::TryParse((Get-Content -Raw $lock).Trim(), [ref]$held)
    if ($held -gt 0 -and $held -ne $owner -and
        (Get-Process -Id $held -ErrorAction SilentlyContinue)) {
        throw "Another commit gate is running (git process $held). " +
        'Wait for it to finish; do not start a second commit.'
    }
}
Set-Content -Path $lock -Value $owner -Encoding ascii

# Started through the shell: a window that inherited the hook's output pipe
# would keep the hook (and the commit) waiting until it was closed.
$info = New-Object Diagnostics.ProcessStartInfo
$info.FileName = 'pwsh'
$info.Arguments = '-NoProfile -File "' +
    (Join-Path $PSScriptRoot 'Show-TestProgress.ps1') + '"'
$info.UseShellExecute = $true
$info.WindowStyle = 'Hidden'
[void][Diagnostics.Process]::Start($info)
Write-Output "[PASS] commit gate started (git process $owner)"
