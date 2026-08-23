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
    'Pauses at or below this level',
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

function Get-StaticTexts([IntPtr]$Owner) {
    $texts = New-Object 'System.Collections.Generic.List[string]'
    $callback = [IdleHarbor.ControlHelpTips+EnumWindowsProc] {
        param([IntPtr]$window, [IntPtr]$data)
        $className = New-Object Text.StringBuilder 256
        [void][IdleHarbor.ControlHelpTips]::GetClassName($window, $className, $className.Capacity)
        if ($className.ToString() -eq 'Static') {
            $text = New-Object Text.StringBuilder 1024
            [void][IdleHarbor.ControlHelpTips]::GetWindowText($window, $text, $text.Capacity)
            $texts.Add($text.ToString())
        }
        return $true
    }
    [void][IdleHarbor.ControlHelpTips]::EnumChildWindows($Owner, $callback, [IntPtr]::Zero)
    return $texts
}

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
    $staticTexts = Get-StaticTexts $viewport
    $missing = @($expectedHints | Where-Object {
        $prefix = $_
        -not ($staticTexts | Where-Object { $_.StartsWith($prefix, [StringComparison]::Ordinal) })
    })
    if ($missing.Count -ne 0) {
        throw "These fields lost their printed explanation: $($missing -join ' | ')"
    }
    $blank = @($staticTexts | Where-Object { [string]::IsNullOrWhiteSpace($_) })
    if ($blank.Count -ne 0) {
        throw "$($blank.Count) label or explanation in the settings body has no text."
    }

    Write-Output "Control help tips passed: $toolCount controls explain themselves on hover, $($expectedHints.Count) fields carry a printed explanation."
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
