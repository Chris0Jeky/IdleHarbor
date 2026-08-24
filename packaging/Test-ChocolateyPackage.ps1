[CmdletBinding()]
param(
    # Download the pinned release asset and confirm it really has the pinned
    # SHA-256. Off by default because it needs network access; the checks that
    # do not need it must stay runnable in CI and offline.
    [switch]$VerifyPublishedChecksum
)

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

# The package pins an immutable published release asset, so its version is the
# release it targets rather than whatever the working tree is building. Those
# are the same version most of the time, and differ in the window between
# tagging a release and repointing the package at the new archive, because the
# archive's SHA-256 does not exist until the release workflow has published it.
[xml]$nuspec = Get-Content -Raw -LiteralPath $nuspecPath
$metadata = $nuspec.package.metadata
$version = [string]$metadata.version
Assert-True ($version -match '^\d+\.\d+\.\d+$') "Chocolatey package version is not a stable release version: $version"

$cmake = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'CMakeLists.txt')
$projectVersionMatch = [regex]::Match($cmake, '(?is)project\s*\(\s*IdleHarbor\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)')
Assert-True $projectVersionMatch.Success 'Could not read the IdleHarbor version from CMakeLists.txt.'
$projectVersion = $projectVersionMatch.Groups[1].Value
Assert-True ([version]$version -le [version]$projectVersion) `
    "Chocolatey package version $version is ahead of the project version $projectVersion; it can only target a release that exists."

Assert-True ([string]$metadata.id -ceq 'idleharbor') 'Chocolatey package id must remain idleharbor.'
Assert-True ([string]$metadata.licenseUrl -ceq "https://github.com/Chris0Jeky/IdleHarbor/blob/v$version/LICENSE") `
    'Chocolatey licence URL is not pinned to the package version.'
Assert-True ([string]$metadata.releaseNotes -ceq "https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v$version") `
    'Chocolatey release-notes URL is not pinned to the package version.'
Assert-True ([string]$metadata.docsUrl -ceq "https://github.com/Chris0Jeky/IdleHarbor/blob/v$version/docs/USER_GUIDE.md") `
    'Chocolatey docs URL is not pinned to the package version.'
Assert-True ([string]$metadata.iconUrl -ceq "https://raw.githubusercontent.com/Chris0Jeky/IdleHarbor/v$version/resources/IdleHarbor.ico") `
    'Chocolatey icon URL is not pinned to the package version.'

$rootLicense = (Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'LICENSE')) -replace "`r`n", "`n"
$packageLicense = (Get-Content -Raw -LiteralPath $packageLicensePath) -replace "`r`n", "`n"
Assert-True ($packageLicense -ceq $rootLicense) 'Chocolatey LICENSE.txt differs from the repository GPL licence.'

$install = Get-Content -Raw -LiteralPath $installPath
$expectedUrl = "https://github.com/Chris0Jeky/IdleHarbor/releases/download/v$version/IdleHarbor-$version-windows-x64-portable.zip"
Assert-True ($install.Contains($expectedUrl)) 'Chocolatey installer URL is not the immutable x64 release asset.'
$checksumMatch = [regex]::Match($install, "checksum64\s*=\s*'(?<hash>[0-9a-f]{64})'")
Assert-True $checksumMatch.Success 'Chocolatey installer lacks a lowercase SHA-256 checksum.'
$pinnedSha256 = $checksumMatch.Groups['hash'].Value
Assert-True ($install -match 'Install-ChocolateyZipPackage') 'Chocolatey installer does not use the ZIP helper.'
Assert-True ($install -notmatch '(?i)Register-ScheduledTask|CurrentVersion\\Run|StartupFolder|Start-Process') `
    'Chocolatey installation must not launch IdleHarbor or configure startup.'

$uninstall = Get-Content -Raw -LiteralPath $uninstallPath
Assert-True ($uninstall -match "IdleHarbor-$([regex]::Escape($version))-windows-x64-portable") `
    'Chocolatey uninstaller does not target the versioned package executable.'
Assert-True ($uninstall -match "ArgumentList\s+'--exit'") `
    'Chocolatey uninstaller lacks the graceful IdleHarbor exit command.'

# VERIFICATION.txt is what a Chocolatey moderator reads, so it has to repeat the
# installer's own URL and digest rather than a stale copy of an older release.
$verification = Get-Content -Raw -LiteralPath $verificationPath
Assert-True ($verification.Contains($expectedUrl)) 'Chocolatey verification file does not name the release asset.'
Assert-True ($verification.Contains($pinnedSha256)) `
    'Chocolatey verification file does not repeat the installer SHA-256.'
# Get-FileHash and SHA256SUMS.txt both emit uppercase, so a digest pasted from
# either is exactly the stale value worth catching; match case-insensitively.
$foreignDigests = @([regex]::Matches($verification, '(?i)[0-9a-f]{64}') |
    Where-Object { $_.Value -ine $pinnedSha256 } | ForEach-Object { $_.Value })
Assert-True ($foreignDigests.Count -eq 0) `
    "Chocolatey verification file names a SHA-256 that is not the installer checksum: $($foreignDigests -join ', ')"

# The digest is not the only thing that goes stale here. VERIFICATION.txt also
# names the release page, the archive, and the folder inside it, and a moderator
# reads all of them.
$foreignArchives = @([regex]::Matches($verification, 'IdleHarbor-\d+\.\d+\.\d+-windows') |
    Where-Object { $_.Value -cne "IdleHarbor-$version-windows" } | ForEach-Object { $_.Value })
Assert-True ($foreignArchives.Count -eq 0) `
    "Chocolatey verification file names an archive from another release: $($foreignArchives -join ', ')"
$foreignTags = @([regex]::Matches($verification, 'v\d+\.\d+\.\d+') |
    Where-Object { $_.Value -cne "v$version" } | ForEach-Object { $_.Value })
Assert-True ($foreignTags.Count -eq 0) `
    "Chocolatey verification file names another release tag: $($foreignTags -join ', ')"
Assert-True ($verification -match 'NotSigned') 'Chocolatey verification file must disclose the unsigned executable.'

if ($VerifyPublishedChecksum) {
    $temporaryArchive = Join-Path ([IO.Path]::GetTempPath()) "idleharbor-choco-$([Guid]::NewGuid().ToString('N')).zip"
    try {
        Invoke-WebRequest -Uri $expectedUrl -OutFile $temporaryArchive -UseBasicParsing
        $publishedSha256 = (Get-FileHash -LiteralPath $temporaryArchive -Algorithm SHA256).Hash.ToLowerInvariant()
        Assert-True ($publishedSha256 -ceq $pinnedSha256) `
            "Published archive SHA-256 $publishedSha256 does not match the pinned $pinnedSha256."
        Write-Output "Published archive checksum verified against $expectedUrl"
    }
    finally {
        Remove-Item -LiteralPath $temporaryArchive -Force -ErrorAction SilentlyContinue
    }
}

if ($version -cne $projectVersion) {
    Write-Output "Chocolatey package metadata validated: idleharbor $version (project version is $projectVersion; repoint the package after that release publishes)"
}
else {
    Write-Output "Chocolatey package metadata validated: idleharbor $version"
}
