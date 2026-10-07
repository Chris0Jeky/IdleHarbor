[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# These scripts use terminating assertions, not Pester discovery or an installed
# module version. Keep every noninteractive script regression registered here.
foreach ($suite in @(
    'Test-InputMonitor.Tests.ps1',
    'Test-ReleaseVersion.Tests.ps1',
    'Test-NewReleasePackage.Tests.ps1',
    'Test-ChocolateyShutdown.ps1',
    'Test-PerUserShutdown.ps1',
    'Test-ReleaseLicense.ps1',
    'Test-ChocolateyReleaseLag.ps1',
    'Test-PackagingPreview.ps1'
)) {
    $path = Join-Path $PSScriptRoot $suite
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing registered regression suite: $suite" }
    Write-Host "Running $suite with PowerShell $($PSVersionTable.PSVersion)."
    & $path
}
Write-Host 'All registered script regressions passed.'
