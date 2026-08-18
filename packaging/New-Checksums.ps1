[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$InputDirectory,

    [string]$OutputPath = (Join-Path $InputDirectory 'SHA256SUMS.txt')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path -LiteralPath $InputDirectory).Path
$output = [IO.Path]::GetFullPath($OutputPath)
$entries = @(Get-ChildItem -LiteralPath $root -File -Recurse | Where-Object { [IO.Path]::GetFullPath($_.FullName) -ine $output } | Sort-Object FullName)
if ($entries.Count -eq 0) { throw "No files found below $root." }

$lines = foreach ($entry in $entries) {
    $relative = [IO.Path]::GetRelativePath($root, $entry.FullName).Replace('\', '/')
    $hash = (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "${hash} *${relative}"
}

$parent = Split-Path -Parent $output
New-Item -ItemType Directory -Path $parent -Force | Out-Null
$lines | Set-Content -LiteralPath $output -Encoding ASCII
Write-Output $output
