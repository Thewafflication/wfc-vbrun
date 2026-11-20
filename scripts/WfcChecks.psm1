<#
.SYNOPSIS
Shared helpers for the wfc-vbrun canonical lint, build, and test scripts.
.DESCRIPTION
Provides native-command execution with exit-code propagation
(WSP-CHECK-0004) and pinned tool resolution (WSP-CHECK-0007). Every
helper throws on failure; callers do not inspect $LASTEXITCODE.
#>

Set-StrictMode -Version Latest

<#
.SYNOPSIS
Runs a native command and throws when it exits with a nonzero status.
.PARAMETER Command
Executable name or path.
.PARAMETER Arguments
Arguments passed unchanged to the command.
.OUTPUTS
None. The command's own output is written to the host.
#>
function Invoke-CheckedCommand {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Command,
        [string[]]$Arguments = @()
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command $($Arguments -join ' ') failed ($LASTEXITCODE)."
    }
}

<#
.SYNOPSIS
Resolves a required tool and verifies its pinned version.
.DESCRIPTION
Looks the tool up on PATH, then in the per-user Python scripts directory
where requirements-dev.txt installs it. Throws when the tool is missing or
its --version output does not contain the expected version, so a commit
gate never runs with an unpinned formatter.
.PARAMETER Name
Tool executable name without extension, for example clang-format.
.PARAMETER Version
Exact version text required in the tool's --version output.
.OUTPUTS
System.String. The full path of the verified executable.
#>
function Resolve-PinnedTool {
    [CmdletBinding()]
    [OutputType([string])]
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Version
    )

    $candidates = [Collections.Generic.List[string]]::new()
    $onPath = Get-Command $Name -CommandType Application `
        -ErrorAction SilentlyContinue
    foreach ($command in @($onPath)) {
        $candidates.Add($command.Source)
    }
    $userScripts = & python -c `
        'import sysconfig; print(sysconfig.get_path("scripts", "nt_user"))' `
        2>$null
    if ($LASTEXITCODE -eq 0 -and $userScripts) {
        $candidates.Add((Join-Path $userScripts "$Name.exe"))
    }
    foreach ($path in $candidates) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            continue
        }
        $reported = (& $path --version 2>&1 | Out-String).Trim()
        if ($reported -match [regex]::Escape($Version)) {
            return $path
        }
    }
    throw ("$Name $Version is required; install it with " +
        "'python -m pip install --requirement requirements-dev.txt'.")
}

<#
.SYNOPSIS
Lists tracked and untracked (not ignored) files under the given paths.
.PARAMETER Path
Repository-relative paths passed to git ls-files.
.PARAMETER Extension
Extensions to keep, including the leading dot.
.OUTPUTS
System.String[]. Repository-relative file paths. Throws when none match,
because an empty scan is an error (WSP-STYLE-0007).
#>
function Get-OwnedFile {
    [CmdletBinding()]
    [OutputType([string[]])]
    param(
        [Parameter(Mandatory)][string[]]$Path,
        [Parameter(Mandatory)][string[]]$Extension
    )

    $files = & git ls-files --cached --others --exclude-standard -- @Path
    if ($LASTEXITCODE -ne 0) {
        throw 'git ls-files failed.'
    }
    $selected = @($files | Where-Object {
            [IO.Path]::GetExtension($_) -in $Extension -and
            (Test-Path -LiteralPath $_ -PathType Leaf)
        })
    if ($selected.Count -eq 0) {
        throw ("No $($Extension -join '/') files found under " +
            "$($Path -join ', ').")
    }
    return $selected
}

<#
.SYNOPSIS
Writes a stage line to the output and to the test progress log.
.DESCRIPTION
scripts/Show-TestProgress.ps1 follows out/test-progress.log, so lint and
build stages show there before the tests start. -Reset starts a new log.
.PARAMETER Text
The stage line, for example "[lint] gersemi (CMake)".
.PARAMETER Reset
Empties the log first (the first stage of a commit gate run).
#>
function Write-Stage {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Text,
        [switch]$Reset
    )

    $log = Join-Path (Split-Path -Parent $PSScriptRoot) 'out/test-progress.log'
    New-Item -ItemType Directory -Force -Path (Split-Path $log) | Out-Null
    if ($Reset) {
        [IO.File]::WriteAllText($log, '')
    }
    [IO.File]::AppendAllText($log, $Text + "`n")
    Write-Output $Text
}

Export-ModuleMember -Function Invoke-CheckedCommand, Write-Stage,
    Resolve-PinnedTool, Get-OwnedFile
