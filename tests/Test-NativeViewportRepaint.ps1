[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Executable,

    [string]$EvidenceDirectory,

    [ValidateRange(1, 100)]
    [int]$ChurnCycles = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (@(Get-Process -Name IdleHarbor -ErrorAction SilentlyContinue).Count -ne 0) {
    throw 'Close every running IdleHarbor instance before the native repaint test.'
}

$executablePath = (Resolve-Path -LiteralPath $Executable).Path
if (-not ('IdleHarbor.NativeViewportRepaint' -as [type])) {
    Add-Type -AssemblyName System.Drawing
    Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

namespace IdleHarbor {
public static class NativeViewportRepaint {
    public delegate bool EnumWindowsProc(IntPtr window, IntPtr data);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }

    [StructLayout(LayoutKind.Sequential)]
    public struct SCROLLINFO {
        public uint cbSize;
        public uint fMask;
        public int nMin;
        public int nMax;
        public uint nPage;
        public int nPos;
        public int nTrackPos;
    }

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr data);

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr parent, EnumWindowsProc callback, IntPtr data);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("kernel32.dll")]
    public static extern uint GetCurrentThreadId();

    [DllImport("user32.dll")]
    public static extern bool AttachThreadInput(uint sourceThread, uint targetThread, bool attach);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr window, StringBuilder buffer, int length);

    [DllImport("user32.dll")]
    public static extern IntPtr GetParent(IntPtr window);

    [DllImport("user32.dll")]
    public static extern int GetDlgCtrlID(IntPtr window);

    [DllImport("user32.dll")]
    public static extern bool IsWindowEnabled(IntPtr window);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")]
    public static extern IntPtr GetWindowLongPtr(IntPtr window, int index);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr window, int command);

    [DllImport("user32.dll")]
    public static extern bool BringWindowToTop(IntPtr window);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr window);

    [DllImport("user32.dll")]
    public static extern IntPtr SetActiveWindow(IntPtr window);

    [DllImport("user32.dll")]
    public static extern IntPtr SetFocus(IntPtr window);

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
    public static extern bool GetClientRect(IntPtr window, out RECT rectangle);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr window, ref POINT point);

    [DllImport("user32.dll")]
    public static extern uint GetDpiForWindow(IntPtr window);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr window, StringBuilder text, int maximum);

    [DllImport("user32.dll")]
    public static extern bool GetScrollInfo(IntPtr window, int bar, ref SCROLLINFO info);

    [DllImport("user32.dll")]
    public static extern bool GetPhysicalCursorPos(out POINT point);

    [DllImport("user32.dll")]
    public static extern bool SetPhysicalCursorPos(int x, int y);

    [DllImport("user32.dll")]
    public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extraInfo);

    [DllImport("user32.dll")]
    public static extern bool RedrawWindow(IntPtr window, IntPtr rectangle, IntPtr region, uint flags);

    [DllImport("user32.dll")]
    public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);

    public static IntPtr FindMainWindow(uint targetProcessId) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((window, data) => {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId != targetProcessId) {
                return true;
            }
            StringBuilder className = new StringBuilder(256);
            GetClassName(window, className, className.Capacity);
            if (className.ToString() == "IdleHarbor.MainWindow") {
                result = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static IntPtr FindSettingsViewport(IntPtr owner) {
        IntPtr result = IntPtr.Zero;
        EnumChildWindows(owner, (window, data) => {
            if (GetParent(window) != owner) {
                return true;
            }
            const long WS_VSCROLL = 0x00200000L;
            if ((GetWindowLongPtr(window, -16).ToInt64() & WS_VSCROLL) != 0) {
                result = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static IntPtr FindDescendantControl(IntPtr owner, int controlId) {
        IntPtr result = IntPtr.Zero;
        EnumChildWindows(owner, (window, data) => {
            if (GetDlgCtrlID(window) == controlId) {
                result = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static string ReadWindowText(IntPtr window) {
        StringBuilder text = new StringBuilder(512);
        GetWindowText(window, text, text.Capacity);
        return text.ToString();
    }

    public static void ActivateWindow(IntPtr window) {
        uint processId;
        uint targetThread = GetWindowThreadProcessId(window, out processId);
        uint currentThread = GetCurrentThreadId();
        bool attached = targetThread != 0 && targetThread != currentThread &&
                        AttachThreadInput(currentThread, targetThread, true);
        try {
            ShowWindow(window, 9);
            BringWindowToTop(window);
            SetForegroundWindow(window);
            SetActiveWindow(window);
            SetFocus(window);
        }
        finally {
            if (attached) {
                AttachThreadInput(currentThread, targetThread, false);
            }
        }
    }
}
}
'@
}

function Get-ViewportScrollInfo([IntPtr]$Viewport) {
    $info = New-Object IdleHarbor.NativeViewportRepaint+SCROLLINFO
    $info.cbSize = [Runtime.InteropServices.Marshal]::SizeOf($info)
    $info.fMask = 0x0017 # SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS
    if (-not [IdleHarbor.NativeViewportRepaint]::GetScrollInfo($Viewport, 1, [ref]$info)) {
        throw 'GetScrollInfo failed for the settings viewport.'
    }
    return $info
}

function Invoke-ViewportWheelChurn(
    [IntPtr]$Window,
    [IntPtr]$Viewport,
    [int]$Maximum,
    [int]$Cycles) {
    $client = New-Object IdleHarbor.NativeViewportRepaint+RECT
    if (-not [IdleHarbor.NativeViewportRepaint]::GetClientRect($Viewport, [ref]$client)) {
        throw 'GetClientRect failed for native wheel input.'
    }
    $cursorTarget = New-Object IdleHarbor.NativeViewportRepaint+POINT
    $cursorTarget.X = [int](($client.Right - $client.Left) / 2)
    $cursorTarget.Y = [int](($client.Bottom - $client.Top) / 2)
    if (-not [IdleHarbor.NativeViewportRepaint]::ClientToScreen($Viewport, [ref]$cursorTarget)) {
        throw 'ClientToScreen failed for native wheel input.'
    }
    $originalCursor = New-Object IdleHarbor.NativeViewportRepaint+POINT
    if (-not [IdleHarbor.NativeViewportRepaint]::GetPhysicalCursorPos([ref]$originalCursor)) {
        throw 'GetPhysicalCursorPos failed for native wheel input.'
    }
    $wheelDown = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int32]-120), 0)
    $wheelUp = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int32]120), 0)
    try {
        [IdleHarbor.NativeViewportRepaint]::ActivateWindow($Window)
        if (-not [IdleHarbor.NativeViewportRepaint]::SetPhysicalCursorPos(
                $cursorTarget.X,
                $cursorTarget.Y)) {
            throw 'Could not position the cursor over the settings viewport.'
        }
        Start-Sleep -Milliseconds 100
        foreach ($attempt in 1..40) {
            if ((Get-ViewportScrollInfo $Viewport).nPos -eq $Maximum) {
                break
            }
            [IdleHarbor.NativeViewportRepaint]::mouse_event(0x0800, 0, 0, $wheelDown, [UIntPtr]::Zero)
            Start-Sleep -Milliseconds 20
        }
        if ((Get-ViewportScrollInfo $Viewport).nPos -ne $Maximum) {
            throw 'Native wheel input did not reach the bottom of the settings viewport.'
        }
        foreach ($cycle in 1..$Cycles) {
            [IdleHarbor.NativeViewportRepaint]::mouse_event(0x0800, 0, 0, $wheelUp, [UIntPtr]::Zero)
            Start-Sleep -Milliseconds 10
            [IdleHarbor.NativeViewportRepaint]::mouse_event(0x0800, 0, 0, $wheelDown, [UIntPtr]::Zero)
            Start-Sleep -Milliseconds 10
        }
    }
    finally {
        [IdleHarbor.NativeViewportRepaint]::SetPhysicalCursorPos($originalCursor.X, $originalCursor.Y) | Out-Null
    }
    Start-Sleep -Milliseconds 250
}

function Save-ClientCapture([IntPtr]$Window, [string]$Path, [string]$Description) {
    $client = New-Object IdleHarbor.NativeViewportRepaint+RECT
    if (-not [IdleHarbor.NativeViewportRepaint]::GetClientRect($Window, [ref]$client)) {
        throw "GetClientRect failed for $Description."
    }
    $origin = New-Object IdleHarbor.NativeViewportRepaint+POINT
    if (-not [IdleHarbor.NativeViewportRepaint]::ClientToScreen($Window, [ref]$origin)) {
        throw "ClientToScreen failed for $Description."
    }
    $width = $client.Right - $client.Left
    $height = $client.Bottom - $client.Top
    if ($width -le 0 -or $height -le 0) {
        throw "$Description has invalid dimensions: ${width}x${height}."
    }

    $bitmap = New-Object Drawing.Bitmap($width, $height)
    try {
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0, $bitmap.Size)
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

function Save-ViewportCapture([IntPtr]$Viewport, [string]$Path) {
    Save-ClientCapture $Viewport $Path 'the settings viewport'
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) "IdleHarbor-native-repaint-$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $tempRoot | Out-Null
$captureRoot = if ([string]::IsNullOrWhiteSpace($EvidenceDirectory)) {
    $tempRoot
}
else {
    New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Select-Object -ExpandProperty FullName
}
$configPath = Join-Path $tempRoot 'settings.ini'
$process = $null
$window = [IntPtr]::Zero

try {
    [void][IdleHarbor.NativeViewportRepaint]::SetThreadDpiAwarenessContext([IntPtr](-4))

    # A first-owner runtime override must remain explicitly saveable instead
    # of being mistaken for persisted INI state. Prove that contract before
    # starting the clean repaint scenario below.
    $overrideConfigPath = Join-Path $tempRoot 'override-settings.ini'
    $process = Start-Process -FilePath $executablePath -ArgumentList @(
        '--show', '--config', $overrideConfigPath, '--motion', 'off') -PassThru
    $overrideDeadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 100
        $window = [IdleHarbor.NativeViewportRepaint]::FindMainWindow([uint32]$process.Id)
    } while ($window -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $overrideDeadline)
    if ($window -eq [IntPtr]::Zero) {
        throw 'The runtime-override owner did not expose its main window within 10 seconds.'
    }
    $overrideSave = [IdleHarbor.NativeViewportRepaint]::FindDescendantControl($window, 122)
    $overrideStatus = [IdleHarbor.NativeViewportRepaint]::FindDescendantControl($window, 100)
    if ($overrideSave -eq [IntPtr]::Zero -or $overrideStatus -eq [IntPtr]::Zero -or
        -not [IdleHarbor.NativeViewportRepaint]::IsWindowEnabled($overrideSave)) {
        throw 'A first-owner runtime override was not exposed as explicitly saveable.'
    }
    if ([IdleHarbor.NativeViewportRepaint]::ReadWindowText($overrideStatus) -notlike 'Unsaved changes*') {
        throw 'A first-owner runtime override did not expose its unsaved status.'
    }
    $exitCommand = Start-Process -FilePath $executablePath -ArgumentList @('--exit') -PassThru
    if (-not $exitCommand.WaitForExit(5000)) {
        throw 'The override-owner exit command did not return.'
    }
    if (-not $process.WaitForExit(5000)) {
        throw 'The runtime-override owner did not exit.'
    }
    $process = $null
    $window = [IntPtr]::Zero

    $arguments = @('--show', '--config', $configPath)
    $process = Start-Process -FilePath $executablePath -ArgumentList $arguments -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 100
        $window = [IdleHarbor.NativeViewportRepaint]::FindMainWindow([uint32]$process.Id)
    } while ($window -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
    if ($window -eq [IntPtr]::Zero) {
        throw 'IdleHarbor did not expose its main window within 10 seconds.'
    }

    $dpi = [IdleHarbor.NativeViewportRepaint]::GetDpiForWindow($window)
    $targetWidth = [Math]::Max([int][Math]::Round(600 * $dpi / 96.0), 600)
    $targetHeight = [Math]::Max([int][Math]::Round(380 * $dpi / 96.0), 380)
    [IdleHarbor.NativeViewportRepaint]::ActivateWindow($window)
    if (-not [IdleHarbor.NativeViewportRepaint]::SetWindowPos(
            $window,
            [IntPtr](-1),
            20,
            20,
            $targetWidth,
            $targetHeight,
            0x0040)) {
        throw 'Could not expose the native window for repaint capture.'
    }
    [IdleHarbor.NativeViewportRepaint]::ActivateWindow($window)
    Start-Sleep -Milliseconds 300

    $windowNaturalPath = Join-Path $captureRoot 'window-after-resize-natural.png'
    Save-ClientCapture $window $windowNaturalPath 'the main window'
    if (-not [IdleHarbor.NativeViewportRepaint]::RedrawWindow(
            $window,
            [IntPtr]::Zero,
            [IntPtr]::Zero,
            0x0185)) {
        throw 'Could not establish the whole-window repaint reference.'
    }
    $windowReferencePath = Join-Path $captureRoot 'window-after-resize-reference.png'
    Save-ClientCapture $window $windowReferencePath 'the main window'
    $windowNaturalHash = (Get-FileHash -LiteralPath $windowNaturalPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $windowReferenceHash = (Get-FileHash -LiteralPath $windowReferencePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($windowNaturalHash -cne $windowReferenceHash) {
        throw "Post-resize window repaint differs from the explicit reference: $windowNaturalHash versus $windowReferenceHash."
    }

    $viewport = [IdleHarbor.NativeViewportRepaint]::FindSettingsViewport($window)
    if ($viewport -eq [IntPtr]::Zero) {
        throw 'Could not find the settings viewport.'
    }
    $initial = Get-ViewportScrollInfo $viewport
    $maximum = [Math]::Max($initial.nMax - [int]$initial.nPage + 1, 0)
    if ($maximum -le 0) {
        throw 'The compact native viewport did not expose a scroll range.'
    }

    $profile = [IdleHarbor.NativeViewportRepaint]::FindDescendantControl($window, 101)
    if ($profile -eq [IntPtr]::Zero -or
        -not [IdleHarbor.NativeViewportRepaint]::IsWindowEnabled($profile)) {
        throw 'The stopped native window did not expose an enabled profile control.'
    }
    $save = [IdleHarbor.NativeViewportRepaint]::FindDescendantControl($window, 122)
    $status = [IdleHarbor.NativeViewportRepaint]::FindDescendantControl($window, 100)
    if ($save -eq [IntPtr]::Zero -or $status -eq [IntPtr]::Zero) {
        throw 'The redesigned native window did not expose its fixed Save action and status card.'
    }
    if ([IdleHarbor.NativeViewportRepaint]::IsWindowEnabled($save)) {
        throw 'The fixed Save action was enabled before any setting changed.'
    }

    $currentProfile = [int][IdleHarbor.NativeViewportRepaint]::SendMessage(
        $profile, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero) # CB_GETCURSEL
    $nextProfile = ($currentProfile + 1) % 7
    [IdleHarbor.NativeViewportRepaint]::SendMessage(
        $profile, 0x014E, [IntPtr]$nextProfile, [IntPtr]::Zero) | Out-Null # CB_SETCURSEL
    $profileChanged = [IntPtr](101 -bor (1 -shl 16)) # CBN_SELCHANGE
    [IdleHarbor.NativeViewportRepaint]::SendMessage(
        $window, 0x0111, $profileChanged, $profile) | Out-Null # WM_COMMAND
    if (-not [IdleHarbor.NativeViewportRepaint]::IsWindowEnabled($save)) {
        throw 'Selecting a different profile did not enable the fixed Save action.'
    }
    if ([IdleHarbor.NativeViewportRepaint]::ReadWindowText($status) -notlike 'Unsaved changes*') {
        throw 'Selecting a different profile did not expose the unsaved-changes status.'
    }
    [IdleHarbor.NativeViewportRepaint]::SendMessage(
        $save, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null # BM_CLICK
    if ([IdleHarbor.NativeViewportRepaint]::IsWindowEnabled($save)) {
        throw 'Saving the selected profile did not clear the dirty state.'
    }

    # A settings-only command forwarded to the owner must expose the same
    # explicitly saveable dirty state as a first-owner override.
    $forwardedOverride = Start-Process -FilePath $executablePath -ArgumentList @(
        '--motion', 'linear') -PassThru
    if (-not $forwardedOverride.WaitForExit(5000) -or $forwardedOverride.ExitCode -ne 0) {
        throw 'The forwarded settings override did not complete successfully.'
    }
    $forwardedDeadline = [DateTime]::UtcNow.AddSeconds(5)
    while (-not [IdleHarbor.NativeViewportRepaint]::IsWindowEnabled($save) -and
           [DateTime]::UtcNow -lt $forwardedDeadline) {
        Start-Sleep -Milliseconds 25
    }
    if (-not [IdleHarbor.NativeViewportRepaint]::IsWindowEnabled($save) -or
        [IdleHarbor.NativeViewportRepaint]::ReadWindowText($status) -notlike 'Unsaved changes*') {
        throw 'A forwarded settings override was not exposed as explicitly saveable and unsaved.'
    }
    [IdleHarbor.NativeViewportRepaint]::SendMessage(
        $save, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null # BM_CLICK
    if ([IdleHarbor.NativeViewportRepaint]::IsWindowEnabled($save) -or
        [IdleHarbor.NativeViewportRepaint]::ReadWindowText($status) -like 'Unsaved changes*') {
        throw 'Saving the forwarded settings override did not clear its dirty state.'
    }

    [IdleHarbor.NativeViewportRepaint]::RedrawWindow(
        $viewport,
        [IntPtr]::Zero,
        [IntPtr]::Zero,
        0x0185) | Out-Null
    $startCommand = Start-Process -FilePath $executablePath -ArgumentList @(
        '--start',
        '--motion', 'off',
        '--power', 'system',
        '--pause-on-input', '0',
        '--stop-after', '30s') -PassThru
    if (-not $startCommand.WaitForExit(5000)) {
        throw 'The start command did not return after contacting the visible instance.'
    }
    $activeDeadline = [DateTime]::UtcNow.AddSeconds(5)
    while ([IdleHarbor.NativeViewportRepaint]::IsWindowEnabled($profile) -and
           [DateTime]::UtcNow -lt $activeDeadline) {
        Start-Sleep -Milliseconds 25
    }
    if ([IdleHarbor.NativeViewportRepaint]::IsWindowEnabled($profile)) {
        throw 'The motion-free native session did not disable its settings controls.'
    }
    if ([IdleHarbor.NativeViewportRepaint]::ReadWindowText($status) -notlike 'Unsaved changes*') {
        throw 'A forwarded start with runtime overrides did not retain its unsaved status.'
    }
    # The themed combo boxes animate their enabled-to-disabled transition.
    # Let that finite system animation settle before comparing a natural frame
    # with a forced repaint; the assertion is intended to catch persistent
    # repaint loss, not intermediate colour frames in a normal transition.
    Start-Sleep -Milliseconds 500

    $startNaturalPath = Join-Path $captureRoot 'viewport-after-start-natural.png'
    Save-ViewportCapture $viewport $startNaturalPath
    if (-not [IdleHarbor.NativeViewportRepaint]::RedrawWindow(
            $viewport,
            [IntPtr]::Zero,
            [IntPtr]::Zero,
            0x0185)) {
        throw 'Could not establish the post-start explicit repaint reference.'
    }
    $startReferencePath = Join-Path $captureRoot 'viewport-after-start-reference.png'
    Save-ViewportCapture $viewport $startReferencePath
    $startNaturalHash = (Get-FileHash -LiteralPath $startNaturalPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $startReferenceHash = (Get-FileHash -LiteralPath $startReferencePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($startNaturalHash -cne $startReferenceHash) {
        throw "Post-start repaint differs from the explicit reference: $startNaturalHash versus $startReferenceHash."
    }

    # Exercise the real wheel-input route while the settings controls are
    # disabled by an active, motion-free session.
    [IdleHarbor.NativeViewportRepaint]::SendMessage($viewport, 0x0115, [IntPtr]6, [IntPtr]::Zero) | Out-Null
    Invoke-ViewportWheelChurn $window $viewport $maximum $ChurnCycles
    $natural = Get-ViewportScrollInfo $viewport
    if ($natural.nPos -le 0) {
        throw 'Native wheel churn did not leave the settings viewport scrolled.'
    }
    $naturalPath = Join-Path $captureRoot 'viewport-natural-scroll.png'
    Save-ViewportCapture $viewport $naturalPath

    # Compare the naturally painted viewport with a forced full descendant
    # repaint at the exact same scroll position.
    if (-not [IdleHarbor.NativeViewportRepaint]::RedrawWindow(
            $viewport,
            [IntPtr]::Zero,
            [IntPtr]::Zero,
            0x0185)) {
        throw 'Could not establish the explicit repaint reference.'
    }
    $referencePosition = (Get-ViewportScrollInfo $viewport).nPos
    if ($referencePosition -ne $natural.nPos) {
        throw "Explicit repaint changed the scroll position from $($natural.nPos) to $referencePosition."
    }
    $referencePath = Join-Path $captureRoot 'viewport-explicit-reference.png'
    Save-ViewportCapture $viewport $referencePath

    $referenceHash = (Get-FileHash -LiteralPath $referencePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $naturalHash = (Get-FileHash -LiteralPath $naturalPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($referenceHash -cne $naturalHash) {
        throw "Natural scroll repaint differs from the explicit reference: $naturalHash versus $referenceHash."
    }

    Write-Output "Native viewport repaint passed at $dpi DPI: $naturalHash"
}
finally {
    if ($window -ne [IntPtr]::Zero) {
        [IdleHarbor.NativeViewportRepaint]::SetWindowPos(
            $window,
            [IntPtr](-2),
            20,
            20,
            0,
            0,
            0x0001 -bor 0x0002 -bor 0x0040) | Out-Null
    }
    if ($null -ne $process) {
        $process.Refresh()
        if (-not $process.HasExited) {
            Start-Process -FilePath $executablePath -ArgumentList '--exit' -WindowStyle Hidden -Wait
            $process.WaitForExit(5000) | Out-Null
        }
    }
    $temporaryPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    $resolvedTempRoot = [IO.Path]::GetFullPath($tempRoot)
    if ($resolvedTempRoot.StartsWith($temporaryPrefix, [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolvedTempRoot) -like 'IdleHarbor-native-repaint-*') {
        Remove-Item -LiteralPath $resolvedTempRoot -Recurse -Force
    }
    else {
        throw "Refusing to remove an unexpected native-test directory: $resolvedTempRoot"
    }
}
