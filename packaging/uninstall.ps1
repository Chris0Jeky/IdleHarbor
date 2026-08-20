[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'High')]
param(
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'Programs\IdleHarbor'),

    [switch]$PurgeData
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProductName = 'IdleHarbor'
$TaskPath = '\IdleHarbor\'
$TaskName = 'IdleHarbor'
$RunValueName = 'IdleHarbor'
$MarkerName = '.idleharbor-managed.json'
$DataMarkerName = '.idleharbor-data.json'

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
        throw "InstallRoot must end in '$ProductName'."
    }
    $item = Get-Item -LiteralPath $trimmed -ErrorAction SilentlyContinue
    if ($null -ne $item -and (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)) {
        throw "InstallRoot may not be a junction or symbolic link: $full"
    }
    return $full
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
    if (-not (Confirm-Change $Executable 'Ask the running IdleHarbor instance to exit for uninstall')) { return }

    Start-Process -FilePath $Executable -ArgumentList '--exit' -WindowStyle Hidden -Wait
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        Start-Sleep -Milliseconds 100
        $running = @(Get-OwnedProcesses $Executable)
    } while ($running.Count -ne 0 -and [DateTime]::UtcNow -lt $deadline)
    if ($running.Count -ne 0) {
        throw 'The running IdleHarbor instance did not exit; no installed files were removed.'
    }
}

function Get-StartupLinkPath() {
    return Join-Path ([Environment]::GetFolderPath('Startup')) 'IdleHarbor.lnk'
}

function Get-StartMenuLinkPath() {
    return Join-Path ([Environment]::GetFolderPath('Programs')) 'IdleHarbor.lnk'
}

function Assert-SafeShortcutFile([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
    if ($null -eq $item) { return }
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Managed shortcut may not be a junction or symbolic link: $Path"
    }
    if ($item.PSIsContainer -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Managed shortcut is not a regular file: $Path"
    }
    $linkCount = [IdleHarbor.Packaging.FileIdentity]::GetLinkCount($item.FullName)
    if ($linkCount -ne 1) {
        throw "Managed shortcut has $linkCount hard links; refusing to cross the user profile boundary: $Path"
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
    $property = Get-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name $RunValueName -ErrorAction SilentlyContinue
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

function Test-StartMenuShortcutOwned([object]$Shortcut, [string]$Executable) {
    return (
        (Test-SamePath ([string]$Shortcut.TargetPath) $Executable) -and
        ([string]$Shortcut.Arguments -ceq '--show') -and
        (Test-SamePath ([string]$Shortcut.WorkingDirectory) (Split-Path -Parent $Executable)) -and
        ([string]$Shortcut.Description -ceq 'Shows IdleHarbor settings.') -and
        ([string]$Shortcut.IconLocation -ieq ('{0},0' -f $Executable))
    )
}

function Assert-StartMenuPreflight {
    param(
        [Parameter(Mandatory)]
        [string]$Executable,

        [Parameter(Mandatory)]
        [bool]$MarkerOwned,

        [string]$Link = (Get-StartMenuLinkPath)
    )

    $linkItem = Get-Item -LiteralPath $link -Force -ErrorAction SilentlyContinue
    if ($null -eq $linkItem) { return }
    try {
        Assert-SafeShortcutFile $link
    }
    catch {
        Write-Warning "Preserving unsafe or foreign Start Menu path: $link ($($_.Exception.Message))"
        return
    }
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($link)
    $exact = Test-StartMenuShortcutOwned -Shortcut $shortcut -Executable $Executable
    if ($MarkerOwned -and -not $exact) {
        Write-Warning "Preserving changed or foreign Start Menu shortcut and relinquishing installer ownership: $link"
    }
    elseif (-not $MarkerOwned -and $exact) {
        Write-Warning "Preserving an exact Start Menu shortcut that this installer did not create: $link"
    }
    elseif (-not $exact) {
        Write-Warning "Preserving foreign Start Menu shortcut: $link"
    }
}

function Get-OwnedDataMarker([string]$DataRoot) {
    $markerPath = Join-Path $DataRoot $DataMarkerName
    if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) { return $null }
    try { $marker = Get-Content -Raw -LiteralPath $markerPath | ConvertFrom-Json }
    catch {
        Write-Warning "Preserving data directory with an unreadable ownership marker: $DataRoot"
        return $null
    }
    $properties = @($marker.PSObject.Properties.Name)
    if ($properties -notcontains 'product' -or $properties -notcontains 'markerVersion' -or
        $properties -notcontains 'settingsFile' -or $marker.product -ne $ProductName -or
        $marker.markerVersion -ne 1 -or $marker.settingsFile -ne 'settings.ini') {
        Write-Warning "Preserving data directory with a non-IdleHarbor ownership marker: $DataRoot"
        return $null
    }
    return $marker
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

function Remove-OwnedStartMenu {
    [CmdletBinding(SupportsShouldProcess = $true)]
    param(
        [Parameter(Mandatory)]
        [string]$Executable,

        [Parameter(Mandatory)]
        [bool]$MarkerOwned,

        [string]$Link = (Get-StartMenuLinkPath)
    )

    $linkItem = Get-Item -LiteralPath $link -Force -ErrorAction SilentlyContinue
    if ($null -eq $linkItem) { return }
    try {
        Assert-SafeShortcutFile $link
    }
    catch {
        Write-Warning "Preserved unsafe or foreign Start Menu path: $link ($($_.Exception.Message))"
        return
    }
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($link)
    if ($MarkerOwned -and (Test-StartMenuShortcutOwned -Shortcut $shortcut -Executable $Executable)) {
        if ($PSCmdlet.ShouldProcess($link, 'Remove installer-owned Start Menu shortcut')) {
            Remove-Item -LiteralPath $link -Force
        }
    }
    elseif ($MarkerOwned) {
        Write-Warning "Preserved changed or foreign Start Menu shortcut: $link"
    }
    elseif (Test-StartMenuShortcutOwned -Shortcut $shortcut -Executable $Executable) {
        Write-Warning "Preserved an exact Start Menu shortcut that this installer did not create: $link"
    }
    else {
        Write-Warning "Preserved foreign Start Menu shortcut: $link"
    }
}

$safeRoot = Assert-SafeInstallRoot $InstallRoot
$markerPath = Join-Path $safeRoot $MarkerName
if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) {
    throw "Ownership marker not found; refusing to remove an unverified directory: $safeRoot"
}

$marker = Get-Content -Raw -LiteralPath $markerPath | ConvertFrom-Json
if ($marker.product -ne $ProductName -or -not (Test-SamePath $marker.installRoot $safeRoot)) {
    throw "Ownership marker does not belong to ${ProductName}: $markerPath"
}
$taskFolderOwned = (
    @($marker.PSObject.Properties.Name) -contains 'taskFolderOwned' -and
    $marker.taskFolderOwned -eq $true
)

$executable = Get-FullPath ([string]$marker.executable)
if (-not (Test-SamePath (Split-Path -Parent $executable) $safeRoot)) {
    throw "Ownership marker points outside the installation directory; refusing to continue."
}
$startMenuLinkOwned = (
    @($marker.PSObject.Properties.Name) -contains 'startMenuLinkOwned' -and
    @($marker.PSObject.Properties.Name) -contains 'startMenuLinkPath' -and
    $marker.startMenuLinkOwned -eq $true -and
    (Test-SamePath ([string]$marker.startMenuLinkPath) (Get-StartMenuLinkPath))
)

Assert-StartMenuPreflight -Executable $executable -MarkerOwned $startMenuLinkOwned
Stop-OwnedApplicationIfRunning $executable
Remove-OwnedStartup $executable
Remove-OwnedStartMenu -Executable $executable -MarkerOwned $startMenuLinkOwned
if ($taskFolderOwned) {
    $folderRemoved = Remove-EmptyOwnedTaskFolder -Owned $true
    if (-not $WhatIfPreference -and -not $folderRemoved -and (Test-TaskFolderExists)) {
        Write-Warning "Preserved non-empty Task Scheduler folder: $TaskPath"
    }
}

foreach ($file in @($marker.managedFiles)) {
    $candidate = Join-Path $safeRoot ([string]$file)
    if (-not (Test-SamePath (Split-Path -Parent $candidate) $safeRoot)) {
        throw "Ownership marker contains a path outside the installation directory: $file"
    }
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        if (Confirm-Change $candidate 'Remove installed file') {
            Remove-Item -LiteralPath $candidate -Force
        }
    }
}

if (Test-Path -LiteralPath $markerPath -PathType Leaf) {
    if (Confirm-Change $markerPath 'Remove ownership marker') {
        Remove-Item -LiteralPath $markerPath -Force
    }
}

if (Test-Path -LiteralPath $safeRoot -PathType Container) {
    $remaining = @(Get-ChildItem -LiteralPath $safeRoot -Force)
    if ($remaining.Count -eq 0) {
        if (Confirm-Change $safeRoot 'Remove empty installation directory') {
            Remove-Item -LiteralPath $safeRoot -Force
        }
    }
    else {
        Write-Warning "Preserved unexpected files in ${safeRoot}: $($remaining.Name -join ', ')"
    }
}

if ($PurgeData) {
    foreach ($dataRoot in @(
        (Join-Path $env:APPDATA $ProductName),
        (Join-Path $env:LOCALAPPDATA $ProductName)
    )) {
        if (Test-Path -LiteralPath $dataRoot) {
            $dataItem = Get-Item -LiteralPath $dataRoot
            if (($dataItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Refusing to purge a reparse-point data directory: $dataRoot"
            }
            $dataMarker = Get-OwnedDataMarker $dataRoot
            if ($null -eq $dataMarker) {
                Write-Warning "Preserved unowned data directory: $dataRoot"
                continue
            }
            if (Confirm-Change $dataRoot 'Purge IdleHarbor-owned user data') {
                Remove-Item -LiteralPath $dataRoot -Recurse -Force
            }
        }
    }
}

Write-Output "Uninstalled $ProductName from $safeRoot"
if (-not $PurgeData) { Write-Output 'User settings were preserved. Use -PurgeData only when removal is intentional.' }
