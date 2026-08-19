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

function Get-RunCommand() {
    $property = Get-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name $RunValueName -ErrorAction SilentlyContinue
    if ($null -eq $property) { return $null }
    return [string]$property.$RunValueName
}

function Test-RunCommandOwned([string]$Command, [string]$Executable) {
    if ([string]::IsNullOrWhiteSpace($Command)) { return $false }
    $candidate = $Command -replace '^\s*"([^"]+)".*$', '$1'
    if ($candidate -eq $Command) { $candidate = ($Command -split '\s+', 2)[0] }
    return (Test-SamePath $candidate $Executable)
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

$safeRoot = Assert-SafeInstallRoot $InstallRoot
$markerPath = Join-Path $safeRoot $MarkerName
if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) {
    throw "Ownership marker not found; refusing to remove an unverified directory: $safeRoot"
}

$marker = Get-Content -Raw -LiteralPath $markerPath | ConvertFrom-Json
if ($marker.product -ne $ProductName -or -not (Test-SamePath $marker.installRoot $safeRoot)) {
    throw "Ownership marker does not belong to ${ProductName}: $markerPath"
}

$executable = Get-FullPath ([string]$marker.executable)
if (-not (Test-SamePath (Split-Path -Parent $executable) $safeRoot)) {
    throw "Ownership marker points outside the installation directory; refusing to continue."
}

Stop-OwnedApplicationIfRunning $executable
Remove-OwnedStartup $executable

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
