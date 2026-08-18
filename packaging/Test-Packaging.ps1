[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$packagingRoot = $PSScriptRoot
foreach ($script in Get-ChildItem -LiteralPath $packagingRoot -Filter '*.ps1' -File) {
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($script.FullName, [ref]$tokens, [ref]$errors) | Out-Null
    Assert-True ($errors.Count -eq 0) "PowerShell parse failed for $($script.Name): $($errors -join '; ')"
}

foreach ($workflow in Get-ChildItem -LiteralPath (Join-Path (Split-Path -Parent $packagingRoot) '.github\workflows') -Filter '*.yml' -File) {
    foreach ($line in Get-Content -LiteralPath $workflow.FullName | Where-Object { $_ -match '^\s*uses:' }) {
        Assert-True ($line -match '@[0-9a-f]{40}(?:\s|$)') "Workflow action is not pinned in $($workflow.Name): $line"
    }
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) "IdleHarbor-packaging-test-$([Guid]::NewGuid().ToString('N'))"
$sourceRoot = Join-Path $tempRoot 'source'
$buildRoot = Join-Path $tempRoot 'build'
$outputRoot = Join-Path $tempRoot 'dist'
$installRoot = Join-Path $tempRoot 'IdleHarbor'
New-Item -ItemType Directory -Path $sourceRoot, $buildRoot, $outputRoot -Force | Out-Null
try {
    $fakeExecutable = Join-Path $buildRoot 'IdleHarbor.exe'
    [IO.File]::WriteAllBytes($fakeExecutable, [byte[]](0x4d, 0x5a, 0x00, 0x01, 0x02, 0x03))

    & (Join-Path $packagingRoot 'install.ps1') -SourcePath $buildRoot -InstallRoot $installRoot -Startup None -NoLaunch -WhatIf | Out-Null
    Assert-True (-not (Test-Path -LiteralPath $installRoot)) 'Installer -WhatIf created files.'

    & (Join-Path $packagingRoot 'install.ps1') -SourcePath $buildRoot -InstallRoot $installRoot -Startup None -NoLaunch | Out-Null
    Assert-True (Test-Path -LiteralPath (Join-Path $installRoot '.idleharbor-managed.json')) 'Installer did not write its ownership marker.'
    Assert-True (Test-Path -LiteralPath (Join-Path $installRoot 'IdleHarbor.exe')) 'Installer did not copy the executable.'
    & (Join-Path $packagingRoot 'uninstall.ps1') -InstallRoot $installRoot | Out-Null
    Assert-True (-not (Test-Path -LiteralPath $installRoot)) 'Uninstaller left an empty install root.'

    $archive = & (Join-Path $packagingRoot 'New-ReleasePackage.ps1') `
        -BuildDirectory $buildRoot `
        -OutputDirectory $outputRoot `
        -Version '0.1.0-test' `
        -Architecture x64
    Assert-True (Test-Path -LiteralPath ([string]$archive) -PathType Leaf) 'Portable archive was not created.'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead([string]$archive)
    try {
        $entryNames = @($zip.Entries | ForEach-Object FullName)
        Assert-True ($entryNames -contains 'IdleHarbor-0.1.0-test-windows-x64-portable/IdleHarbor.exe') 'Archive lacks the executable.'
        Assert-True ($entryNames -contains 'IdleHarbor-0.1.0-test-windows-x64-portable/install.ps1') 'Archive lacks the installer.'
        Assert-True ($entryNames -contains 'IdleHarbor-0.1.0-test-windows-x64-portable/DISTRIBUTION.md') 'Archive lacks the distribution guide.'
    }
    finally {
        $zip.Dispose()
    }

    $sbom = Join-Path $outputRoot 'test.spdx.json'
    & (Join-Path $packagingRoot 'New-Sbom.ps1') -InputPath $fakeExecutable -OutputPath $sbom -Version '0.1.0-test' -Architecture x64 | Out-Null
    $sbomDocument = Get-Content -Raw -LiteralPath $sbom | ConvertFrom-Json
    Assert-True ($sbomDocument.spdxVersion -eq 'SPDX-2.3') 'SBOM is not SPDX 2.3.'
    Assert-True ($sbomDocument.files[0].checksums[0].algorithm -eq 'SHA256') 'SBOM lacks a SHA-256 file checksum.'

    $checksums = Join-Path $outputRoot 'SHA256SUMS.txt'
    & (Join-Path $packagingRoot 'New-Checksums.ps1') -InputDirectory $outputRoot -OutputPath $checksums | Out-Null
    Assert-True ((Get-Content -LiteralPath $checksums).Count -ge 2) 'Checksum manifest did not include release files.'
    Write-Output 'Packaging checks passed.'
}
finally {
    if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}
