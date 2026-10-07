[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# The fixture implementation uses global command doubles. A fresh process keeps
# those globals and module auto-loading out of the caller's interactive session,
# including when fixture setup or cleanup throws. Keep the same PowerShell edition.
$shellName = 'pwsh.exe'
if ($PSVersionTable.PSEdition -eq 'Desktop') { $shellName = 'powershell.exe' }
$shellPath = Join-Path $PSHOME $shellName
$fixturePath = Join-Path $PSScriptRoot 'Invoke-PackagingFixtureTests.ps1'
foreach ($required in @($shellPath, $fixturePath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Packaging test dependency is missing: $required" }
}

# Do not change execution policy, load profiles, or run an installed application.
& $shellPath -NoProfile -NonInteractive -File $fixturePath
$fixtureExitCode = $LASTEXITCODE
if ($fixtureExitCode -ne 0) {
    throw "Packaging fixture process failed with exit code $fixtureExitCode. See the preceding fixture output."
}
