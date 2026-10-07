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

# Full GPLv3 plain-text reference, not a digest derived from the candidate file.
# Provenance and normalization are documented in packaging/LICENSE-INTEGRITY.md.
# Normalize CRLF only; preserve every other character and the final newline.
$expectedSha256 = '3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986'
$utf8 = New-Object Text.UTF8Encoding($false, $true)
$licenseText = [IO.File]::ReadAllText($licensePath, $utf8).Replace("`r`n", "`n")
$sha256 = [Security.Cryptography.SHA256]::Create()
try {
    $actualSha256 = [BitConverter]::ToString($sha256.ComputeHash($utf8.GetBytes($licenseText))).Replace('-', '').ToLowerInvariant()
}
finally { $sha256.Dispose() }
if ($actualSha256 -cne $expectedSha256) {
    throw "Publication requires the complete GNU General Public License version 3 text. Normalized SHA-256 was $actualSha256; expected $expectedSha256."
}

Write-Output "Tracked GPL-3.0-only root LICENSE validated: $licensePath"
