Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$NewReleasePackage = Join-Path (Split-Path -Parent $PSScriptRoot) 'packaging\New-ReleasePackage.ps1'

function New-ReleaseFixture {
    [CmdletBinding()]
    param(
        [switch]$IncludeLicense
    )

    $tempRoot = Join-Path ([IO.Path]::GetTempPath()) "IdleHarbor-release-package-test-$([Guid]::NewGuid().ToString('N'))"
    $repoRoot = Join-Path $tempRoot 'repo'
    $buildRoot = Join-Path $tempRoot 'build'
    $outputRoot = Join-Path $tempRoot 'dist'
    New-Item -ItemType Directory -Path (Join-Path $repoRoot 'packaging'), $buildRoot, $outputRoot -Force | Out-Null

    $payload = @{
        'packaging\install.ps1'      = '# fixture installer'
        'packaging\uninstall.ps1'    = '# fixture uninstaller'
        'packaging\install.cmd'      = '@echo off'
        'README.md'                  = '# fixture readme'
        'THIRD-PARTY-NOTICES.md'     = '# fixture notices'
    }
    if ($IncludeLicense) {
        $payload['LICENSE'] = 'fixture licence text'
    }
    foreach ($relative in $payload.Keys) {
        Set-Content -LiteralPath (Join-Path $repoRoot $relative) -Value $payload[$relative] -Encoding ASCII
    }

    [IO.File]::WriteAllBytes((Join-Path $buildRoot 'IdleHarbor.exe'), [byte[]](0x4d, 0x5a, 0x00, 0x01))

    return [pscustomobject]@{
        TempRoot   = $tempRoot
        RepoRoot   = $repoRoot
        BuildRoot  = $buildRoot
        OutputRoot = $outputRoot
    }
}

function Remove-ReleaseFixture {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [pscustomobject]$Fixture
    )

    if (Test-Path -LiteralPath $Fixture.TempRoot) {
        Remove-Item -LiteralPath $Fixture.TempRoot -Recurse -Force
    }
}

Describe 'New-ReleasePackage required payload' {
    It 'MissingLicenseThrows' {
        $fixture = New-ReleaseFixture
        try {
            $message = ''
            try {
                & $NewReleasePackage `
                    -BuildDirectory $fixture.BuildRoot `
                    -OutputDirectory $fixture.OutputRoot `
                    -Version '0.1.0-test' `
                    -Architecture x64 `
                    -RepositoryRoot $fixture.RepoRoot
            }
            catch {
                $message = $_.Exception.Message
            }
            $message | Should Match 'LICENSE'
            @(Get-ChildItem -LiteralPath $fixture.OutputRoot -Filter '*.zip' -ErrorAction SilentlyContinue).Count | Should Be 0
        }
        finally {
            Remove-ReleaseFixture $fixture
        }
    }

    It 'HappyPathIncludesLicense' {
        $fixture = New-ReleaseFixture -IncludeLicense
        try {
            $archive = & $NewReleasePackage `
                -BuildDirectory $fixture.BuildRoot `
                -OutputDirectory $fixture.OutputRoot `
                -Version '0.1.0-test' `
                -Architecture x64 `
                -RepositoryRoot $fixture.RepoRoot
            (Test-Path -LiteralPath ([string]$archive) -PathType Leaf) | Should Be $true

            Add-Type -AssemblyName System.IO.Compression.FileSystem
            $zip = [IO.Compression.ZipFile]::OpenRead([string]$archive)
            try {
                $entryNames = @($zip.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
                ($entryNames -contains 'IdleHarbor-0.1.0-test-windows-x64-portable/LICENSE') | Should Be $true

                $manifestEntry = @($zip.Entries | Where-Object {
                    $_.FullName.Replace('\', '/') -ceq 'IdleHarbor-0.1.0-test-windows-x64-portable/package-manifest.json'
                })
                $manifestEntry.Count | Should Be 1
                $reader = New-Object IO.StreamReader($manifestEntry[0].Open())
                try {
                    $packageManifest = $reader.ReadToEnd() | ConvertFrom-Json
                }
                finally {
                    $reader.Dispose()
                }
                $packageManifest.licenseFile | Should Be 'LICENSE'
            }
            finally {
                $zip.Dispose()
            }
        }
        finally {
            Remove-ReleaseFixture $fixture
        }
    }
}
