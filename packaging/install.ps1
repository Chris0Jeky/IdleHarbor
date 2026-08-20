[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param(
    [Parameter(Position = 0)]
    [string]$SourcePath = $PSScriptRoot,

    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'Programs\IdleHarbor'),

    [ValidateSet('TaskScheduler', 'StartupFolder', 'RunKey', 'None')]
    [string]$Startup = 'None',

    [switch]$NoLaunch,

    [ValidateSet('AfterCopy', 'AfterMarker', 'AfterStartupRemoval', 'AfterStartup')]
    [string]$InjectFailureAt,

    # Test-only deterministic rollback fault injection; normal release invocations leave it unset.
    [ValidateSet('Files', 'Startup')]
    [string]$InjectRestoreFailureAt
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProductName = 'IdleHarbor'
$TaskPath = '\IdleHarbor\'
$TaskName = 'IdleHarbor'
$RunValueName = 'IdleHarbor'
$MarkerName = '.idleharbor-managed.json'
$KnownFiles = @(
    'IdleHarbor.exe',
    'install.ps1',
    'uninstall.ps1',
    'install.cmd',
    'README.md',
    'DISTRIBUTION.md',
    'LICENSE'
)

if (-not ('IdleHarbor.Packaging.FileIdentity' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace IdleHarbor.Packaging
{
    public static class FileIdentity
    {
        [StructLayout(LayoutKind.Sequential)]
        private struct ByHandleFileInformation
        {
            public uint FileAttributes;
            public System.Runtime.InteropServices.ComTypes.FILETIME CreationTime;
            public System.Runtime.InteropServices.ComTypes.FILETIME LastAccessTime;
            public System.Runtime.InteropServices.ComTypes.FILETIME LastWriteTime;
            public uint VolumeSerialNumber;
            public uint FileSizeHigh;
            public uint FileSizeLow;
            public uint NumberOfLinks;
            public uint FileIndexHigh;
            public uint FileIndexLow;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetFileInformationByHandle(
            SafeFileHandle file,
            out ByHandleFileInformation information);

        public static uint GetLinkCount(string path)
        {
            using (FileStream stream = new FileStream(
                path,
                FileMode.Open,
                FileAccess.Read,
                FileShare.ReadWrite | FileShare.Delete))
            {
                ByHandleFileInformation information;
                if (!GetFileInformationByHandle(stream.SafeFileHandle, out information))
                {
                    throw new Win32Exception(Marshal.GetLastWin32Error());
                }
                return information.NumberOfLinks;
            }
        }
    }
}
'@
}

function Confirm-Change {
    [CmdletBinding(SupportsShouldProcess = $true)]
    param(
        [Parameter(Mandatory)]
        [string]$Target,

        [Parameter(Mandatory)]
        [string]$Action
    )

    return $PSCmdlet.ShouldProcess($Target, $Action)
}

function Get-FullPath([string]$Path) {
    return [System.IO.Path]::GetFullPath($Path)
}

function Assert-SafeInstallRoot([string]$Path) {
    $full = Get-FullPath $Path
    $trimmed = $full.TrimEnd('\')
    if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed -eq [System.IO.Path]::GetPathRoot($full).TrimEnd('\')) {
        throw "InstallRoot must be a product directory, not a drive or filesystem root."
    }
    if ((Split-Path -Leaf $trimmed) -ne $ProductName) {
        throw "InstallRoot must end in '$ProductName' so the installer can prove its ownership boundary."
    }
    $item = Get-Item -LiteralPath $trimmed -ErrorAction SilentlyContinue
    if ($null -ne $item -and (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)) {
        throw "InstallRoot may not be a junction or symbolic link: $full"
    }
    return $full
}

function Get-SourceRoot([string]$Path) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if ((Get-Item -LiteralPath $resolved).PSIsContainer) {
        return $resolved
    }
    return Split-Path -Parent $resolved
}

function Get-ExecutablePath([string]$Root) {
    $path = Join-Path $Root 'IdleHarbor.exe'
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "The source package does not contain IdleHarbor.exe: $Root"
    }
    return (Get-FullPath $path)
}

function Test-SamePath([string]$Left, [string]$Right) {
    try { return ((Get-FullPath $Left).TrimEnd('\') -ieq (Get-FullPath $Right).TrimEnd('\')) }
    catch { return $false }
}

function Get-OwnedProcesses([string]$Executable) {
    $matches = @()
    foreach ($process in @(Get-Process -Name 'IdleHarbor' -ErrorAction SilentlyContinue)) {
        try {
            if (Test-SamePath $process.Path $Executable) { $matches += $process }
        }
        catch { }
    }
    return $matches
}

function Stop-OwnedApplicationIfRunning([string]$Executable) {
    $running = @(Get-OwnedProcesses $Executable)
    if ($running.Count -eq 0) { return }
    if (-not (Confirm-Change $Executable 'Ask the running IdleHarbor instance to exit for update')) { return }

    Start-Process -FilePath $Executable -ArgumentList '--exit' -WindowStyle Hidden -Wait
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        Start-Sleep -Milliseconds 100
        $running = @(Get-OwnedProcesses $Executable)
    } while ($running.Count -ne 0 -and [DateTime]::UtcNow -lt $deadline)
    if ($running.Count -ne 0) {
        throw 'The running IdleHarbor instance did not exit; no installed files were overwritten.'
    }
}

function Assert-OwnedOrEmptyDestination([string]$Root, [string]$SourceRoot) {
    $markerPath = Join-Path $Root $MarkerName
    if ((Test-SamePath $Root $SourceRoot) -and
        -not (Test-Path -LiteralPath $markerPath -PathType Leaf)) {
        throw "SourcePath and InstallRoot are the same directory but have no valid IdleHarbor ownership marker; refusing to continue: $markerPath"
    }
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) { return }

    if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) {
        $entries = @(Get-ChildItem -LiteralPath $Root -Force)
        if ($entries.Count -ne 0) {
            throw "InstallRoot contains files but no IdleHarbor ownership marker; refusing to overwrite it: $Root"
        }
        return
    }

    try { $marker = Get-Content -Raw -LiteralPath $markerPath | ConvertFrom-Json }
    catch { throw "InstallRoot has an unreadable ownership marker; refusing to overwrite it: $markerPath" }
    if ($marker.product -ne $ProductName -or -not (Test-SamePath ([string]$marker.installRoot) $Root)) {
        throw "InstallRoot ownership marker does not match this destination: $markerPath"
    }
    $ownedExecutable = Get-FullPath ([string]$marker.executable)
    if (-not (Test-SamePath (Split-Path -Parent $ownedExecutable) $Root)) {
        throw "InstallRoot ownership marker points outside the destination: $markerPath"
    }
}

function Get-StartupLinkPath() {
    return Join-Path ([Environment]::GetFolderPath('Startup')) 'IdleHarbor.lnk'
}

function Assert-SafeManagedFile([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return }
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Managed install path is not a regular file: $Path"
    }
    $item = Get-Item -LiteralPath $Path -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Managed install path may not be a junction or symbolic link: $Path"
    }
    $linkCount = [IdleHarbor.Packaging.FileIdentity]::GetLinkCount($item.FullName)
    if ($linkCount -ne 1) {
        throw "Managed install path has $linkCount hard links; refusing to cross the install boundary: $Path"
    }
}

function Test-TaskFolderExists {
    $service = New-Object -ComObject 'Schedule.Service'
    $service.Connect()
    try {
        $null = $service.GetFolder($TaskPath.TrimEnd('\'))
        return $true
    }
    catch {
        if ($_.Exception.HResult -eq -2147024894) { return $false }
        throw
    }
}

function Remove-EmptyOwnedTaskFolder {
    [CmdletBinding(SupportsShouldProcess = $true)]
    param(
        [Parameter(Mandatory)]
        [bool]$Owned
    )

    if (-not $Owned) { return $false }
    $service = New-Object -ComObject 'Schedule.Service'
    $service.Connect()
    try {
        $folder = $service.GetFolder($TaskPath.TrimEnd('\'))
    }
    catch {
        if ($_.Exception.HResult -eq -2147024894) { return $true }
        throw
    }

    if ($folder.GetTasks(0).Count -ne 0 -or $folder.GetFolders(0).Count -ne 0) {
        return $false
    }
    if ($PSCmdlet.ShouldProcess("scheduled task folder $TaskPath", 'Remove empty installer-owned folder')) {
        $service.GetFolder('\').DeleteFolder($TaskPath.Trim('\'), 0)
        return $true
    }
    return $false
}

function Get-RunCommand() {
    $key = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
    $property = Get-ItemProperty -Path $key -Name $RunValueName -ErrorAction SilentlyContinue
    if ($null -eq $property) { return $null }
    return [string]$property.$RunValueName
}

function Get-ExpectedRunCommand([string]$Executable) {
    return '"{0}" --start --minimized' -f $Executable
}

function Test-RunCommandOwned([string]$Command, [string]$Executable) {
    if ([string]::IsNullOrWhiteSpace($Command)) { return $false }
    return ($Command -ieq (Get-ExpectedRunCommand $Executable))
}

function Test-TaskActionsOwned([object[]]$Actions, [string]$Executable) {
    if ($Actions.Count -ne 1) { return $false }
    $action = $Actions[0]
    return (
        (Test-SamePath ([string]$action.Execute) $Executable) -and
        ([string]$action.Arguments -ceq '--start --minimized') -and
        (Test-SamePath ([string]$action.WorkingDirectory) (Split-Path -Parent $Executable))
    )
}

function Test-StartupShortcutOwned([object]$Shortcut, [string]$Executable) {
    return (
        (Test-SamePath ([string]$Shortcut.TargetPath) $Executable) -and
        ([string]$Shortcut.Arguments -ceq '--start --minimized') -and
        (Test-SamePath ([string]$Shortcut.WorkingDirectory) (Split-Path -Parent $Executable))
    )
}

function Get-OwnedTask([string]$Executable) {
    $task = Get-ScheduledTask -TaskPath $TaskPath -TaskName $TaskName -ErrorAction SilentlyContinue
    if ($null -eq $task) { return $null }
    if (-not (Test-TaskActionsOwned -Actions @($task.Actions) -Executable $Executable)) {
        throw "A different scheduled task already owns $TaskPath$TaskName; refusing to overwrite it."
    }
    return $task
}

function Assert-StartupEntriesOwned([string]$Executable) {
    $null = Get-OwnedTask $Executable

    $link = Get-StartupLinkPath
    if (Test-Path -LiteralPath $link -PathType Leaf) {
        $shell = New-Object -ComObject WScript.Shell
        $shortcut = $shell.CreateShortcut($link)
        if (-not (Test-StartupShortcutOwned -Shortcut $shortcut -Executable $Executable)) {
            throw "A different startup shortcut already owns ${link}; refusing to overwrite it."
        }
    }

    $command = Get-RunCommand
    if ($null -ne $command -and -not (Test-RunCommandOwned $command $Executable)) {
        throw "A different HKCU Run value already owns ${RunValueName}; refusing to overwrite it."
    }
}

function Remove-OwnedStartup {
    [CmdletBinding(SupportsShouldProcess = $true)]
    param(
        [Parameter(Mandatory)]
        [string]$Executable
    )

    $task = Get-ScheduledTask -TaskPath $TaskPath -TaskName $TaskName -ErrorAction SilentlyContinue
    if ($null -ne $task -and (Test-TaskActionsOwned -Actions @($task.Actions) -Executable $Executable)) {
        if ($PSCmdlet.ShouldProcess("scheduled task $TaskPath$TaskName", 'Remove')) {
            Unregister-ScheduledTask -TaskPath $TaskPath -TaskName $TaskName -Confirm:$false
        }
    }

    $link = Get-StartupLinkPath
    if (Test-Path -LiteralPath $link -PathType Leaf) {
        $shell = New-Object -ComObject WScript.Shell
        $shortcut = $shell.CreateShortcut($link)
        if (Test-StartupShortcutOwned -Shortcut $shortcut -Executable $Executable) {
            if ($PSCmdlet.ShouldProcess($link, 'Remove startup shortcut')) {
                Remove-Item -LiteralPath $link -Force
            }
        }
    }

    $command = Get-RunCommand
    if (Test-RunCommandOwned $command $Executable) {
        if ($PSCmdlet.ShouldProcess("HKCU Run value $RunValueName", 'Remove')) {
            Remove-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name $RunValueName -Force
        }
    }
}

function Set-Startup {
    [CmdletBinding(SupportsShouldProcess = $true)]
    param(
        [Parameter(Mandatory)]
        [string]$Executable,

        [Parameter(Mandatory)]
        [string]$Mode
    )

    Assert-StartupEntriesOwned $Executable
    Remove-OwnedStartup $Executable
    Invoke-InjectedFailure 'AfterStartupRemoval'
    if ($Mode -eq 'None') {
        Invoke-InjectedFailure 'AfterStartup'
        return
    }

    switch ($Mode) {
        'TaskScheduler' {
            $null = Get-OwnedTask $Executable
            $action = New-ScheduledTaskAction -Execute $Executable -Argument '--start --minimized' -WorkingDirectory (Split-Path -Parent $Executable)
            $trigger = New-ScheduledTaskTrigger -AtLogOn -User "$env:USERDOMAIN\$env:USERNAME"
            $principal = New-ScheduledTaskPrincipal -UserId "$env:USERDOMAIN\$env:USERNAME" -LogonType Interactive -RunLevel Limited
            $settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
            if ($PSCmdlet.ShouldProcess("scheduled task $TaskPath$TaskName", 'Register')) {
                Register-ScheduledTask -TaskPath $TaskPath -TaskName $TaskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'Starts IdleHarbor for the signed-in user.' -Force | Out-Null
            }
        }
        'StartupFolder' {
            $link = Get-StartupLinkPath
            if ($PSCmdlet.ShouldProcess($link, 'Create startup shortcut')) {
                $shell = New-Object -ComObject WScript.Shell
                $shortcut = $shell.CreateShortcut($link)
                $shortcut.TargetPath = $Executable
                $shortcut.Arguments = '--start --minimized'
                $shortcut.WorkingDirectory = Split-Path -Parent $Executable
                $shortcut.Description = 'Starts IdleHarbor for the signed-in user.'
                $shortcut.Save()
            }
        }
        'RunKey' {
            $command = Get-ExpectedRunCommand $Executable
            if ($PSCmdlet.ShouldProcess("HKCU Run value $RunValueName", 'Set')) {
                New-Item -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Force | Out-Null
                Set-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name $RunValueName -Value $command -Type String
            }
        }
    }
    Invoke-InjectedFailure 'AfterStartup'
}

function Invoke-InjectedFailure([string]$Location) {
    if (-not $WhatIfPreference -and $InjectFailureAt -eq $Location) {
        throw "Injected installer failure at $Location."
    }
}

function Invoke-InjectedRestoreFailure([string]$Location) {
    if (-not $WhatIfPreference -and $InjectRestoreFailureAt -eq $Location) {
        throw "Injected installer rollback failure at $Location."
    }
}

function Remove-TransactionBackup([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path -PathType Container)) {
        return
    }

    $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not $fullPath.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $fullPath) -notlike 'IdleHarbor-install-transaction-*') {
        throw "Refusing to remove an invalid installer transaction directory: $fullPath"
    }
    Remove-Item -LiteralPath $fullPath -Recurse -Force
}

function New-InstallSnapshot {
    param(
        [Parameter(Mandatory)]
        [string]$InstallRoot,

        [Parameter(Mandatory)]
        [string[]]$RelativeFiles
    )

    $backupRoot = Join-Path ([IO.Path]::GetTempPath()) "IdleHarbor-install-transaction-$([Guid]::NewGuid().ToString('N'))"
    New-Item -ItemType Directory -Path $backupRoot | Out-Null
    try {
        $entries = @()
        foreach ($relativeFile in @($RelativeFiles | Select-Object -Unique)) {
            $destination = Join-Path $InstallRoot $relativeFile
            $exists = Test-Path -LiteralPath $destination
            if ($exists) { Assert-SafeManagedFile $destination }

            $backup = Join-Path $backupRoot $relativeFile
            if ($exists) {
                Copy-Item -LiteralPath $destination -Destination $backup
            }
            $entries += [pscustomobject]@{
                relativeFile = $relativeFile
                destination = $destination
                existed = $exists
                backup = $backup
            }
        }

        return [pscustomobject]@{
            backupRoot = $backupRoot
            installRootExisted = (Test-Path -LiteralPath $InstallRoot -PathType Container)
            entries = $entries
        }
    }
    catch {
        Remove-TransactionBackup $backupRoot
        throw
    }
}

function Restore-InstallSnapshot {
    param(
        [Parameter(Mandatory)]
        [object]$Snapshot,

        [Parameter(Mandatory)]
        [string]$InstallRoot
    )

    Invoke-InjectedRestoreFailure 'Files'
    foreach ($entry in @($Snapshot.entries)) {
        if ($entry.existed) {
            if (-not (Test-Path -LiteralPath $InstallRoot -PathType Container)) {
                New-Item -ItemType Directory -Path $InstallRoot | Out-Null
            }
            if (Test-Path -LiteralPath ([string]$entry.destination)) {
                Assert-SafeManagedFile ([string]$entry.destination)
            }
            Copy-Item -LiteralPath ([string]$entry.backup) -Destination ([string]$entry.destination) -Force
        }
        elseif (Test-Path -LiteralPath ([string]$entry.destination) -PathType Leaf) {
            Remove-Item -LiteralPath ([string]$entry.destination) -Force
        }
        elseif (Test-Path -LiteralPath ([string]$entry.destination)) {
            throw "Rollback found a non-file at a managed path: $($entry.destination)"
        }
    }

    if (-not $Snapshot.installRootExisted -and (Test-Path -LiteralPath $InstallRoot -PathType Container)) {
        $rootItem = Get-Item -LiteralPath $InstallRoot
        if (($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Rollback will not remove a reparse-point install root: $InstallRoot"
        }
        $remaining = @(Get-ChildItem -LiteralPath $InstallRoot -Force)
        if ($remaining.Count -eq 0) {
            Remove-Item -LiteralPath $InstallRoot -Force
        }
    }
}

function Get-StartupSnapshot {
    param(
        [Parameter(Mandatory)]
        [string]$Executable,

        [Parameter(Mandatory)]
        [bool]$CaptureTaskFolder
    )

    $taskXml = $null
    $task = Get-OwnedTask $Executable
    if ($null -ne $task) {
        $taskXml = [string](Export-ScheduledTask -TaskPath $TaskPath -TaskName $TaskName)
    }

    $shortcutBytes = $null
    $link = Get-StartupLinkPath
    if (Test-Path -LiteralPath $link -PathType Leaf) {
        $shortcutBytes = [IO.File]::ReadAllBytes($link)
    }

    $runCommand = Get-RunCommand
    $runKind = $null
    if ($null -ne $runCommand) {
        $runKey = Get-Item -LiteralPath 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
        $runKind = $runKey.GetValueKind($RunValueName)
    }

    return [pscustomobject]@{
        taskXml = $taskXml
        taskFolderExisted = $(if ($CaptureTaskFolder) { Test-TaskFolderExists } else { $true })
        shortcutBytes = $shortcutBytes
        runCommand = $runCommand
        runKind = $runKind
    }
}

function Restore-StartupSnapshot {
    param(
        [Parameter(Mandatory)]
        [object]$Snapshot,

        [Parameter(Mandatory)]
        [string]$Executable
    )

    Invoke-InjectedRestoreFailure 'Startup'
    Assert-StartupEntriesOwned $Executable
    Remove-OwnedStartup -Executable $Executable -Confirm:$false

    if ($null -ne $Snapshot.taskXml) {
        Register-ScheduledTask -TaskPath $TaskPath -TaskName $TaskName -Xml ([string]$Snapshot.taskXml) -Force | Out-Null
    }
    if ($null -ne $Snapshot.shortcutBytes) {
        $link = Get-StartupLinkPath
        New-Item -ItemType Directory -Path (Split-Path -Parent $link) -Force | Out-Null
        [IO.File]::WriteAllBytes($link, [byte[]]$Snapshot.shortcutBytes)
    }
    if ($null -ne $Snapshot.runCommand) {
        $key = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
        New-Item -Path $key -Force | Out-Null
        Set-ItemProperty `
            -Path $key `
            -Name $RunValueName `
            -Value ([string]$Snapshot.runCommand) `
            -Type ([Microsoft.Win32.RegistryValueKind]$Snapshot.runKind)
    }
    if (-not $Snapshot.taskFolderExisted) {
        $folderRemoved = Remove-EmptyOwnedTaskFolder -Owned $true -Confirm:$false
        if (-not $folderRemoved -and (Test-TaskFolderExists)) {
            Write-Warning "Preserved non-empty Task Scheduler folder after rollback: $TaskPath"
        }
    }
}

$sourceRoot = Get-SourceRoot $SourcePath
$sourceExecutable = Get-ExecutablePath $sourceRoot
$safeRoot = Assert-SafeInstallRoot $InstallRoot
$destinationExecutable = Join-Path $safeRoot 'IdleHarbor.exe'
$parent = Split-Path -Parent $safeRoot
$markerPath = Join-Path $safeRoot $MarkerName
Assert-OwnedOrEmptyDestination $safeRoot $sourceRoot
Assert-StartupEntriesOwned $destinationExecutable

$sourceIsInstallRoot = Test-SamePath $sourceRoot $safeRoot
$installSnapshot = $null
$startupSnapshot = $null
$previousTaskFolderOwned = $false
$retainTransactionBackup = $false
if (Test-Path -LiteralPath $markerPath -PathType Leaf) {
    $existingMarker = Get-Content -Raw -LiteralPath $markerPath | ConvertFrom-Json
    if (@($existingMarker.PSObject.Properties.Name) -contains 'taskFolderOwned') {
        $previousTaskFolderOwned = ($existingMarker.taskFolderOwned -eq $true)
    }
}

if (-not (Test-SamePath $sourceRoot $safeRoot) -and (Test-Path -LiteralPath $destinationExecutable -PathType Leaf)) {
    Stop-OwnedApplicationIfRunning $destinationExecutable
}

if (-not $WhatIfPreference) {
    $captureTaskFolder = ($Startup -eq 'TaskScheduler' -or $previousTaskFolderOwned)
    $startupSnapshot = Get-StartupSnapshot -Executable $destinationExecutable -CaptureTaskFolder $captureTaskFolder
    $installSnapshot = New-InstallSnapshot -InstallRoot $safeRoot -RelativeFiles @($KnownFiles + $MarkerName)
}

try {
    if (-not $sourceIsInstallRoot) {
        if (Confirm-Change $safeRoot 'Create installation directory') {
            New-Item -ItemType Directory -Path $safeRoot -Force | Out-Null
        }
    }

    foreach ($fileName in $KnownFiles) {
        if ($sourceIsInstallRoot) { continue }
        $sourceFile = Join-Path $sourceRoot $fileName
        if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) { continue }
        $destinationFile = Join-Path $safeRoot $fileName
        if (Confirm-Change $destinationFile 'Install package file') {
            if (Test-Path -LiteralPath $destinationFile) { Assert-SafeManagedFile $destinationFile }
            Copy-Item -LiteralPath $sourceFile -Destination $destinationFile -Force
            Invoke-InjectedFailure 'AfterCopy'
        }
    }

    Set-Startup $destinationExecutable $Startup

    $taskFolderOwned = $previousTaskFolderOwned
    if (-not $WhatIfPreference) {
        if ($Startup -eq 'TaskScheduler' -and -not $startupSnapshot.taskFolderExisted) {
            $taskFolderOwned = $true
        }
        elseif ($Startup -ne 'TaskScheduler' -and $taskFolderOwned) {
            if (Remove-EmptyOwnedTaskFolder -Owned $true -Confirm:$false) {
                $taskFolderOwned = $false
            }
        }
    }

    if (Confirm-Change $markerPath 'Write ownership marker') {
        if (Test-Path -LiteralPath $markerPath) { Assert-SafeManagedFile $markerPath }
        $installedFiles = @($KnownFiles | Where-Object { Test-Path -LiteralPath (Join-Path $safeRoot $_) -PathType Leaf })
        $marker = [ordered]@{
            product = $ProductName
            installRoot = $safeRoot
            executable = $destinationExecutable
            managedFiles = $installedFiles
            taskFolderOwned = $taskFolderOwned
            installedAtUtc = [DateTime]::UtcNow.ToString('o')
        }
        $marker | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $markerPath -Encoding UTF8
        Invoke-InjectedFailure 'AfterMarker'
    }
}
catch {
    $failure = $_
    $rollbackErrors = @()
    if ($null -ne $installSnapshot) {
        try {
            Restore-InstallSnapshot -Snapshot $installSnapshot -InstallRoot $safeRoot
        }
        catch {
            $retainTransactionBackup = $true
            $rollbackErrors += "files: $($_.Exception.Message)"
        }
    }
    if ($null -ne $startupSnapshot) {
        try {
            Restore-StartupSnapshot -Snapshot $startupSnapshot -Executable $destinationExecutable
        }
        catch {
            $rollbackErrors += "startup: $($_.Exception.Message)"
        }
    }
    if ($rollbackErrors.Count -ne 0) {
        $message = "$($failure.Exception.Message) Rollback was incomplete ($($rollbackErrors -join '; '))."
        if ($retainTransactionBackup) {
            $message += " Recovery backup retained at: $([string]$installSnapshot.backupRoot)."
        }
        throw $message
    }
    throw $failure
}
finally {
    if ($null -ne $installSnapshot) {
        try {
            if (-not $retainTransactionBackup) {
                Remove-TransactionBackup ([string]$installSnapshot.backupRoot)
            }
        }
        catch {
            Write-Warning "Installer transaction backup cleanup failed: $($_.Exception.Message)"
        }
    }
}

Write-Output "Installed $ProductName to $safeRoot"
Write-Output "Startup mode: $Startup"
if (-not $NoLaunch -and -not $WhatIfPreference -and (Test-Path -LiteralPath $destinationExecutable -PathType Leaf)) {
    Start-Process -FilePath $destinationExecutable -ArgumentList '--start', '--minimized' -WorkingDirectory $safeRoot
}
