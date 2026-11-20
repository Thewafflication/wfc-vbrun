<#
.SYNOPSIS
Runs the wfc-vbrun canonical lint stage.
.DESCRIPTION
Checks the owned source and configuration scope:
- WSP physical style (80 columns, UTF-8, whitespace, final newline);
- clang-format check mode for C++ (WSP-LANG-0002);
- gersemi check mode for CMake (WSP-LANG-0010);
- PowerShell syntax and PSScriptAnalyzer (WSP-LANG-0007);
- YAML syntax (WSP-LANG-0005);
- requirements to test cases to runners (scripts/Test-Traceability.ps1);
- pre-commit configuration validity (WSP-CHECK-0001).
Stops at the first failing check and exits nonzero. Changes no files.
.EXAMPLE
pwsh -NoProfile -File scripts/Invoke-Lint.ps1
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'WfcChecks.psm1') -Force

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    Write-Stage '[lint] WSP physical source style' -Reset
    $styleScope = @(
        'src', 'tests', 'cmake', 'scripts', '.github', 'CMakeLists.txt',
        '.clang-format', '.editorconfig', '.gersemirc', '.gitattributes',
        '.pre-commit-config.yaml', 'PSScriptAnalyzerSettings.psd1'
    )
    # The first two scripts are byte-identical in every WFC library (a WFC
    # test compares the copies); they are wrapped when all of them change
    # together. The third holds Visual Basic 6 source in Python strings,
    # which wrapping would change.
    $sharedCopies = "'scripts/capture_web.py'," +
        "'scripts/render_terminal.py'," +
        "'scripts/vb6_harness_gen.py'"
    # A child process gives a reliable exit status; -Command keeps the
    # array parameter intact.
    $quoted = ($styleScope | ForEach-Object { "'$_'" }) -join ','
    Invoke-CheckedCommand pwsh @('-NoProfile', '-Command',
        "& ./wsp/tools/Test-SourceStyle.ps1 -RepositoryRoot '$root' " +
        "-SourcePath @($quoted) -ExcludePath @($sharedCopies)")

    Write-Stage '[lint] clang-format (C++)'
    $clangFormat = Resolve-PinnedTool 'clang-format' '22.1.3'
    $cxx = Get-OwnedFile @('src', 'tests') @('.cpp', '.hpp', '.c', '.h')
    Invoke-CheckedCommand $clangFormat (
        @('--dry-run', '--Werror', '--style=file') + $cxx)

    Write-Stage '[lint] gersemi (CMake)'
    $gersemi = Resolve-PinnedTool 'gersemi' '0.29.2'
    $cmake = @('CMakeLists.txt') +
        (Get-OwnedFile @('cmake', 'tests') @('.cmake'))
    Invoke-CheckedCommand $gersemi (@(
            '--check', '--warnings-as-errors',
            '--definitions', 'CMakeLists.txt', 'cmake', 'tests',
            'wsp/tools/cmake',
            '--') + $cmake)

    Write-Stage '[lint] PowerShell syntax and analysis'
    & ./wsp/tools/Test-RepositorySyntax.ps1
    Import-Module PSScriptAnalyzer -RequiredVersion 1.25.0
    $findings = @(Invoke-ScriptAnalyzer -Path scripts -Recurse `
            -Settings PSScriptAnalyzerSettings.psd1)
    if ($findings.Count -gt 0) {
        $findings | Format-Table ScriptName, Line, RuleName, Message -Wrap |
            Out-String | Write-Output
        throw 'PowerShell analysis failed; see findings above.'
    }

    Write-Stage '[lint] YAML syntax'
    $yaml = Get-OwnedFile @('.github', '.pre-commit-config.yaml') `
        @('.yml', '.yaml')
    $parseYaml = 'import sys, yaml; [yaml.safe_load(open(p, ' +
        "encoding='utf-8')) for p in sys.argv[1:]]"
    Invoke-CheckedCommand python (@('-c', $parseYaml) + $yaml)

    Write-Stage '[lint] requirements to test cases (traceability)'
    Invoke-CheckedCommand pwsh @('-NoProfile', '-File',
        'scripts/Test-Traceability.ps1')

    Write-Stage '[lint] pre-commit configuration'
    $preCommit = Resolve-PinnedTool 'pre-commit' '4.6.2'
    Invoke-CheckedCommand $preCommit @('validate-config')

    Write-Output '[PASS] lint'
}
finally {
    Pop-Location
}
