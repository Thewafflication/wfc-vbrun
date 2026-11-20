<#
.SYNOPSIS
Small always-on-top window that follows a ctest log and shows progress.

.DESCRIPTION
Follows the log that scripts/Invoke-Tests.ps1 writes (lines such as
`12/377 Test  #12: name ... Passed`) and shows a progress bar, the pass and
fail counts and the log itself, scrolling as lines arrive. Failed tests are
red. Start it before a run, or run the tests through it:

  pwsh scripts/Show-TestProgress.ps1 -Run

The window closes itself 8 seconds after a run in which every test passed; it
stays open when a test failed. To start it without a console window, use
Process.Start with CreateNoWindow (not -WindowStyle Hidden, which can hide the
window as well).

.PARAMETER Log
The log file to follow (default: out/test-progress.log).

.PARAMETER Run
Run ctest for the x64 debug build into the log and follow it.

.PARAMETER Filter
A ctest -R expression when -Run is given.
#>
[CmdletBinding()]
param(
    [string]$Log = (Join-Path $PSScriptRoot '..\out\test-progress.log'),
    [switch]$Run,
    [string]$Filter = '',
    [string]$BuildDir = (Join-Path $PSScriptRoot `
        '..\out\build\windows-x64-debug')
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$Log = [IO.Path]::GetFullPath($Log)
# One window per log: a second start finds the first and leaves.
$hash = [BitConverter]::ToString(
    [Security.Cryptography.SHA1]::Create().ComputeHash(
        [Text.Encoding]::UTF8.GetBytes($Log.ToLowerInvariant()))
).Replace('-', '')
$created = $false
$single = New-Object Threading.Mutex($true, "Local\WfcTestProgress$hash",
    [ref]$created)
if (-not $created) { exit 0 }
New-Item -ItemType Directory -Force -Path (Split-Path $Log) | Out-Null
$process = $null
if ($Run) {
    Set-Content -Path $Log -Value '' -Encoding ascii
    $arguments = @('--test-dir', $BuildDir, '-C', 'Debug',
        '--output-on-failure')
    if ($Filter) { $arguments += @('-R', $Filter) }
    $process = Start-Process ctest -ArgumentList $arguments -NoNewWindow `
        -PassThru `
        -RedirectStandardOutput $Log -RedirectStandardError "$Log.err"
}
elseif (-not (Test-Path $Log)) {
    Set-Content -Path $Log -Value '' -Encoding ascii
}

$form = New-Object Windows.Forms.Form
$form.Text = 'WFC tests'
$form.TopMost = $true
$form.Size = New-Object Drawing.Size(520, 440)
$form.StartPosition = 'Manual'
$area = [Windows.Forms.Screen]::PrimaryScreen.WorkingArea
$form.Location = New-Object Drawing.Point(($area.Right - 530), ($area.Top + 10))
# A normal window: a tool window is left out of Alt+Tab and the taskbar.
$form.FormBorderStyle = 'Sizable'
$form.ShowInTaskbar = $true

$status = New-Object Windows.Forms.Label
$status.Dock = 'Top'
$status.Height = 22
$status.Text = 'waiting for tests...'
$bar = New-Object Windows.Forms.ProgressBar
$bar.Dock = 'Top'
$bar.Height = 20
$bar.Minimum = 0
$bar.Maximum = 100
# Pinned: the tests that have started and not yet finished (ctest runs
# several at once, so results arrive in the order they finish).
$running = New-Object Windows.Forms.ListBox
$running.Dock = 'Top'
$running.Height = 104
$running.IntegralHeight = $false
$running.SelectionMode = 'None'
$running.BackColor = [Drawing.Color]::LightYellow
$running.Font = New-Object Drawing.Font('Consolas', 8)
$box = New-Object Windows.Forms.RichTextBox
$box.ReadOnly = $true
$box.WordWrap = $false
$box.ScrollBars = 'Both'
$box.Dock = 'Fill'
$box.BackColor = [Drawing.Color]::White
$box.Font = New-Object Drawing.Font('Consolas', 8)
$form.Controls.Add($box)
$form.Controls.Add($running)
$form.Controls.Add($bar)
$form.Controls.Add($status)

# What has been read so far; the log is read as raw bytes so a line that is
# only half written waits for its end.
$script:offset = 0L
$script:pending = ''
$script:done = 0
$script:total = 0
$script:failed = 0
$script:active = @{}

function Add-Line([string]$line) {
    if ($line -match '^\[(lint|build)\]') { $status.Text = $line }
    # The result is the word after the dots; a test name can contain
    # "failed" or "error" and still pass.
    $result = ''
    if ($line -match '^\s*Start\s+(\d+):\s*(\S+)') {
        $script:active[[int]$Matches[1]] = $Matches[2]
        return  # shown in the pinned list, not in the log
    }
    if ($line -match '^\s*\d+/\d+ Test\s+#(\d+):') {
        $script:active.Remove([int]$Matches[1])
    }
    if ($line -match '^\s*(\d+)/(\d+) Test\s+#\d+:.*\.\s+(\*\*\*\S+|Passed)') {
        $script:done = [int]$Matches[1]
        $script:total = [int]$Matches[2]
        $result = $Matches[3]
        if ($result -ne 'Passed') { $script:failed++ }
    }
    $box.SelectionStart = $box.TextLength
    $box.SelectionLength = 0
    $box.SelectionColor = if ($result -eq 'Passed') {
        [Drawing.Color]::DarkGreen
    } elseif ($result -ne '' -or $line -match '^The following tests FAILED') {
        [Drawing.Color]::Firebrick
    } else {
        [Drawing.Color]::Black
    }
    $box.AppendText($line + "`n")
}

$timer = New-Object Windows.Forms.Timer
$timer.Interval = 250
$timer.Add_Tick({
    $stream = $null
    try {
        $stream = [IO.File]::Open($Log, 'Open', 'Read', 'ReadWrite')
        if ($stream.Length -lt $script:offset) {
            # A new run truncated the log: start over.
            $script:offset = 0
            $script:pending = ''
            $box.Clear()
            $script:done = 0; $script:total = 0; $script:failed = 0
            $script:active = @{}
            $bar.Value = 0
        }
        [void]$stream.Seek($script:offset, 'Begin')
        $buffer = New-Object byte[] ($stream.Length - $script:offset)
        $count = $stream.Read($buffer, 0, $buffer.Length)
        $script:offset += $count
        $chunk = $script:pending +
            [Text.Encoding]::UTF8.GetString($buffer, 0, $count)
    }
    catch { return }
    finally { if ($stream) { $stream.Dispose() } }
    if (-not $chunk) { return }
    # What was in the log when the window opened belongs to an earlier run.
    $old = -not $script:started
    $script:started = $true
    if ($old) { $script:seenOld = $false } else { $script:seenOld = $true }
    $lines = $chunk -split "`r?`n"
    $script:pending = $lines[-1]
    $box.SuspendLayout()
    for ($i = 0; $i -lt $lines.Count - 1; $i++) { Add-Line $lines[$i] }
    if ($box.Lines.Count -gt 4000) {
        $box.Select(0, $box.GetFirstCharIndexFromLine(1000))
        $box.SelectedText = ''
    }
    $running.BeginUpdate()
    $running.Items.Clear()
    foreach ($id in ($script:active.Keys | Sort-Object)) {
        [void]$running.Items.Add(('#{0,-4} {1}' -f $id, $script:active[$id]))
    }
    $running.EndUpdate()
    $box.SelectionStart = $box.TextLength
    $box.ScrollToCaret()
    $box.ResumeLayout()
    if ($script:total -gt 0) {
        $bar.Value = [Math]::Min(100,
            [int](100 * $script:done / $script:total))
        $status.Text = "$($script:done) / $($script:total) tests, " +
            "$($script:failed) failed"
    }
    if ($script:seenOld -and $chunk -match 'Total Test time') {
        $status.Text += ' - finished'
        # Run on its own (-Run) a clean run closes the window.
        if ($Run -and $script:failed -eq 0 -and
            $script:done -eq $script:total) {
            $close.Start()
        }
        # A commit routine that never writes its marker (a manual push)
        # would leave the window open for ever: after a clean gate, close
        # it a minute later unless the routine reports a failure.
        elseif ($script:failed -eq 0 -and $script:done -eq $script:total) {
            $fallback.Start()
        }
    }
    if ($chunk -match '(?m)^\[commit\] failed') {
        $fallback.Stop()
        $status.Text += ' - failed'
    }
    # The commit routine ends its log with "[commit] pushed" or
    # "[commit] failed": the window stays open for the whole gate and
    # closes only after a push; a failure keeps it open.
    if ($script:seenOld -and $chunk -match '(?m)^\[commit\] pushed') {
        $status.Text += ' - pushed'
        $close.Start()
    }
})
$close = New-Object Windows.Forms.Timer
$close.Interval = 8000
$close.Add_Tick({ $close.Stop(); $form.Close() })
$fallback = New-Object Windows.Forms.Timer
$fallback.Interval = 20000
$fallback.Add_Tick({ $fallback.Stop(); $form.Close() })
$idle = New-Object Windows.Forms.Timer
$idle.Interval = 10000
$idle.Add_Tick({
    $quiet = ((Get-Date) - (Get-Item $Log).LastWriteTime).TotalSeconds
    $finished = $script:total -gt 0 -or
        (Select-String -Path $Log -Pattern 'Total Test time|\[commit\]' `
            -Quiet)
    # Finished (passed or failed): five minutes to read it. Otherwise a log
    # that has not moved for half an hour belongs to a run that is gone.
    if (($finished -and $quiet -gt 300) -or $quiet -gt 1800) { $form.Close() }
})
$timer.Start()
$idle.Start()
$form.Add_FormClosed({ $timer.Stop(); $close.Stop(); $fallback.Stop() })
Add-Type -Name Native -Namespace Wfc -MemberDefinition @'
[DllImport("user32.dll")]
public static extern bool ShowWindow(IntPtr window, int command);
'@
$form.Add_Shown({
    # The first show follows the hidden start of the process.
    [void][Wfc.Native]::ShowWindow($form.Handle, 5)
    $form.Activate()
})
[void]$form.ShowDialog()
if ($process -and -not $process.HasExited) { $process.Kill() }
$single.ReleaseMutex()
