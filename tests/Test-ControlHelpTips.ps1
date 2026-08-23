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

    Write-Output "Control help tips passed: $toolCount controls explain themselves on hover."
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
