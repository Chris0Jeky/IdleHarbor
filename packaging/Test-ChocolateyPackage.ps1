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
$beforeModifyPath = Join-Path $toolsRoot 'chocolateyBeforeModify.ps1'
$stopHelperPath = Join-Path $toolsRoot 'Stop-IdleHarborPackage.ps1'
$verificationPath = Join-Path $toolsRoot 'VERIFICATION.txt'
$packageLicensePath = Join-Path $toolsRoot 'LICENSE.txt'

foreach ($required in $nuspecPath, $installPath, $uninstallPath, $beforeModifyPath, $stopHelperPath, $verificationPath, $packageLicensePath) {
    Assert-True (Test-Path -LiteralPath $required -PathType Leaf) "Missing Chocolatey package file: $required"
}

foreach ($script in $installPath, $uninstallPath, $beforeModifyPath, $stopHelperPath) {
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

if ($version -cne $projectVersion) {
    $changelogPath = Join-Path $repositoryRoot 'CHANGELOG.md'
    Assert-True (Test-Path -LiteralPath $changelogPath -PathType Leaf) 'CHANGELOG.md is required to justify a lagging Chocolatey package.'
    $history = Get-Content -Raw -LiteralPath $changelogPath
    $releases = @([regex]::Matches($history, '(?m)^## \[(?<version>\d+\.\d+\.\d+)\] - (?<date>\d{4}-\d{2}-\d{2})\r?$'))
    Assert-True ($releases.Count -gt 0) 'CHANGELOG.md contains no dated release history for the lagging package.'
    $lastVersion = $null
    foreach ($release in $releases) {
        $releaseVersion = [version]$release.Groups['version'].Value
        try {
            $null = [DateTime]::ParseExact($release.Groups['date'].Value, 'yyyy-MM-dd', [Globalization.CultureInfo]::InvariantCulture)
        }
        catch { throw "CHANGELOG.md contains an invalid release date: $($release.Groups['date'].Value)." }
        Assert-True ($releaseVersion -le [version]$projectVersion) 'CHANGELOG.md contains a release ahead of the project version.'
        if ($null -ne $lastVersion) {
            Assert-True ($releaseVersion -lt $lastVersion) 'CHANGELOG.md release versions must be strictly descending without duplicates.'
        }
        $lastVersion = $releaseVersion
    }
    # Before a release is dated, the newest dated version is its predecessor.
    # After dating it, exactly the next section is allowed during repointing.
    $previousIndex = 0
    if ($releases[0].Groups['version'].Value -ceq $projectVersion) { $previousIndex = 1 }
    Assert-True ($previousIndex -lt $releases.Count) 'CHANGELOG.md provides no previous release for this package.'
    $previousVersion = $releases[$previousIndex].Groups['version'].Value
    Assert-True ($version -ceq $previousVersion) `
        "Chocolatey package $version must target project $projectVersion or its immediate previous release $previousVersion."
}

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

# Architecture guard (issue #51). The decision is a pure function over the native
# registry value and process variables; extract it from the installer so the test
# runs the shipped text, with injected values and no machine changes.
$installAst = [Management.Automation.Language.Parser]::ParseInput($install, [ref]$null, [ref]$null)
$guardAst = $installAst.Find({
        param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Assert-IdleHarborX64Os'
    }, $false)
Assert-True ($null -ne $guardAst) 'Chocolatey installer lacks the Assert-IdleHarborX64Os architecture guard.'
. ([scriptblock]::Create($guardAst.Extent.Text))

# The native value wins regardless of the process flavour, including Prism.
$architectureCases = @(
    @{ Name = '64-bit PowerShell on x64'; Native = 'AMD64'; Arch = 'AMD64'; Wow = ''; Accepted = $true },
    @{ Name = '32-bit PowerShell on x64'; Native = 'AMD64'; Arch = 'x86'; Wow = 'AMD64'; Accepted = $true },
    @{ Name = 'missing process values on x64'; Native = 'AMD64'; Arch = ''; Wow = ''; Accepted = $true },
    @{ Name = 'conflicting process values on x64'; Native = 'AMD64'; Arch = 'ARM64'; Wow = 'ARM64'; Accepted = $true },
    @{ Name = 'case-insensitive native x64'; Native = 'amd64'; Arch = 'x86'; Wow = ''; Accepted = $true },
    @{ Name = 'Prism x64 process on ARM64 without W6432'; Native = 'ARM64'; Arch = 'AMD64'; Wow = ''; Accepted = $false },
    @{ Name = 'x64-emulated process on ARM64 with W6432'; Native = 'ARM64'; Arch = 'AMD64'; Wow = 'ARM64'; Accepted = $false },
    @{ Name = '32-bit PowerShell on ARM64'; Native = 'ARM64'; Arch = 'x86'; Wow = 'ARM64'; Accepted = $false },
    @{ Name = 'native ARM64 PowerShell'; Native = 'ARM64'; Arch = 'ARM64'; Wow = ''; Accepted = $false },
    @{ Name = 'conflicting W6432 on ARM64'; Native = 'ARM64'; Arch = 'x86'; Wow = 'AMD64'; Accepted = $false },
    @{ Name = 'x86 Windows'; Native = 'x86'; Arch = 'x86'; Wow = ''; Accepted = $false },
    @{ Name = 'x86 with misleading AMD64 process values'; Native = 'x86'; Arch = 'AMD64'; Wow = 'AMD64'; Accepted = $false },
    @{ Name = 'unknown native architecture'; Native = 'unknown'; Arch = 'AMD64'; Wow = ''; Accepted = $false },
    @{ Name = 'missing native registry value'; Native = ''; Arch = 'AMD64'; Wow = ''; Accepted = $false },
    @{ Name = 'blank native value with W6432 AMD64'; Native = ' '; Arch = 'x86'; Wow = 'AMD64'; Accepted = $false },
    @{ Name = 'unreadable native registry'; Native = $null; Arch = 'AMD64'; Wow = ''; Accepted = $false; ReadFails = $true },
    @{ Name = 'unreadable native registry with W6432 AMD64'; Native = $null; Arch = 'x86'; Wow = 'AMD64'; Accepted = $false; ReadFails = $true }
)
foreach ($case in $architectureCases) {
    $failure = $null
    try { Assert-IdleHarborX64Os -NativeArchitecture $case.Native -ProcessorArchitecture $case.Arch -ProcessorArchitectureW6432 $case.Wow }
    catch { $failure = $_.Exception.Message }
    if ($case.Accepted) { Assert-True ($null -eq $failure) "Architecture guard rejected $($case.Name): $failure" }
    else { Assert-True ($failure -match 'x64 build only') "Architecture guard did not reject $($case.Name) clearly: $failure" }
}

# The guard has to run, and fail, before the ZIP helper can download anything.
# Run the installer with the registry read and helper doubled and process values
# restored below; no registry is read/changed and nothing is downloaded/unpacked.
$installerState = @{ Downloads = 0 }
function Get-ItemPropertyValue {
    [CmdletBinding()]
    param([string]$LiteralPath, [string]$Name)
    Assert-True ($LiteralPath -ceq 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment') `
        'Installer must read the machine-wide environment key.'
    Assert-True ($Name -ceq 'PROCESSOR_ARCHITECTURE') 'Installer must read the native architecture value.'
    $installerState.Reads++
    if ($case.ContainsKey('ReadFails') -and $case.ReadFails) { throw 'Synthetic registry access denied.' }
    return $case.Native
}
function Install-ChocolateyZipPackage {
    [CmdletBinding()]
    param($packageName, $unzipLocation, $url64bit, $checksum64, $checksumType64)
    $installerState.Downloads++
}
$savedArchitecture = [Environment]::GetEnvironmentVariable('PROCESSOR_ARCHITECTURE', 'Process')
$savedWow = [Environment]::GetEnvironmentVariable('PROCESSOR_ARCHITEW6432', 'Process')
try {
    foreach ($case in $architectureCases) {
        [Environment]::SetEnvironmentVariable('PROCESSOR_ARCHITECTURE', $case.Arch, 'Process')
        [Environment]::SetEnvironmentVariable('PROCESSOR_ARCHITEW6432', $case.Wow, 'Process')
        $installerState.Downloads = 0
        $installerState.Reads = 0
        $failure = $null
        try { & $installPath | Out-Null }
        catch { $failure = $_.Exception.Message }
        Assert-True ($installerState.Reads -eq 1) "Installer did not read the native registry value on $($case.Name)."
        if ($case.Accepted) {
            Assert-True ($null -eq $failure -and $installerState.Downloads -eq 1) "Installer did not proceed on $($case.Name): $failure"
        }
        else {
            Assert-True ($installerState.Downloads -eq 0) "Installer reached the download on $($case.Name)."
            Assert-True ($failure -match 'x64 build only') "Installer did not fail clearly on $($case.Name): $failure"
            Assert-True ($failure -match 'unsupported or unreadable machine-wide architecture') `
                "Installer did not explain the native architecture refusal on $($case.Name): $failure"
        }
    }
}
finally {
    [Environment]::SetEnvironmentVariable('PROCESSOR_ARCHITECTURE', $savedArchitecture, 'Process')
    [Environment]::SetEnvironmentVariable('PROCESSOR_ARCHITEW6432', $savedWow, 'Process')
}
Write-Output "Chocolatey native architecture guard passed ($($architectureCases.Count) cases, pure decision and installer before download)."

# Process-query failure, another-session and no-process outcomes of the shutdown
# path are executed by tests/Test-ChocolateyShutdown.ps1, registered in
# tests/Test-RegressionScripts.ps1; the contract is pinned here so it cannot
# be dropped from the helper without this check noticing.
$stopHelperText = Get-Content -Raw -LiteralPath $stopHelperPath
Assert-True ($stopHelperText -match 'Get-CimInstance\s+Win32_Process[^\r\n]*-ErrorAction\s+Stop') `
    'Shared shutdown helper must treat a process-query failure as an error (Get-CimInstance -ErrorAction Stop).'
Assert-True ($stopHelperText -match '\.SessionId') 'Shared shutdown helper must compare the Windows session of a running process.'

$uninstall = Get-Content -Raw -LiteralPath $uninstallPath
Assert-True ($uninstall -match "IdleHarbor-$([regex]::Escape($version))-windows-x64-portable") `
    'Chocolatey uninstaller does not target the versioned package executable.'
Assert-True ($uninstall.Contains('Stop-IdleHarborPackage -Executable $executable')) `
    'Chocolatey uninstaller does not invoke the shared graceful shutdown helper.'

# choco upgrade does not run chocolateyUninstall.ps1. It runs the installed
# package's chocolateyBeforeModify.ps1, so the graceful shutdown has to exist
# there too or a running IdleHarbor locks its own package directory.
$beforeModify = Get-Content -Raw -LiteralPath $beforeModifyPath
Assert-True ($beforeModify -match "IdleHarbor-$([regex]::Escape($version))-windows-x64-portable") `
    'Chocolatey before-modify script does not target the versioned package executable.'
Assert-True ($beforeModify.Contains('Stop-IdleHarborPackage -Executable $executable')) `
    'Chocolatey before-modify script does not invoke the shared graceful shutdown helper.'
$stopHelper = Get-Content -Raw -LiteralPath $stopHelperPath
Assert-True ($stopHelper -match "ArgumentList\s+'--exit'") 'Shared shutdown helper lacks the graceful IdleHarbor exit command.'

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
