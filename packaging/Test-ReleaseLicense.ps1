[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$licensePath = Join-Path $root 'LICENSE'
if (-not (Test-Path -LiteralPath $licensePath -PathType Leaf)) {
    throw 'Publication requires a tracked root LICENSE file; none was found.'
}

$tracked = @(git -C $root ls-files --error-unmatch -- LICENSE 2>$null)
if ($LASTEXITCODE -ne 0 -or $tracked.Count -ne 1 -or $tracked[0] -cne 'LICENSE') {
    throw 'Publication requires LICENSE to be tracked at the repository root.'
}

$licenseText = Get-Content -Raw -LiteralPath $licensePath
if ($licenseText -notmatch '(?m)^\s*GNU GENERAL PUBLIC LICENSE\s*$' -or
    $licenseText -notmatch '(?m)^\s*Version 3, 29 June 2007\s*$' -or
    $licenseText -notmatch 'Everyone is permitted to copy and distribute verbatim copies') {
    throw 'Publication requires the complete GNU General Public License version 3 text.'
}

Write-Output "Tracked GPL-3.0-only root LICENSE validated: $licensePath"
