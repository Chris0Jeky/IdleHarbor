[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param(
    [Parameter(Position = 0)]
    [string]$SourcePath = $PSScriptRoot,

    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'Programs\IdleHarbor'),

    [ValidateSet('TaskScheduler', 'StartupFolder', 'RunKey', 'None')]
    [string]$Startup = 'None',

    [switch]$NoLaunch
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
    'LICENSE',
    'SHA256SUMS.txt',
    'sbom.spdx.json'
)

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
    if (Test-SamePath $Root $SourceRoot) { return }
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) { return }

    $markerPath = Join-Path $Root $MarkerName
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

function Get-RunCommand() {
    $key = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
    $property = Get-ItemProperty -Path $key -Name $RunValueName -ErrorAction SilentlyContinue
    if ($null -eq $property) { return $null }
    return [string]$property.$RunValueName
}

function Test-RunCommandOwned([string]$Command, [string]$Executable) {
    if ([string]::IsNullOrWhiteSpace($Command)) { return $false }
    $candidate = $Command -replace '^\s*"([^"]+)".*$', '$1'
    if ($candidate -eq $Command) { $candidate = ($Command -split '\s+', 2)[0] }
    return (Test-SamePath $candidate $Executable)
}

function Get-OwnedTask([string]$Executable) {
    $task = Get-ScheduledTask -TaskPath $TaskPath -TaskName $TaskName -ErrorAction SilentlyContinue
    if ($null -eq $task) { return $null }
    $action = @($task.Actions) | Select-Object -First 1
    if ($null -eq $action -or -not (Test-SamePath $action.Execute $Executable)) {
        throw "A different scheduled task already owns $TaskPath$TaskName; refusing to overwrite it."
    }
    return $task
}

function Remove-OwnedStartup {
    [CmdletBinding(SupportsShouldProcess = $true)]
    param(
        [Parameter(Mandatory)]
        [string]$Executable
    )

    $task = Get-ScheduledTask -TaskPath $TaskPath -TaskName $TaskName -ErrorAction SilentlyContinue
    if ($null -ne $task) {
        $action = @($task.Actions) | Select-Object -First 1
        if ($null -ne $action -and (Test-SamePath $action.Execute $Executable)) {
            if ($PSCmdlet.ShouldProcess("scheduled task $TaskPath$TaskName", 'Remove')) {
                Unregister-ScheduledTask -TaskPath $TaskPath -TaskName $TaskName -Confirm:$false
            }
        }
    }

    $link = Get-StartupLinkPath
    if (Test-Path -LiteralPath $link -PathType Leaf) {
        $shell = New-Object -ComObject WScript.Shell
        $shortcut = $shell.CreateShortcut($link)
        if (Test-SamePath $shortcut.TargetPath $Executable) {
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

    Remove-OwnedStartup $Executable
    if ($Mode -eq 'None') { return }

    switch ($Mode) {
        'TaskScheduler' {
            $null = Get-OwnedTask $Executable
            $action = New-ScheduledTaskAction -Execute $Executable -Argument '--start --minimized' -WorkingDirectory (Split-Path -Parent $Executable)
            $trigger = New-ScheduledTaskTrigger -AtLogOn -User "$env:USERDOMAIN\$env:USERNAME"
            $principal = New-ScheduledTaskPrincipal -UserId "$env:USERDOMAIN\$env:USERNAME" -LogonType Interactive -RunLevel LeastPrivilege
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
            $command = '"{0}" --start --minimized' -f $Executable
            if ($PSCmdlet.ShouldProcess("HKCU Run value $RunValueName", 'Set')) {
                New-Item -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Force | Out-Null
                Set-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name $RunValueName -Value $command -Type String
            }
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

if (-not (Test-SamePath $sourceRoot $safeRoot) -and (Test-Path -LiteralPath $destinationExecutable -PathType Leaf)) {
    Stop-OwnedApplicationIfRunning $destinationExecutable
}

if (-not (Test-SamePath $sourceRoot $safeRoot)) {
    if (Confirm-Change $safeRoot 'Create installation directory') {
        New-Item -ItemType Directory -Path $safeRoot -Force | Out-Null
    }
}

foreach ($fileName in $KnownFiles) {
    if (Test-SamePath $sourceRoot $safeRoot) { continue }
    $sourceFile = Join-Path $sourceRoot $fileName
    if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) { continue }
    $destinationFile = Join-Path $safeRoot $fileName
    if (Confirm-Change $destinationFile 'Install package file') {
        Copy-Item -LiteralPath $sourceFile -Destination $destinationFile -Force
    }
}

if (Confirm-Change $markerPath 'Write ownership marker') {
    $installedFiles = @($KnownFiles | Where-Object { Test-Path -LiteralPath (Join-Path $safeRoot $_) -PathType Leaf })
    $marker = [ordered]@{
        product = $ProductName
        installRoot = $safeRoot
        executable = $destinationExecutable
        managedFiles = $installedFiles
        installedAtUtc = [DateTime]::UtcNow.ToString('o')
    }
    $marker | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $markerPath -Encoding UTF8
}

Set-Startup $destinationExecutable $Startup

Write-Output "Installed $ProductName to $safeRoot"
Write-Output "Startup mode: $Startup"
if (-not $NoLaunch -and -not $WhatIfPreference -and (Test-Path -LiteralPath $destinationExecutable -PathType Leaf)) {
    Start-Process -FilePath $destinationExecutable -ArgumentList '--start', '--minimized' -WorkingDirectory $safeRoot
}
