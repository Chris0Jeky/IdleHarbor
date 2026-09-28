Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Plain top-level setup (no BeforeAll) so the file loads under both
# Windows PowerShell 5.1 and PowerShell 7, and under Pester 4 or 5.
$script:ReleaseVersionScript = Join-Path (Join-Path (Join-Path $PSScriptRoot '..') 'packaging') 'Test-ReleaseVersion.ps1'
$script:ReleaseVersionScript = (Resolve-Path -LiteralPath $script:ReleaseVersionScript).Path

function New-ReleaseVersionFixture([string]$Version) {
    $parts = $Version -split '\.'
    $commaVersion = '{0},{1},{2},0' -f $parts[0], $parts[1], $parts[2]
    $manifestVersion = '{0}.{1}.{2}.0' -f $parts[0], $parts[1], $parts[2]

    $root = Join-Path ([IO.Path]::GetTempPath()) ('IdleHarbor-release-version-{0}' -f [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path (Join-Path $root 'include/idleharbor') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $root 'resources') -Force | Out-Null

    Set-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Encoding ASCII -Value @"
cmake_minimum_required(VERSION 3.21)
project(
  IdleHarbor
  VERSION $Version
  LANGUAGES CXX
)
"@
    Set-Content -LiteralPath (Join-Path $root 'include/idleharbor/version.hpp') -Encoding ASCII -Value @"
#pragma once

#include <string_view>

namespace idleharbor {

inline constexpr std::wstring_view kVersion = L"$Version";

}  // namespace idleharbor
"@
    Set-Content -LiteralPath (Join-Path $root 'resources/IdleHarbor.rc') -Encoding ASCII -Value @"
#include <windows.h>

VS_VERSION_INFO VERSIONINFO
 FILEVERSION $commaVersion
 PRODUCTVERSION $commaVersion
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "FileVersion", "$Version\0"
            VALUE "ProductVersion", "$Version\0"
        END
    END
END
"@
    Set-Content -LiteralPath (Join-Path $root 'resources/app.manifest') -Encoding ASCII -Value @"
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity
    name="Chris0Jeky.IdleHarbor"
    processorArchitecture="*"
    type="win32"
    version="$manifestVersion" />
</assembly>
"@
    return $root
}

function Remove-ReleaseVersionFixture([string]$Path) {
    if (-not [string]::IsNullOrWhiteSpace($Path) -and (Test-Path -LiteralPath $Path)) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

Describe 'Test-ReleaseVersion.ps1' {
    It 'RejectsPrereleaseTag' {
        $fixture = New-ReleaseVersionFixture '1.2.3'
        try {
            $message = ''
            try { & $script:ReleaseVersionScript -Tag 'v1.2.3-rc.1' -SourceRoot $fixture }
            catch { $message = $_.Exception.Message }
            $message | Should Match 'not a supported stable SemVer tag'
        }
        finally {
            Remove-ReleaseVersionFixture $fixture
        }
    }

    It 'RejectsMissingV' {
        $fixture = New-ReleaseVersionFixture '1.2.3'
        try {
            $message = ''
            try { & $script:ReleaseVersionScript -Tag '1.2.3' -SourceRoot $fixture }
            catch { $message = $_.Exception.Message }
            $message | Should Match 'not a supported stable SemVer tag'
        }
        finally {
            Remove-ReleaseVersionFixture $fixture
        }
    }

    It 'ExactMatch rejects a tag when the fixture tree versions differ' {
        $fixture = New-ReleaseVersionFixture '1.2.3'
        try {
            $message = ''
            try { & $script:ReleaseVersionScript -Tag 'v9.9.9' -SourceRoot $fixture }
            catch { $message = $_.Exception.Message }
            $message | Should Match 'does not exactly match'
        }
        finally {
            Remove-ReleaseVersionFixture $fixture
        }
    }

    It 'ExactMatch rejects a tree with a single drifted source' {
        $fixture = New-ReleaseVersionFixture '1.2.3'
        try {
            $header = Join-Path $fixture 'include/idleharbor/version.hpp'
            $text = Get-Content -Raw -LiteralPath $header
            Set-Content -LiteralPath $header -Encoding ASCII -Value ($text -replace '1\.2\.3', '1.2.4')
            $message = ''
            try { & $script:ReleaseVersionScript -Tag 'v1.2.3' -SourceRoot $fixture }
            catch { $message = $_.Exception.Message }
            $message | Should Match "does not exactly match tag version '1.2.3'"
        }
        finally {
            Remove-ReleaseVersionFixture $fixture
        }
    }

    It 'ExactMatch accepts a tag when every fixture source agrees' {
        $fixture = New-ReleaseVersionFixture '1.2.3'
        try {
            $output = & $script:ReleaseVersionScript -Tag 'v1.2.3' -SourceRoot $fixture
            $output | Should Be 'Release version validated: v1.2.3 -> 1.2.3'
        }
        finally {
            Remove-ReleaseVersionFixture $fixture
        }
    }
}
