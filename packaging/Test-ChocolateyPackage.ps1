[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        throw $Message
    }
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$packageRoot = Join-Path $PSScriptRoot 'chocolatey'
$toolsRoot = Join-Path $packageRoot 'tools'
$nuspecPath = Join-Path $packageRoot 'idleharbor.nuspec'
$installPath = Join-Path $toolsRoot 'chocolateyInstall.ps1'
$uninstallPath = Join-Path $toolsRoot 'chocolateyUninstall.ps1'
$verificationPath = Join-Path $toolsRoot 'VERIFICATION.txt'
$packageLicensePath = Join-Path $toolsRoot 'LICENSE.txt'

foreach ($required in $nuspecPath, $installPath, $uninstallPath, $verificationPath, $packageLicensePath) {
    Assert-True (Test-Path -LiteralPath $required -PathType Leaf) "Missing Chocolatey package file: $required"
}

foreach ($script in $installPath, $uninstallPath) {
    $tokens = $null
    $errors = $null
    [Management.Automation.Language.Parser]::ParseFile($script, [ref]$tokens, [ref]$errors) | Out-Null
    Assert-True ($errors.Count -eq 0) "PowerShell parse failed for $script`: $($errors -join '; ')"
}

[xml]$nuspec = Get-Content -Raw -LiteralPath $nuspecPath
$metadata = $nuspec.package.metadata
$cmake = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'CMakeLists.txt')
$versionMatch = [regex]::Match($cmake, '(?is)project\s*\(\s*IdleHarbor\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)')
Assert-True $versionMatch.Success 'Could not read the IdleHarbor version from CMakeLists.txt.'
$version = $versionMatch.Groups[1].Value

Assert-True ([string]$metadata.id -ceq 'idleharbor') 'Chocolatey package id must remain idleharbor.'
Assert-True ([string]$metadata.version -ceq $version) 'Chocolatey package version does not match CMakeLists.txt.'
Assert-True ([string]$metadata.licenseUrl -ceq "https://github.com/Chris0Jeky/IdleHarbor/blob/v$version/LICENSE") `
    'Chocolatey licence URL is not pinned to the package version.'
Assert-True ([string]$metadata.releaseNotes -ceq "https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v$version") `
    'Chocolatey release-notes URL is not pinned to the package version.'

$rootLicense = (Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'LICENSE')) -replace "`r`n", "`n"
$packageLicense = (Get-Content -Raw -LiteralPath $packageLicensePath) -replace "`r`n", "`n"
Assert-True ($packageLicense -ceq $rootLicense) 'Chocolatey LICENSE.txt differs from the repository GPL licence.'

$install = Get-Content -Raw -LiteralPath $installPath
$expectedUrl = "https://github.com/Chris0Jeky/IdleHarbor/releases/download/v$version/IdleHarbor-$version-windows-x64-portable.zip"
$expectedSha256 = 'b3bc7e5714543cee3877e94f3c74ecfedb30d3599decef316e57715fdf9f6d28'
Assert-True ($install.Contains($expectedUrl)) 'Chocolatey installer URL is not the immutable x64 release asset.'
$checksumMatch = [regex]::Match($install, "checksum64\s*=\s*'(?<hash>[0-9a-f]{64})'")
Assert-True $checksumMatch.Success 'Chocolatey installer lacks a lowercase SHA-256 checksum.'
Assert-True ($checksumMatch.Groups['hash'].Value -ceq $expectedSha256) `
    'Chocolatey installer SHA-256 does not match the published v0.1.0 x64 archive.'
Assert-True ($install -match 'Install-ChocolateyZipPackage') 'Chocolatey installer does not use the ZIP helper.'
Assert-True ($install -notmatch '(?i)Register-ScheduledTask|CurrentVersion\\Run|StartupFolder|Start-Process') `
    'Chocolatey installation must not launch IdleHarbor or configure startup.'

$uninstall = Get-Content -Raw -LiteralPath $uninstallPath
Assert-True ($uninstall -match "IdleHarbor-$([regex]::Escape($version))-windows-x64-portable") `
    'Chocolatey uninstaller does not target the versioned package executable.'
Assert-True ($uninstall -match "ArgumentList\s+'--exit'") `
    'Chocolatey uninstaller lacks the graceful IdleHarbor exit command.'

$verification = Get-Content -Raw -LiteralPath $verificationPath
Assert-True ($verification.Contains($expectedUrl)) 'Chocolatey verification file does not name the release asset.'
Assert-True ($verification.Contains($checksumMatch.Groups['hash'].Value)) `
    'Chocolatey verification file does not repeat the installer SHA-256.'
Assert-True ($verification -match 'NotSigned') 'Chocolatey verification file must disclose the unsigned executable.'

Write-Output "Chocolatey package metadata validated: idleharbor $version"
