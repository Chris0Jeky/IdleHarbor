[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Executable
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (@(Get-Process -Name IdleHarbor -ErrorAction SilentlyContinue).Count -ne 0) {
    throw 'Close every running IdleHarbor instance before the control help test.'
}

# Every control that must explain itself on hover. The tooltip stores one tool
# per control, so the total is the sum of these groups. Both halves of a
# label/field pair carry the same text: a user reading the name should get the
# explanation without having to find the input.
$expectedTools = [ordered]@{
    'status card'                                  = 1
    'labels (profile, motion, keep awake, interval, motion size, real input, battery, duration)' = 8
    'combo boxes (profile, motion, keep awake)'    = 3
    'edits (interval, motion size, real input, battery, duration)' = 5
    'check boxes'                                  = 9
    'action buttons (start, stop, save)'           = 3
}
$expectedTotal = ($expectedTools.Values | Measure-Object -Sum).Sum

# Each field also carries a short explanation printed under it. A distinctive
# opening phrase per hint is enough to prove the static exists with the right
# text, without pinning the whole sentence.
$expectedHints = @(
    'Starting values you can edit.',
    'Off emits nothing.',
    'Independent of motion, but not both off',
    'Time between motion pulses.',
    'A 1 to 120 scale for the visible path',
    'Quiet time required after you really type',
    'On battery only: pauses at or below this level',
    'Ends the session on its own after this long.'
)

$executablePath = (Resolve-Path -LiteralPath $Executable).Path

if (-not ('IdleHarbor.ControlHelpTips' -as [type])) {
    Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

namespace IdleHarbor {
public static class ControlHelpTips {
    public delegate bool EnumWindowsProc(IntPtr window, IntPtr data);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr data);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr window, StringBuilder buffer, int length);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);

    [DllImport("user32.dll")]
    public static extern IntPtr GetWindow(IntPtr window, uint command);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr parent, EnumWindowsProc callback, IntPtr data);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr window, StringBuilder text, int maximum);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")]
    public static extern IntPtr GetWindowLongPtr(IntPtr window, int index);

    [DllImport("user32.dll")]
    public static extern IntPtr GetParent(IntPtr window);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out RECT rectangle);

    [DllImport("user32.dll")]
    public static extern uint GetDpiForWindow(IntPtr window);

    [DllImport("user32.dll")]
    public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);

    [DllImport("user32.dll")]
    public static extern IntPtr GetDC(IntPtr window);

    [DllImport("user32.dll")]
    public static extern int ReleaseDC(IntPtr window, IntPtr dc);

    [DllImport("gdi32.dll")]
    public static extern IntPtr SelectObject(IntPtr dc, IntPtr obj);

    [DllImport("gdi32.dll")]
    public static extern bool DeleteObject(IntPtr obj);

    [DllImport("gdi32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr CreateFont(
        int height, int width, int escapement, int orientation, int weight,
        uint italic, uint underline, uint strikeOut, uint charSet,
        uint outputPrecision, uint clipPrecision, uint quality, uint pitchAndFamily, string face);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int DrawText(IntPtr dc, string text, int count, ref RECT rectangle, uint format);

    // The height an explanation's own text needs at the width it was given.
    // Deliberately reimplements what the app does rather than trusting it: the
    // failure this guards against is the app reserving too little and silently
    // clipping the last line, which reading the text back cannot detect.
    // Mirrors CreateHintFont in src/app/main.cpp (Segoe UI, 8pt, normal weight).
    public static int RequiredTextHeight(string text, int physicalWidth, uint dpi) {
        const int DT_CALCRECT = 0x00000400, DT_WORDBREAK = 0x00000010, DT_NOPREFIX = 0x00000800;
        IntPtr dc = GetDC(IntPtr.Zero);
        if (dc == IntPtr.Zero) {
            return 0;
        }
        int lfHeight = -(int)((8 * dpi + 36) / 72);
        IntPtr font = CreateFont(lfHeight, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 5, 0, "Segoe UI");
        IntPtr previous = font != IntPtr.Zero ? SelectObject(dc, font) : IntPtr.Zero;
        RECT box = new RECT();
        box.Right = physicalWidth;
        DrawText(dc, text, -1, ref box, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        if (previous != IntPtr.Zero) {
            SelectObject(dc, previous);
        }
        if (font != IntPtr.Zero) {
            DeleteObject(font);
        }
        ReleaseDC(IntPtr.Zero, dc);
        return box.Bottom - box.Top;
    }

    // The scrolling settings body. Its own children are the labels, headings,
    // fields, and printed explanations; the status card and the action buttons
    // belong to the top-level window instead.
    public static IntPtr FindSettingsViewport(IntPtr owner) {
        const int GWL_STYLE = -16;
        const long WS_VSCROLL = 0x00200000L;
        IntPtr result = IntPtr.Zero;
        EnumChildWindows(owner, (window, data) => {
            if (GetParent(window) != owner) {
                return true;
            }
            if ((GetWindowLongPtr(window, GWL_STYLE).ToInt64() & WS_VSCROLL) != 0) {
                result = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

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
            if (className.ToString() == "IdleHarbor.MainWindow" && IsWindowVisible(window)) {
                result = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    // The tooltip is an owned pop-up of the main window, not a child, so it is
    // reached through the top-level list rather than a child enumeration. Match
    // on the owner as well as the class: a themed control may create a tooltip
    // of its own in the same process, and reading the tool count from that one
    // would fail for a reason the message does not describe.
    public static IntPtr FindToolTip(uint targetProcessId, IntPtr owner) {
        const uint GW_OWNER = 4;
        IntPtr result = IntPtr.Zero;
        EnumWindows((window, data) => {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId != targetProcessId) {
                return true;
            }
            StringBuilder className = new StringBuilder(256);
            GetClassName(window, className, className.Capacity);
            if (className.ToString() == "tooltips_class32" && GetWindow(window, GW_OWNER) == owner) {
                result = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
}
'@
}

function Get-BodyStatics([IntPtr]$Owner) {
    $found = New-Object 'System.Collections.Generic.List[object]'
    $callback = [IdleHarbor.ControlHelpTips+EnumWindowsProc] {
        param([IntPtr]$window, [IntPtr]$data)
        $className = New-Object Text.StringBuilder 256
        [void][IdleHarbor.ControlHelpTips]::GetClassName($window, $className, $className.Capacity)
        if ($className.ToString() -eq 'Static') {
            $text = New-Object Text.StringBuilder 1024
            [void][IdleHarbor.ControlHelpTips]::GetWindowText($window, $text, $text.Capacity)
            $rect = New-Object IdleHarbor.ControlHelpTips+RECT
            [void][IdleHarbor.ControlHelpTips]::GetWindowRect($window, [ref]$rect)
            $found.Add([pscustomobject]@{
                Text   = $text.ToString()
                Top    = $rect.Top
                Bottom = $rect.Bottom
                Height = $rect.Bottom - $rect.Top
                Width  = $rect.Right - $rect.Left
            })
        }
        return $true
    }
    [void][IdleHarbor.ControlHelpTips]::EnumChildWindows($Owner, $callback, [IntPtr]::Zero)
    return $found
}

# Window rectangles must come back in physical pixels to compare against a font
# sized for the window's DPI.
[void][IdleHarbor.ControlHelpTips]::SetThreadDpiAwarenessContext([IntPtr](-4))

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) "IdleHarbor-help-tips-$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $tempRoot | Out-Null
$configPath = Join-Path $tempRoot 'settings.ini'
$process = $null

try {
    $process = Start-Process -FilePath $executablePath -ArgumentList @('--show', '--config', $configPath) -PassThru

    $window = [IntPtr]::Zero
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 100
        $window = [IdleHarbor.ControlHelpTips]::FindMainWindow([uint32]$process.Id)
    } while ($window -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
    if ($window -eq [IntPtr]::Zero) {
        throw 'IdleHarbor did not show its main window within 10 seconds.'
    }

    $tooltip = [IdleHarbor.ControlHelpTips]::FindToolTip([uint32]$process.Id, $window)
    if ($tooltip -eq [IntPtr]::Zero) {
        throw 'The settings window did not create a tooltip control, so no control explains itself on hover.'
    }

    # TTM_GETTOOLCOUNT (WM_USER + 13). The count is a scalar, so it survives a
    # cross-process SendMessage where a TOOLINFO pointer would not.
    $toolCount = [int][IdleHarbor.ControlHelpTips]::SendMessage(
        $tooltip, (0x0400 + 13), [IntPtr]::Zero, [IntPtr]::Zero)
    if ($toolCount -ne $expectedTotal) {
        $breakdown = ($expectedTools.GetEnumerator() | ForEach-Object { "$($_.Value) $($_.Key)" }) -join '; '
        throw "Expected $expectedTotal help tips ($breakdown) but the tooltip holds $toolCount."
    }

    $viewport = [IdleHarbor.ControlHelpTips]::FindSettingsViewport($window)
    if ($viewport -eq [IntPtr]::Zero) {
        throw 'Could not find the scrolling settings body.'
    }
    $dpi = [IdleHarbor.ControlHelpTips]::GetDpiForWindow($window)
    if ($dpi -eq 0) {
        throw 'Could not read the window DPI.'
    }
    $bodyStatics = Get-BodyStatics $viewport
    $blank = @($bodyStatics | Where-Object { [string]::IsNullOrWhiteSpace($_.Text) })
    if ($blank.Count -ne 0) {
        throw "$($blank.Count) label or explanation in the settings body has no text."
    }

    # Sorting by screen position lets each explanation be checked against
    # whatever the layout put after it. A wrapped explanation reserves the height
    # its own text needs, so the failure worth catching is one that reserved too
    # little and now runs into the control below -- which reading the text alone
    # cannot see.
    $ordered = @($bodyStatics | Sort-Object Top)
    foreach ($prefix in $expectedHints) {
        $hint = $ordered | Where-Object { $_.Text.StartsWith($prefix, [StringComparison]::Ordinal) } | Select-Object -First 1
        if ($null -eq $hint) {
            throw "A field lost its printed explanation: '$prefix'"
        }
        if ($hint.Height -le 0 -or $hint.Width -le 0) {
            throw "The explanation '$prefix' collapsed to $($hint.Width)x$($hint.Height)."
        }
        $required = [IdleHarbor.ControlHelpTips]::RequiredTextHeight($hint.Text, $hint.Width, $dpi)
        if ($required -le 0) {
            throw "Could not measure the explanation '$prefix'."
        }
        if ($hint.Height -lt $required) {
            throw ("The explanation '$prefix' is $($hint.Height)px tall but its text needs $required" +
                   "px at $($hint.Width)px wide, so its last line is cut off.")
        }
        $next = $ordered | Where-Object { $_.Top -gt $hint.Top } | Select-Object -First 1
        if ($null -ne $next -and $next.Top -lt $hint.Bottom) {
            throw ("The explanation '$prefix' ends at y=$($hint.Bottom) but the next label starts at " +
                   "y=$($next.Top), so its last line is covered.")
        }
    }

    Write-Output "Control help tips passed: $toolCount controls explain themselves on hover, $($expectedHints.Count) fields carry a printed explanation that fits its reserved space."
}
finally {
    if ($null -ne $process) {
        $process.Refresh()
        if (-not $process.HasExited) {
            Start-Process -FilePath $executablePath -ArgumentList '--exit' -WindowStyle Hidden -Wait
            if (-not $process.WaitForExit(5000)) {
                # Leaving a window on the user's desktop would also make the next
                # run fail at the single-instance guard for an unrelated reason.
                $process.Kill()
                $process.WaitForExit(5000) | Out-Null
            }
        }
    }
    $temporaryPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    $resolvedTempRoot = [IO.Path]::GetFullPath($tempRoot)
    if ($resolvedTempRoot.StartsWith($temporaryPrefix, [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolvedTempRoot) -like 'IdleHarbor-help-tips-*') {
        Remove-Item -LiteralPath $resolvedTempRoot -Recurse -Force
    }
    else {
        throw "Refusing to remove an unexpected help-tip test directory: $resolvedTempRoot"
    }
}
