Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Remove-IdleHarborScopedTemp.ps1')

if (-not ('IdleHarbor.TestHost' -as [type])) {
  Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
namespace IdleHarbor {
public static class TestHost {
    public delegate bool EnumWindowsProc(IntPtr window, IntPtr data);
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr data);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr window, StringBuilder buffer, int length);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);

    public static IntPtr FindMainWindow(uint targetProcessId) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((window, data) => {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId != targetProcessId) return true;
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
}
}
'@
}

function New-IdleHarborTestHost {
  param([string]$Executable, [string]$LeafPrefix, [string]$TestName)
  if (@(Get-Process -Name IdleHarbor -ErrorAction SilentlyContinue).Count -ne 0) {
    throw "Close every running IdleHarbor instance before the $TestName."
  }
  $executablePath = (Resolve-Path -LiteralPath $Executable -ErrorAction Stop).Path
  $tempRoot = Join-Path ([IO.Path]::GetTempPath()) ($LeafPrefix + [Guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Path $tempRoot -ErrorAction Stop | Out-Null
  [pscustomobject]@{ Executable = $executablePath; TempRoot = $tempRoot; ConfigPath = Join-Path $tempRoot 'settings.ini' }
}

function Start-IdleHarborTestOwner {
  param($TestHost, [string]$ConfigPath = $TestHost.ConfigPath, [string[]]$Arguments = @())
  # Start-Process joins arguments; the config is a file path, so it cannot end in a backslash.
  Start-Process -FilePath $TestHost.Executable -ArgumentList (@('--show', '--config', ('"' + $ConfigPath + '"')) + $Arguments) -PassThru
}

function Wait-IdleHarborTestWindow {
  param([int]$ProcessId, [int]$TimeoutSeconds = 10)
  # Initialize shows the window only after publishing the initial status and controls.
  $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
  do {
    Start-Sleep -Milliseconds 100
    $window = [IdleHarbor.TestHost]::FindMainWindow([uint32]$ProcessId)
    if ($window -ne [IntPtr]::Zero) { return $window }
  } while ([DateTime]::UtcNow -lt $deadline)
  return [IntPtr]::Zero
}

function Stop-IdleHarborTestProcess {
  param([Diagnostics.Process]$Process, [int]$WaitMilliseconds, [string]$FailureMessage)
  if ($Process.WaitForExit($WaitMilliseconds)) { return }
  try { $Process.Kill() }
  catch {
    # A timed-out process may exit naturally before Kill reaches it. Only
    # suppress that error when a fresh process status confirms shutdown.
    $Process.Refresh()
    if (-not $Process.HasExited) { throw }
  }
  if (-not $Process.WaitForExit($WaitMilliseconds)) { throw $FailureMessage }
}

function Stop-IdleHarborTestHost {
  param($TestHost, [Diagnostics.Process]$Process, [string]$LeafPattern, [string]$RefusalNoun,
        [ValidateRange(1, 60000)][int]$WaitMilliseconds = 5000,
        [Diagnostics.Process[]]$Commands = @())
  # Return errors to be thrown after the caller's try/finally, preserving its original failure.
  $failures = @()
  if ($null -ne $Process) {
    $exitCommand = $null
    try {
      $Process.Refresh()
      if (-not $Process.HasExited) {
        try {
          $exitCommand = Start-Process -FilePath $TestHost.Executable -ArgumentList '--exit' -WindowStyle Hidden -PassThru
          Stop-IdleHarborTestProcess $exitCommand $WaitMilliseconds 'The test exit command did not terminate.'
        }
        catch { $failures += "Could not forward test shutdown: $($_.Exception.Message)" }
        Stop-IdleHarborTestProcess $Process $WaitMilliseconds 'The test owner did not terminate.'
      }
    }
    catch { $failures += "Could not stop test owner: $($_.Exception.Message)" }
    finally { if ($null -ne $exitCommand) { $exitCommand.Dispose() } }
  }
  # Earlier forwarded commands can be stuck in their own error dialog after the owner exits.
  foreach ($command in $Commands) {
    if ($null -eq $command) { continue }
    try {
      Stop-IdleHarborTestProcess $command $WaitMilliseconds 'The earlier test command did not terminate.'
    }
    catch { $failures += "Could not stop earlier test command: $($_.Exception.Message)" }
  }
  $cleanup = Remove-IdleHarborScopedTemp -Path $TestHost.TempRoot -LeafPattern $LeafPattern -RefusalNoun $RefusalNoun
  if ($cleanup) { $failures += $cleanup }
  if ($failures.Count -ne 0) { return ($failures -join [Environment]::NewLine) }
}

Export-ModuleMember -Function New-IdleHarborTestHost, Start-IdleHarborTestOwner, Wait-IdleHarborTestWindow, Stop-IdleHarborTestHost
