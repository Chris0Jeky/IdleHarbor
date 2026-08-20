[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Get-ScriptFunctionDefinition([string]$ScriptPath, [string]$Name) {
    $tokens = $null
    $errors = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseFile(
        $ScriptPath,
        [ref]$tokens,
        [ref]$errors)
    Assert-True ($errors.Count -eq 0) "PowerShell parse failed while loading ${Name}: $($errors -join '; ')"
    $definition = $ast.Find(
        {
            param($node)
            return ($node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
                $node.Name -eq $Name)
        },
        $true)
    Assert-True ($null -ne $definition) "Function ${Name} was not found in ${ScriptPath}."
    return [scriptblock]::Create($definition.Extent.Text)
}

function Assert-StartupOwnershipPredicates([string]$ScriptPath, [string]$TestRoot) {
    foreach ($name in @(
        'Get-FullPath',
        'Test-SamePath',
        'Get-ExpectedRunCommand',
        'Test-RunCommandOwned',
        'Test-TaskActionsOwned',
        'Test-StartupShortcutOwned'
    )) {
        . (Get-ScriptFunctionDefinition $ScriptPath $name)
    }

    $executable = Join-Path $TestRoot 'IdleHarbor\IdleHarbor.exe'
    $workingDirectory = Split-Path -Parent $executable
    $ownedAction = [pscustomobject]@{
        Execute = $executable
        Arguments = '--start --minimized'
        WorkingDirectory = $workingDirectory
    }
    $foreignAction = [pscustomobject]@{
        Execute = (Join-Path $env:WINDIR 'System32\notepad.exe')
        Arguments = ''
        WorkingDirectory = (Join-Path $env:WINDIR 'System32')
    }
    Assert-True (Test-TaskActionsOwned @($ownedAction) $executable) 'Exact single task action was not recognized as owned.'
    Assert-True (-not (Test-TaskActionsOwned @($ownedAction, $foreignAction) $executable)) `
        'Task with an additional foreign action was recognized as owned.'

    $changedArguments = $ownedAction.PSObject.Copy()
    $changedArguments.Arguments = '--start'
    Assert-True (-not (Test-TaskActionsOwned @($changedArguments) $executable)) `
        'Task with changed arguments was recognized as owned.'

    $changedDirectory = $ownedAction.PSObject.Copy()
    $changedDirectory.WorkingDirectory = $TestRoot
    Assert-True (-not (Test-TaskActionsOwned @($changedDirectory) $executable)) `
        'Task with a changed working directory was recognized as owned.'

    $ownedShortcut = [pscustomobject]@{
        TargetPath = $executable
        Arguments = '--start --minimized'
        WorkingDirectory = $workingDirectory
    }
    Assert-True (Test-StartupShortcutOwned $ownedShortcut $executable) `
        'Exact startup shortcut was not recognized as owned.'
    $changedShortcut = $ownedShortcut.PSObject.Copy()
    $changedShortcut.Arguments = '--start --minimized --custom'
    Assert-True (-not (Test-StartupShortcutOwned $changedShortcut $executable)) `
        'Customized startup shortcut was recognized as owned.'

    $runCommand = Get-ExpectedRunCommand $executable
    Assert-True (Test-RunCommandOwned $runCommand $executable) 'Exact Run command was not recognized as owned.'
    Assert-True (-not (Test-RunCommandOwned "$runCommand --custom" $executable)) `
        'Customized Run command was recognized as owned.'
}

function Assert-InstallerOwnershipPreflight([string]$ScriptPath) {
    $contents = Get-Content -Raw -LiteralPath $ScriptPath
    $preflight = $contents.IndexOf('Assert-StartupEntriesOwned $destinationExecutable', [StringComparison]::Ordinal)
    $stop = $contents.IndexOf('Stop-OwnedApplicationIfRunning $destinationExecutable', [StringComparison]::Ordinal)
    $copy = $contents.IndexOf('foreach ($fileName in $KnownFiles)', [StringComparison]::Ordinal)
    $marker = $contents.IndexOf("if (Confirm-Change `$markerPath 'Write ownership marker')", [StringComparison]::Ordinal)
    Assert-True ($preflight -ge 0) 'Installer lacks its startup ownership preflight.'
    Assert-True ($preflight -lt $stop) 'Installer stops the application before its startup ownership preflight.'
    Assert-True ($preflight -lt $copy) 'Installer copies files before its startup ownership preflight.'
    Assert-True ($preflight -lt $marker) 'Installer rewrites its marker before its startup ownership preflight.'
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
$releaseWorkflow = Get-Content -Raw -LiteralPath (Join-Path (Split-Path -Parent $packagingRoot) '.github\workflows\release.yml')
Assert-True ($releaseWorkflow -match 'Test-ReleaseLicense\.ps1') `
    'Release workflow lacks its tracked-LICENSE publication guard.'

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) "IdleHarbor-packaging-test-$([Guid]::NewGuid().ToString('N'))"
$sourceRoot = Join-Path $tempRoot 'source'
$buildRoot = Join-Path $tempRoot 'build'
$outputRoot = Join-Path $tempRoot 'dist'
$installRoot = Join-Path $tempRoot 'IdleHarbor'
$purgeInstallRoot = Join-Path $tempRoot 'PurgeInstall\IdleHarbor'
Assert-StartupOwnershipPredicates (Join-Path $packagingRoot 'install.ps1') $tempRoot
Assert-StartupOwnershipPredicates (Join-Path $packagingRoot 'uninstall.ps1') $tempRoot
Assert-InstallerOwnershipPreflight (Join-Path $packagingRoot 'install.ps1')
New-Item -ItemType Directory -Path $sourceRoot, $buildRoot, $outputRoot -Force | Out-Null
try {
    $fakeExecutable = Join-Path $buildRoot 'IdleHarbor.exe'
    [IO.File]::WriteAllBytes($fakeExecutable, [byte[]](0x4d, 0x5a, 0x00, 0x01, 0x02, 0x03))

    & (Join-Path $packagingRoot 'install.ps1') -SourcePath $buildRoot -InstallRoot $installRoot -NoLaunch -WhatIf | Out-Null
    Assert-True (-not (Test-Path -LiteralPath $installRoot)) 'Installer -WhatIf created files.'
    & (Join-Path $packagingRoot 'install.ps1') -SourcePath $buildRoot -InstallRoot $installRoot -Startup RunKey -NoLaunch -WhatIf | Out-Null
    Assert-True (-not (Test-Path -LiteralPath $installRoot)) 'Installer startup-mode -WhatIf created files.'
    & (Join-Path $packagingRoot 'install.ps1') -SourcePath $buildRoot -InstallRoot $installRoot -Startup TaskScheduler -NoLaunch -WhatIf | Out-Null
    Assert-True (-not (Test-Path -LiteralPath $installRoot)) 'Installer Task Scheduler -WhatIf created files.'

    $transactionParent = Join-Path $tempRoot 'Transactional'
    $transactionRoot = Join-Path $transactionParent 'IdleHarbor'
    New-Item -ItemType Directory -Path $transactionParent -Force | Out-Null
    $transactionSentinel = Join-Path $transactionParent 'keep.txt'
    Set-Content -LiteralPath $transactionSentinel -Value 'preserve'
    $transactionFailed = $false
    try {
        & (Join-Path $packagingRoot 'install.ps1') `
            -SourcePath $buildRoot `
            -InstallRoot $transactionRoot `
            -Startup None `
            -NoLaunch `
            -InjectFailureAt AfterCopy | Out-Null
    }
    catch { $transactionFailed = $true }
    Assert-True $transactionFailed 'Injected fresh-install failure did not fail.'
    Assert-True (-not (Test-Path -LiteralPath $transactionRoot)) `
        'Fresh-install rollback left an owned installation directory behind.'
    Assert-True ((Get-Content -Raw -LiteralPath $transactionSentinel).Trim() -eq 'preserve') `
        'Fresh-install rollback crossed its exact ownership boundary.'

    & (Join-Path $packagingRoot 'install.ps1') -SourcePath $buildRoot -InstallRoot $installRoot -NoLaunch | Out-Null
    Assert-True (Test-Path -LiteralPath (Join-Path $installRoot '.idleharbor-managed.json')) 'Installer did not write its ownership marker.'
    Assert-True (Test-Path -LiteralPath (Join-Path $installRoot 'IdleHarbor.exe')) 'Installer did not copy the executable.'
    & (Join-Path $packagingRoot 'install.ps1') -SourcePath $installRoot -InstallRoot $installRoot -Startup None -NoLaunch | Out-Null
    Assert-True (Test-Path -LiteralPath (Join-Path $installRoot 'IdleHarbor.exe')) 'Same-directory reinstall removed the executable.'
    & (Join-Path $packagingRoot 'install.ps1') -SourcePath $buildRoot -InstallRoot $installRoot -Startup None -NoLaunch | Out-Null
    Assert-True (Test-Path -LiteralPath (Join-Path $installRoot 'IdleHarbor.exe')) 'Managed reinstall removed the executable.'
    & (Join-Path $packagingRoot 'uninstall.ps1') -InstallRoot $installRoot | Out-Null
    Assert-True (-not (Test-Path -LiteralPath $installRoot)) 'Uninstaller left an empty install root.'

    & (Join-Path $packagingRoot 'install.ps1') -SourcePath $buildRoot -InstallRoot $purgeInstallRoot -NoLaunch | Out-Null
    $savedAppData = $env:APPDATA
    $savedLocalAppData = $env:LOCALAPPDATA
    $testAppData = Join-Path $tempRoot 'appdata'
    $testLocalAppData = Join-Path $tempRoot 'localappdata'
    $foreignData = Join-Path $testAppData 'IdleHarbor'
    $ownedData = Join-Path $testLocalAppData 'IdleHarbor'
    New-Item -ItemType Directory -Path $foreignData, $ownedData -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $foreignData 'foreign.txt') -Value 'preserve'
    Set-Content -LiteralPath (Join-Path $ownedData 'settings.ini') -Value 'owned'
    Set-Content -LiteralPath (Join-Path $ownedData '.idleharbor-data.json') -Value '{"product":"IdleHarbor","markerVersion":1,"settingsFile":"settings.ini"}'
    $env:APPDATA = $testAppData
    $env:LOCALAPPDATA = $testLocalAppData
    try {
        & (Join-Path $packagingRoot 'uninstall.ps1') -InstallRoot $purgeInstallRoot -PurgeData | Out-Null
    }
    finally {
        $env:APPDATA = $savedAppData
        $env:LOCALAPPDATA = $savedLocalAppData
    }
    Assert-True (Test-Path -LiteralPath $foreignData) 'Uninstaller purged an unowned data directory.'
    Assert-True (-not (Test-Path -LiteralPath $ownedData)) 'Uninstaller did not purge marker-owned data.'

    $foreignRoot = Join-Path $tempRoot 'foreign\IdleHarbor'
    New-Item -ItemType Directory -Path $foreignRoot -Force | Out-Null
    $foreignExecutable = Join-Path $foreignRoot 'IdleHarbor.exe'
    [IO.File]::WriteAllBytes($foreignExecutable, [byte[]](0x66, 0x6f, 0x72, 0x65, 0x69, 0x67, 0x6e))
    $foreignRejected = $false
    try {
        & (Join-Path $packagingRoot 'install.ps1') -SourcePath $buildRoot -InstallRoot $foreignRoot -NoLaunch | Out-Null
    }
    catch { $foreignRejected = $true }
    Assert-True $foreignRejected 'Installer accepted a non-empty destination without an ownership marker.'
    Assert-True (([Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($foreignExecutable))) -eq 'foreign') `
        'Installer overwrote a foreign executable.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $foreignRoot '.idleharbor-managed.json'))) `
        'Installer marked a foreign destination as owned.'

    $archive = & (Join-Path $packagingRoot 'New-ReleasePackage.ps1') `
        -BuildDirectory $buildRoot `
        -OutputDirectory $outputRoot `
        -Version '0.1.0-test' `
        -Architecture x64 `
        -SourceRevision 'test-revision'
    Assert-True (Test-Path -LiteralPath ([string]$archive) -PathType Leaf) 'Portable archive was not created.'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead([string]$archive)
    try {
        $entryNames = @($zip.Entries | ForEach-Object FullName)
        Assert-True ($entryNames -contains 'IdleHarbor-0.1.0-test-windows-x64-portable/IdleHarbor.exe') 'Archive lacks the executable.'
        Assert-True ($entryNames -contains 'IdleHarbor-0.1.0-test-windows-x64-portable/install.ps1') 'Archive lacks the installer.'
        Assert-True ($entryNames -contains 'IdleHarbor-0.1.0-test-windows-x64-portable/DISTRIBUTION.md') 'Archive lacks the distribution guide.'
        Assert-True ($entryNames -notcontains 'IdleHarbor-0.1.0-test-windows-x64-portable/SHA256SUMS.txt') `
            'Archive unexpectedly owns the release-directory checksum manifest.'
        Assert-True (@($entryNames | Where-Object { $_ -match '\.spdx\.json$' }).Count -eq 0) `
            'Archive unexpectedly owns a release-directory SPDX asset.'
        $manifestEntry = $zip.GetEntry('IdleHarbor-0.1.0-test-windows-x64-portable/package-manifest.json')
        Assert-True ($null -ne $manifestEntry) 'Archive lacks its package manifest.'
        $reader = New-Object IO.StreamReader($manifestEntry.Open())
        try { $packageManifest = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
        $manifestProperties = @($packageManifest.PSObject.Properties.Name)
        Assert-True ($manifestProperties -notcontains 'builtFrom') 'Package manifest contains a legacy local builtFrom path.'
        Assert-True ($manifestProperties -contains 'sourceRevision') 'Package manifest lacks its supplied source revision.'
        Assert-True ($packageManifest.sourceRevision -eq 'test-revision') 'Package manifest source revision is incorrect.'
        $manifestJson = $packageManifest | ConvertTo-Json -Depth 5 -Compress
        Assert-True (-not ($manifestJson -match '([A-Za-z]:\\|/Users/|/home/)')) 'Package manifest contains a local path.'
    }
    finally {
        $zip.Dispose()
    }

    $sbom = Join-Path $outputRoot 'test.spdx.json'
    & (Join-Path $packagingRoot 'New-Sbom.ps1') -InputPath $fakeExecutable -OutputPath $sbom -Version '0.1.0-test' -Architecture x64 | Out-Null
    $sbomDocument = Get-Content -Raw -LiteralPath $sbom | ConvertFrom-Json
    Assert-True ($sbomDocument.spdxVersion -eq 'SPDX-2.3') 'SBOM is not SPDX 2.3.'
    Assert-True ($sbomDocument.packages[0].filesAnalyzed -eq $true) 'SBOM package must declare filesAnalyzed=true.'
    $fileSha1 = (Get-FileHash -LiteralPath $fakeExecutable -Algorithm SHA1).Hash.ToLowerInvariant()
    $sha1Algorithm = [Security.Cryptography.SHA1]::Create()
    try {
        $expectedVerificationCode = [BitConverter]::ToString(
            $sha1Algorithm.ComputeHash([Text.Encoding]::ASCII.GetBytes($fileSha1))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha1Algorithm.Dispose()
    }
    $sha1Checksum = @($sbomDocument.files[0].checksums | Where-Object algorithm -eq 'SHA1')
    $sha256Checksum = @($sbomDocument.files[0].checksums | Where-Object algorithm -eq 'SHA256')
    Assert-True ($sha1Checksum.Count -eq 1 -and $sha1Checksum[0].checksum -eq $fileSha1) `
        'SBOM lacks the executable SHA-1 file checksum.'
    Assert-True ($sha256Checksum.Count -eq 1) 'SBOM lacks a SHA-256 file checksum.'
    Assert-True ($sbomDocument.packages[0].packageVerificationCode.value -eq $expectedVerificationCode) `
        'SBOM package verification code is not the SPDX 2.3 SHA-1 code.'

    & (Join-Path $packagingRoot 'Test-ReleaseVersion.ps1') -Tag 'v0.1.0' | Out-Null
    $versionMismatchRejected = $false
    try { & (Join-Path $packagingRoot 'Test-ReleaseVersion.ps1') -Tag 'v0.1.1' | Out-Null }
    catch { $versionMismatchRejected = $true }
    Assert-True $versionMismatchRejected 'Release version validator accepted a mismatched tag.'

    $versionFixture = Join-Path $tempRoot 'version-fixture'
    New-Item -ItemType Directory -Path (Join-Path $versionFixture 'include\idleharbor'), (Join-Path $versionFixture 'resources') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path (Split-Path -Parent $packagingRoot) 'CMakeLists.txt') -Destination $versionFixture
    Copy-Item -LiteralPath (Join-Path (Split-Path -Parent $packagingRoot) 'include\idleharbor\version.hpp') -Destination (Join-Path $versionFixture 'include\idleharbor')
    Copy-Item -LiteralPath (Join-Path (Split-Path -Parent $packagingRoot) 'resources\IdleHarbor.rc') -Destination (Join-Path $versionFixture 'resources')
    $fixtureManifest = Join-Path $versionFixture 'resources\app.manifest'
    (Get-Content -Raw -LiteralPath (Join-Path (Split-Path -Parent $packagingRoot) 'resources\app.manifest')) -replace 'version="0\.1\.0\.0"', 'version="9.9.9.9"' |
        Set-Content -LiteralPath $fixtureManifest -Encoding UTF8
    $manifestMismatchRejected = $false
    try { & (Join-Path $packagingRoot 'Test-ReleaseVersion.ps1') -Tag 'v0.1.0' -SourceRoot $versionFixture | Out-Null }
    catch { $manifestMismatchRejected = $true }
    Assert-True $manifestMismatchRejected 'Release version validator accepted a mismatched app.manifest version.'

    $licenseScript = Join-Path $packagingRoot 'Test-ReleaseLicense.ps1'
    $licenseFixture = Join-Path $tempRoot 'license-fixture'
    $missingLicenseFixture = Join-Path $tempRoot 'missing-license-fixture'
    foreach ($fixture in @($licenseFixture, $missingLicenseFixture)) {
        New-Item -ItemType Directory -Path $fixture -Force | Out-Null
        & git -C $fixture init --quiet 2>$null | Out-Null
    }
    Set-Content -LiteralPath (Join-Path $licenseFixture 'LICENSE') -Value 'fixture licence'
    & git -C $licenseFixture add -- LICENSE | Out-Null
    & $licenseScript -RepositoryRoot $licenseFixture | Out-Null
    $missingLicenseRejected = $false
    try { & $licenseScript -RepositoryRoot $missingLicenseFixture | Out-Null }
    catch { $missingLicenseRejected = $true }
    Assert-True $missingLicenseRejected 'Release licence guard accepted a repository without a tracked LICENSE.'

    $checksums = Join-Path $outputRoot 'SHA256SUMS.txt'
    & (Join-Path $packagingRoot 'New-Checksums.ps1') -InputDirectory $outputRoot -OutputPath $checksums | Out-Null
    Assert-True ((Get-Content -LiteralPath $checksums).Count -ge 2) 'Checksum manifest did not include release files.'
    Write-Output 'Packaging checks passed.'
}
finally {
    if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}
