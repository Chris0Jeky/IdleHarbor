[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$Executable,

    [string]$OutputDirectory = '',

    [ValidateRange(96, 480)]
    [int]$ExpectedDpi = 192,

    [ValidateRange(640, 3840)]
    [int]$WindowWidth = 1178,

    [ValidateRange(480, 2160)]
    [int]$WindowHeight = 789,

    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'docs\assets'
}
$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

$captureNames = @(
    'idleharbor-window.png',
    'idleharbor-viewport.png',
    'idleharbor-running.png',
    'idleharbor-paused.png',
    'idleharbor-tray-menu.png',
    'capture-manifest.json'
)
foreach ($name in $captureNames) {
    $path = Join-Path $outputRoot $name
    if ((Test-Path -LiteralPath $path) -and -not $Force) {
        throw "Capture already exists; pass -Force to replace it: $path"
    }
}

$runningOwners = @(Get-CimInstance Win32_Process -Filter "Name='IdleHarbor.exe'")
if ($runningOwners.Count -ne 0) {
    $ownerSummary = ($runningOwners | ForEach-Object { "PID $($_.ProcessId): $($_.ExecutablePath)" }) -join '; '
    throw "Close the existing IdleHarbor owner before capture. Found $ownerSummary"
}

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace IdleHarbor.Capture
{
    public static class Native
    {
        public delegate bool EnumWindowProc(IntPtr window, IntPtr parameter);

        [StructLayout(LayoutKind.Sequential)]
        public struct RECT
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct POINT
        {
            public int X;
            public int Y;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NOTIFYICONIDENTIFIER
        {
            public uint cbSize;
            public IntPtr hWnd;
            public uint uID;
            public Guid guidItem;
        }

        [DllImport("user32.dll")]
        public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);

        [DllImport("user32.dll")]
        public static extern bool EnumWindows(EnumWindowProc callback, IntPtr parameter);

        [DllImport("user32.dll")]
        public static extern bool EnumChildWindows(IntPtr parent, EnumWindowProc callback, IntPtr parameter);

        [DllImport("user32.dll")]
        public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        public static extern int GetClassName(IntPtr window, StringBuilder className, int maximum);

        [DllImport("user32.dll")]
        public static extern int GetDlgCtrlID(IntPtr window);

        [DllImport("user32.dll")]
        public static extern int GetWindowLong(IntPtr window, int index);

        [DllImport("user32.dll")]
        public static extern bool IsWindowVisible(IntPtr window);

        [DllImport("user32.dll")]
        public static extern bool IsWindowEnabled(IntPtr window);

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr window, out RECT rectangle);

        [DllImport("user32.dll")]
        public static extern bool SetWindowPos(
            IntPtr window,
            IntPtr insertAfter,
            int x,
            int y,
            int width,
            int height,
            uint flags);

        [DllImport("user32.dll")]
        public static extern bool ShowWindow(IntPtr window, int command);

        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr window);

        [DllImport("user32.dll")]
        public static extern uint GetDpiForWindow(IntPtr window);

        [DllImport("user32.dll")]
        public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

        [DllImport("user32.dll")]
        public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

        [DllImport("user32.dll")]
        public static extern bool RedrawWindow(IntPtr window, IntPtr update, IntPtr region, uint flags);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        public static extern int GetWindowText(IntPtr window, StringBuilder text, int maximum);

        [DllImport("user32.dll")]
        public static extern bool GetPhysicalCursorPos(out POINT point);

        [DllImport("user32.dll")]
        public static extern bool SetPhysicalCursorPos(int x, int y);

        [DllImport("user32.dll")]
        public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extraInfo);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        public static extern IntPtr FindWindow(string className, string windowName);

        [DllImport("shell32.dll")]
        public static extern int Shell_NotifyIconGetRect(
            ref NOTIFYICONIDENTIFIER identifier,
            out RECT iconLocation);

        public static IntPtr FindMainWindow(uint processId)
        {
            IntPtr found = IntPtr.Zero;
            EnumWindows((window, parameter) =>
            {
                uint owner;
                GetWindowThreadProcessId(window, out owner);
                if (owner != processId || !IsWindowVisible(window))
                {
                    return true;
                }
                StringBuilder className = new StringBuilder(128);
                GetClassName(window, className, className.Capacity);
                if (className.ToString() == "IdleHarbor.MainWindow")
                {
                    found = window;
                    return false;
                }
                return true;
            }, IntPtr.Zero);
            return found;
        }

        public static IntPtr FindDescendantControl(IntPtr parent, int identifier)
        {
            IntPtr found = IntPtr.Zero;
            EnumChildWindows(parent, (window, parameter) =>
            {
                if (GetDlgCtrlID(window) == identifier)
                {
                    found = window;
                    return false;
                }
                return true;
            }, IntPtr.Zero);
            return found;
        }

        public static IntPtr FindSettingsViewport(IntPtr parent)
        {
            const int GWL_STYLE = -16;
            const int WS_VSCROLL = 0x00200000;
            IntPtr found = IntPtr.Zero;
            EnumChildWindows(parent, (window, parameter) =>
            {
                StringBuilder className = new StringBuilder(128);
                GetClassName(window, className, className.Capacity);
                int style = GetWindowLong(window, GWL_STYLE);
                if (className.ToString() == "Static" && (style & WS_VSCROLL) != 0)
                {
                    found = window;
                    return false;
                }
                return true;
            }, IntPtr.Zero);
            return found;
        }

        public static string ReadWindowText(IntPtr window)
        {
            StringBuilder text = new StringBuilder(512);
            GetWindowText(window, text, text.Capacity);
            return text.ToString();
        }
    }
}
'@

function Write-CaptureSettings([string]$Path, [bool]$PauseOutsideActiveHours) {
    $activeHoursEnabled = if ($PauseOutsideActiveHours) { 'true' } else { 'false' }
    $currentMinute = ([DateTime]::Now.Hour * 60) + [DateTime]::Now.Minute
    $activeHoursStart = if ($PauseOutsideActiveHours) { ($currentMinute + 5) % 1440 } else { 0 }
    $activeHoursEnd = if ($PauseOutsideActiveHours) { ($activeHoursStart + 1) % 1440 } else { 0 }
    $content = @"
# Deterministic local settings used only for IdleHarbor documentation captures.
schema=1
profile=long-task
motion=off
power=system
interval_seconds=120
random_minimum_seconds=1
distance=1
randomize=false
pause_on_user_activity=false
user_activity_cooldown_seconds=60
pause_when_locked=true
pause_when_disconnected=true
pause_on_battery=false
pause_on_low_battery=true
low_battery_threshold=20
pause_when_fullscreen=false
active_hours_enabled=$activeHoursEnabled
active_hours_start_minute=$activeHoursStart
active_hours_end_minute=$activeHoursEnd
max_duration_seconds=0
start_minimized=false
close_to_tray=true
show_notifications=true
emergency_hotkey=true
"@
    $encoding = New-Object Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($Path, $content.TrimStart(), $encoding)
}

function Start-CaptureOwner([string]$ConfigPath) {
    $owner = Start-Process -FilePath $executablePath -ArgumentList @(
        '--show', '--config', $ConfigPath) -PassThru
    $window = [IntPtr]::Zero
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 100
        $window = [IdleHarbor.Capture.Native]::FindMainWindow([uint32]$owner.Id)
    } while ($window -eq [IntPtr]::Zero -and -not $owner.HasExited -and [DateTime]::UtcNow -lt $deadline)
    if ($window -eq [IntPtr]::Zero) {
        throw 'IdleHarbor did not expose its main window within 10 seconds.'
    }

    [IdleHarbor.Capture.Native]::ShowWindow($window, 9) | Out-Null
    if (-not [IdleHarbor.Capture.Native]::SetWindowPos(
            $window,
            [IntPtr]::Zero,
            20,
            20,
            $WindowWidth,
            $WindowHeight,
            0x0040)) {
        throw 'Could not size the documentation-capture window.'
    }
    [IdleHarbor.Capture.Native]::SetForegroundWindow($window) | Out-Null
    Start-Sleep -Milliseconds 600

    $dpi = [int][IdleHarbor.Capture.Native]::GetDpiForWindow($window)
    if ($dpi -ne $ExpectedDpi) {
        throw "Expected a $ExpectedDpi-DPI capture but the window reports $dpi DPI."
    }
    return [pscustomobject]@{ Process = $owner; Window = $window; Dpi = $dpi }
}

function Stop-CaptureOwner($Owner) {
    if ($null -eq $Owner -or $Owner.Process.HasExited) {
        return
    }
    $exitCommand = Start-Process -FilePath $executablePath -ArgumentList @('--exit') -PassThru
    if (-not $exitCommand.WaitForExit(5000)) {
        throw 'The capture-owner exit command did not return.'
    }
    if (-not $Owner.Process.WaitForExit(5000)) {
        throw 'The capture owner did not exit.'
    }
}

function Invoke-OwnerCommand([string[]]$Arguments) {
    $command = Start-Process -FilePath $executablePath -ArgumentList $Arguments -PassThru
    if (-not $command.WaitForExit(5000)) {
        throw "IdleHarbor command did not return: $($Arguments -join ' ')"
    }
    if ($command.ExitCode -ne 0) {
        throw "IdleHarbor command failed with exit code $($command.ExitCode): $($Arguments -join ' ')"
    }
}

function Wait-Status([IntPtr]$StatusControl, [string]$Pattern, [int]$Seconds = 5) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        $text = [IdleHarbor.Capture.Native]::ReadWindowText($StatusControl)
        if ($text -like $Pattern) {
            return $text
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Status did not match '$Pattern'; last value was '$text'."
}

function Save-ScreenRectangle(
    [IdleHarbor.Capture.Native+RECT]$Rectangle,
    [string]$Path) {
    $width = $Rectangle.Right - $Rectangle.Left
    $height = $Rectangle.Bottom - $Rectangle.Top
    if ($width -le 0 -or $height -le 0) {
        throw "Invalid capture rectangle for ${Path}: ${width}x${height}."
    }
    if (Test-Path -LiteralPath $Path) {
        [IO.File]::Delete($Path)
    }
    $bitmap = New-Object Drawing.Bitmap($width, $height)
    try {
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CopyFromScreen($Rectangle.Left, $Rectangle.Top, 0, 0, $bitmap.Size)
        }
        finally {
            $graphics.Dispose()
        }
        $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

function Save-WindowCapture([IntPtr]$Window, [string]$Path) {
    [IdleHarbor.Capture.Native]::RedrawWindow(
        $Window,
        [IntPtr]::Zero,
        [IntPtr]::Zero,
        0x0185) | Out-Null
    Start-Sleep -Milliseconds 250
    $rectangle = New-Object IdleHarbor.Capture.Native+RECT
    if (-not [IdleHarbor.Capture.Native]::GetWindowRect($Window, [ref]$rectangle)) {
        throw 'Could not read the capture window rectangle.'
    }
    Save-ScreenRectangle $rectangle $Path
}

function Save-TrayMenuCapture([IntPtr]$Window, [string]$Path) {
    $identifier = New-Object IdleHarbor.Capture.Native+NOTIFYICONIDENTIFIER
    $identifier.cbSize = [Runtime.InteropServices.Marshal]::SizeOf($identifier)
    $identifier.hWnd = $Window
    $identifier.uID = 1
    $identifier.guidItem = [Guid]::Empty
    $iconRectangle = New-Object IdleHarbor.Capture.Native+RECT
    $result = [IdleHarbor.Capture.Native]::Shell_NotifyIconGetRect([ref]$identifier, [ref]$iconRectangle)
    if ($result -ne 0) {
        throw "Shell_NotifyIconGetRect failed with HRESULT 0x$('{0:X8}' -f ([uint32]$result))."
    }

    $originalCursor = New-Object IdleHarbor.Capture.Native+POINT
    if (-not [IdleHarbor.Capture.Native]::GetPhysicalCursorPos([ref]$originalCursor)) {
        throw 'Could not read the cursor position before opening the tray menu.'
    }
    try {
        $x = [int](($iconRectangle.Left + $iconRectangle.Right) / 2)
        $y = [int](($iconRectangle.Top + $iconRectangle.Bottom) / 2)
        [IdleHarbor.Capture.Native]::SetPhysicalCursorPos($x, $y) | Out-Null
        # Post the same callback Explorer sends for a tray-icon context menu.
        # A synchronous SendMessage would block inside TrackPopupMenu and leave
        # no opportunity to capture the menu from this process.
        if (-not [IdleHarbor.Capture.Native]::PostMessage(
                $Window, 0x8001, [IntPtr]::Zero, [IntPtr]0x007B)) {
            throw 'Could not request the notification-area menu.'
        }

        $menu = [IntPtr]::Zero
        $deadline = [DateTime]::UtcNow.AddSeconds(5)
        do {
            Start-Sleep -Milliseconds 100
            $menu = [IdleHarbor.Capture.Native]::FindWindow('#32768', $null)
        } while ($menu -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
        if ($menu -eq [IntPtr]::Zero) {
            throw 'The notification-area menu did not appear.'
        }
        Start-Sleep -Milliseconds 250
        $menuRectangle = New-Object IdleHarbor.Capture.Native+RECT
        if (-not [IdleHarbor.Capture.Native]::GetWindowRect($menu, [ref]$menuRectangle)) {
            throw 'Could not read the notification-area menu rectangle.'
        }
        Save-ScreenRectangle $menuRectangle $Path
        [IdleHarbor.Capture.Native]::keybd_event(0x1B, 0, 0, [UIntPtr]::Zero)
        [IdleHarbor.Capture.Native]::keybd_event(0x1B, 0, 0x0002, [UIntPtr]::Zero)
    }
    finally {
        [IdleHarbor.Capture.Native]::SetPhysicalCursorPos($originalCursor.X, $originalCursor.Y) | Out-Null
    }
}

function Get-ImageEvidence([string]$FileName, [string]$State) {
    $path = Join-Path $outputRoot $FileName
    $image = [Drawing.Image]::FromFile($path)
    try {
        return [ordered]@{
            file = $FileName
            state = $State
            width = $image.Width
            height = $image.Height
            sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    finally {
        $image.Dispose()
    }
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) "IdleHarbor-capture-$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $tempRoot | Out-Null
$steadyConfig = Join-Path $tempRoot 'steady.ini'
$pauseConfig = Join-Path $tempRoot 'pause.ini'
Write-CaptureSettings $steadyConfig $false
Write-CaptureSettings $pauseConfig $true

$owner = $null
try {
    [void][IdleHarbor.Capture.Native]::SetThreadDpiAwarenessContext([IntPtr](-4))
    $owner = Start-CaptureOwner $steadyConfig
    $status = [IdleHarbor.Capture.Native]::FindDescendantControl($owner.Window, 100)
    $viewport = [IdleHarbor.Capture.Native]::FindSettingsViewport($owner.Window)
    if ($status -eq [IntPtr]::Zero -or $viewport -eq [IntPtr]::Zero) {
        throw 'Could not find the native status card or settings viewport.'
    }

    Save-WindowCapture $owner.Window (Join-Path $outputRoot 'idleharbor-window.png')
    [IdleHarbor.Capture.Native]::SendMessage(
        $viewport, 0x0115, [IntPtr]7, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 350
    Save-WindowCapture $owner.Window (Join-Path $outputRoot 'idleharbor-viewport.png')
    [IdleHarbor.Capture.Native]::SendMessage(
        $viewport, 0x0115, [IntPtr]6, [IntPtr]::Zero) | Out-Null

    Invoke-OwnerCommand @('--start')
    Wait-Status $status 'Running*' | Out-Null
    Start-Sleep -Milliseconds 600
    Save-WindowCapture $owner.Window (Join-Path $outputRoot 'idleharbor-running.png')
    Save-TrayMenuCapture $owner.Window (Join-Path $outputRoot 'idleharbor-tray-menu.png')
    Stop-CaptureOwner $owner
    $owner = $null

    $owner = Start-CaptureOwner $pauseConfig
    $status = [IdleHarbor.Capture.Native]::FindDescendantControl($owner.Window, 100)
    if ($status -eq [IntPtr]::Zero) {
        throw 'Could not find the native status card for the paused capture.'
    }
    Invoke-OwnerCommand @('--start')
    Wait-Status $status 'Paused*' | Out-Null
    Start-Sleep -Milliseconds 350
    Save-WindowCapture $owner.Window (Join-Path $outputRoot 'idleharbor-paused.png')
}
finally {
    if ($null -ne $owner) {
        Stop-CaptureOwner $owner
    }
    $resolvedTemp = [IO.Path]::GetFullPath($tempRoot)
    $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolvedTemp.StartsWith($systemTemp, [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolvedTemp) -like 'IdleHarbor-capture-*') {
        [IO.Directory]::Delete($resolvedTemp, $true)
    }
}

$sourceRevision = (& git -C $repositoryRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $sourceRevision -notmatch '^[0-9a-f]{40}$') {
    throw 'Could not determine the exact source revision for the capture manifest.'
}
$executableItem = Get-Item -LiteralPath $executablePath
$signature = Get-AuthenticodeSignature -FilePath $executablePath
$captures = @(
    (Get-ImageEvidence 'idleharbor-window.png' 'Stopped settings and fixed actions')
    (Get-ImageEvidence 'idleharbor-viewport.png' 'Stopped settings scrolled to safeguards and notification options')
    (Get-ImageEvidence 'idleharbor-running.png' 'Running motion-free keep-awake session')
    (Get-ImageEvidence 'idleharbor-paused.png' 'Paused outside configured active hours')
    (Get-ImageEvidence 'idleharbor-tray-menu.png' 'Running notification-area menu')
)
$manifest = [ordered]@{
    schema = 1
    capturedAtUtc = [DateTime]::UtcNow.ToString('o')
    sourceRevision = $sourceRevision
    dpi = $ExpectedDpi
    executable = [ordered]@{
        file = $executableItem.Name
        sizeBytes = $executableItem.Length
        sha256 = (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash.ToLowerInvariant()
        authenticodeStatus = [string]$signature.Status
    }
    captures = $captures
}
$manifestPath = Join-Path $outputRoot 'capture-manifest.json'
$encoding = New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText(
    $manifestPath,
    (($manifest | ConvertTo-Json -Depth 6) + [Environment]::NewLine),
    $encoding)

$manifest
