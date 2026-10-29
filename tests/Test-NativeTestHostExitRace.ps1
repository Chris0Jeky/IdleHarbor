[CmdletBinding()]
param([string]$HostModule = (Join-Path $PSScriptRoot 'IdleHarborTestHost.psm1'))

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Drive the real teardown function with typed process doubles. No child process,
# window, input, settings file or directory is created by this regression.
if (-not ('IdleHarbor.ExitRaceProcess' -as [type])) {
  Add-Type @'
using System;
using System.ComponentModel;
using System.Diagnostics;
namespace IdleHarbor {
public class ExitRaceProcess : Process {
    private readonly string mode;
    private bool exited;
    public int KillCalls;
    public int WaitCalls;
    public new bool Disposed;
    public ExitRaceProcess(string mode) {
        this.mode = mode;
        exited = mode == "already-exited";
    }
    public new bool HasExited { get { return exited; } }
    public new void Refresh() {
        if (mode == "refresh-failure" && KillCalls != 0)
            throw new InvalidOperationException("cannot confirm process exit");
    }
    public new bool WaitForExit(int milliseconds) {
        if (milliseconds != 37) throw new InvalidOperationException("unexpected wait bound");
        WaitCalls++;
        if (mode == "graceful") exited = true;
        return exited;
    }
    public new void Kill() {
        KillCalls++;
        if (mode == "kill-denied") throw new Win32Exception(5, "simulated kill denied");
        if (mode == "kill-invalid") throw new InvalidOperationException("process is still running");
        if (mode == "kill-timeout") return;
        exited = true;
        if (mode == "exit-race-invalid" || mode == "refresh-failure")
            throw new InvalidOperationException("process exited between wait and kill");
        if (mode == "exit-race-win32")
            throw new Win32Exception(5, "process exited between wait and kill");
    }
    public new void Dispose() { Disposed = true; }
}
}
'@
}

$checks = 0
function Assert-True([bool]$Condition, [string]$Message) {
  if (-not $Condition) { throw $Message }
  $script:checks++
}

$module = Import-Module $HostModule -Force -PassThru
$state = @{ Exit = $null; Launches = 0; Cleanups = 0 }
try {
  & $module {
    param($State)
    $script:ExitRaceFixture = $State
    function script:Start-Process {
      [CmdletBinding()]
      param([string]$FilePath, [string[]]$ArgumentList, [string]$WindowStyle, [switch]$PassThru)
      if ($FilePath -cne 'fixture-only' -or $ArgumentList.Count -ne 1 -or
          $ArgumentList[0] -cne '--exit' -or $WindowStyle -cne 'Hidden' -or -not $PassThru) {
        throw 'Unexpected shutdown launch contract.'
      }
      $script:ExitRaceFixture.Launches++
      return $script:ExitRaceFixture.Exit
    }
    function script:Remove-IdleHarborScopedTemp {
      param($Path, $LeafPattern, $RefusalNoun)
      $script:ExitRaceFixture.Cleanups++
    }
  } $state

  $cases = @(
    @{ Mode = 'already-exited'; Fails = $false; Kills = 0 },
    @{ Mode = 'graceful'; Fails = $false; Kills = 0 },
    @{ Mode = 'kill'; Fails = $false; Kills = 1 },
    @{ Mode = 'exit-race-invalid'; Fails = $false; Kills = 1 },
    @{ Mode = 'exit-race-win32'; Fails = $false; Kills = 1 },
    @{ Mode = 'kill-denied'; Fails = $true; Kills = 1; Error = 'simulated kill denied' },
    @{ Mode = 'kill-invalid'; Fails = $true; Kills = 1; Error = 'process is still running' },
    @{ Mode = 'kill-timeout'; Fails = $true; Kills = 1; Error = 'did not terminate' },
    @{ Mode = 'refresh-failure'; Fails = $true; Kills = 1; Error = 'cannot confirm process exit' }
  )
  foreach ($role in @('owner', 'exit', 'earlier')) {
    foreach ($case in $cases) {
      $subject = New-Object -TypeName IdleHarbor.ExitRaceProcess -ArgumentList $case.Mode
      $owner = $null
      $commands = @()
      $state.Exit = New-Object -TypeName IdleHarbor.ExitRaceProcess -ArgumentList 'graceful'
      $state.Launches = 0
      $state.Cleanups = 0
      if ($role -eq 'owner') { $owner = $subject }
      elseif ($role -eq 'exit') {
        $owner = New-Object -TypeName IdleHarbor.ExitRaceProcess -ArgumentList 'graceful'
        $state.Exit = $subject
      } else { $commands = @($subject) }
      $testHost = [pscustomobject]@{ Executable = 'fixture-only'; TempRoot = $null }
      $failure = Stop-IdleHarborTestHost $testHost $owner -Commands $commands -WaitMilliseconds 37 `
        -LeafPattern 'fixture-never-created-*' -RefusalNoun 'exit-race fixture'
      if ($case.Fails) {
        Assert-True (-not [string]::IsNullOrEmpty($failure)) "$role/$($case.Mode) must retain a genuine teardown failure."
        Assert-True ($failure -like "*$($case.Error)*") "$role/$($case.Mode) must preserve the genuine failure diagnostic: $failure"
      } else {
        Assert-True ([string]::IsNullOrEmpty($failure)) "$role/$($case.Mode) must not report a false teardown failure: $failure"
      }
      Assert-True ($subject.KillCalls -eq $case.Kills) "$role/$($case.Mode) must only kill after the bounded wait expires."
      Assert-True ($state.Cleanups -eq 1) "$role/$($case.Mode) must still reach scoped cleanup."
      if ($role -ne 'earlier' -and -not ($role -eq 'owner' -and $case.Mode -eq 'already-exited')) {
        Assert-True ($state.Launches -eq 1 -and $state.Exit.Disposed) "$role/$($case.Mode) must reap the forwarded command handle."
      }
    }
  }
} finally { Remove-Module $module -Force }
Write-Host "Native test host exit-race regression passed ($checks checks)."
