[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$BuildDirectory,

    [Parameter(Mandatory)]
    [string]$OutputDirectory,

    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$')]
    [string]$Version,

    [ValidateSet('x64', 'arm64', 'x86')]
    [string]$Architecture = 'x64',

    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),

    [string]$SourceRevision
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-FullPath([string]$Path) { return [IO.Path]::GetFullPath($Path) }

$buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
$repoRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$outputRoot = Get-FullPath $OutputDirectory
$binaryCandidates = @(Get-ChildItem -LiteralPath $buildRoot -Filter 'IdleHarbor.exe' -File -Recurse | Where-Object { $_.FullName -notmatch '\\CMakeFiles\\' })
if ($binaryCandidates.Count -ne 1) {
    throw "Expected exactly one built IdleHarbor.exe below $buildRoot; found $($binaryCandidates.Count)."
}

$binary = $binaryCandidates[0]
$packageName = "IdleHarbor-$Version-windows-$Architecture-portable"
$stageRoot = Join-Path ([IO.Path]::GetTempPath()) "$packageName-$([Guid]::NewGuid().ToString('N'))"
$stagePackage = Join-Path $stageRoot $packageName
$archivePath = Join-Path $outputRoot "$packageName.zip"

New-Item -ItemType Directory -Path $stagePackage -Force | Out-Null
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
try {
    Copy-Item -LiteralPath $binary.FullName -Destination (Join-Path $stagePackage 'IdleHarbor.exe')

    foreach ($relative in @('packaging\install.ps1', 'packaging\uninstall.ps1', 'packaging\install.cmd', 'README.md', 'LICENSE')) {
        $source = Join-Path $repoRoot $relative
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $stagePackage (Split-Path -Leaf $source))
        }
    }
    $distributionGuide = Join-Path $repoRoot 'packaging\README.md'
    if (Test-Path -LiteralPath $distributionGuide -PathType Leaf) {
        Copy-Item -LiteralPath $distributionGuide -Destination (Join-Path $stagePackage 'DISTRIBUTION.md')
    }

    $manifest = [ordered]@{
        product = 'IdleHarbor'
        version = $Version
        architecture = $Architecture
        packageType = 'portable'
        executable = 'IdleHarbor.exe'
        createdUtc = [DateTime]::UtcNow.ToString('o')
    }
    if (-not [string]::IsNullOrWhiteSpace($SourceRevision)) {
        $manifest.sourceRevision = $SourceRevision
    }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stagePackage 'package-manifest.json') -Encoding UTF8

    if (Test-Path -LiteralPath $archivePath) { Remove-Item -LiteralPath $archivePath -Force }
    Compress-Archive -LiteralPath $stagePackage -DestinationPath $archivePath -CompressionLevel Optimal
    Write-Output $archivePath
}
finally {
    if (Test-Path -LiteralPath $stageRoot) { Remove-Item -LiteralPath $stageRoot -Recurse -Force }
}
