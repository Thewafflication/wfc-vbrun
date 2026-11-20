<#
.SYNOPSIS
Runs the WSP traceability check for wfc-vbrun.
.DESCRIPTION
Requirements are requirements/req-NNNN-*.md, the test specifications are
tests/tc-NNNN-*.tex and each test case has the runner tests/run-tc-NNNN.ps1.
The check fails on a requirement without a test case, a test case without a
requirement, a missing specification or runner, or a duplicate identifier.
.EXAMPLE
pwsh -NoProfile -File scripts/Test-Traceability.ps1
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$tool = Join-Path $root 'wsp/tools/Test-Traceability.ps1'
# A child process gives a reliable exit status.
& pwsh -NoProfile -File $tool -RepositoryRoot $root `
    -RequirementsPath requirements `
    -TestSpecificationsPath tests `
    -TestImplementationsPath tests
if ($LASTEXITCODE -ne 0) {
    throw "Traceability check failed ($LASTEXITCODE)."
}
Write-Output '[PASS] traceability'
