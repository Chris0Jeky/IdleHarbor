[CmdletBinding()]
param([string]$HostModule = (Join-Path $PSScriptRoot 'IdleHarborTestHost.psm1'))

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module $HostModule -Force
. (Join-Path $PSScriptRoot 'Remove-IdleHarborScopedTemp.ps1')
$checks = 0
function Assert-True([bool]$Condition, [string]$Message) {
  if (-not $Condition) { throw $Message }
  $script:checks++
}
$firstType = 'IdleHarbor.TestHost' -as [type]
Import-Module $HostModule -Force
Assert-True ([object]::ReferenceEquals($firstType, ('IdleHarbor.TestHost' -as [type]))) 'Repeated import must reuse the native type.'

$fixture = Join-Path ([IO.Path]::GetTempPath()) ('IdleHarbor-host-regression-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$probe = Join-Path $fixture 'IdleHarbor.exe'
$compiler = Join-Path $fixture 'compile.ps1'
try {
  # A harmless process fixture holds settings open; it never creates a window or sends input.
  @'
param([string]$Output)
Add-Type -OutputAssembly $Output -OutputType ConsoleApplication -TypeDefinition @"
using System;
using System.IO;
using System.Threading;
public static class NativeHostProbe {
    public static void Main(string[] args) {
        string exe = System.Reflection.Assembly.GetExecutingAssembly().Location;
        if (args.Length == 1 && args[0] == "--exit") {
            File.WriteAllText(exe + ".exit-pid", System.Diagnostics.Process.GetCurrentProcess().Id.ToString());
            if (File.Exists(exe + ".hang-exit")) Thread.Sleep(60000);
            if (!File.Exists(exe + ".hang-owner")) File.WriteAllText(File.ReadAllText(exe + ".config-path") + ".stop", "");
            return;
        }
        File.WriteAllLines(exe + ".argv", args);
        string config = args[2];
        File.WriteAllText(exe + ".config-path", config);
        using (FileStream file = File.Open(config, FileMode.Create, FileAccess.ReadWrite, FileShare.None)) {
            File.WriteAllText(config + ".ready", "");
            while (!File.Exists(config + ".stop")) Thread.Sleep(10);
        }
    }
}
"@
'@ | Set-Content -LiteralPath $compiler -Encoding ascii
  $windowsPowerShell = Join-Path ([Environment]::GetFolderPath('System')) 'WindowsPowerShell/v1.0/powershell.exe'
  & $windowsPowerShell -NoProfile -ExecutionPolicy Bypass -File $compiler -Output $probe
  Assert-True ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $probe)) 'The native process fixture must compile.'

  foreach ($mode in @('graceful', 'hang-owner', 'hang-exit', 'hang-command', 'launch-failure')) {
    $testHost = New-IdleHarborTestHost -Executable $probe -LeafPrefix 'IdleHarbor-help-tips-' -TestName 'host regression'
    $testHost.ConfigPath = Join-Path $testHost.TempRoot 'settings with spaces.ini'
    foreach ($flag in @('hang-owner', 'hang-exit')) {
      if ($mode -eq $flag -or ($mode -eq 'hang-command' -and $flag -eq 'hang-exit') -or
          ($mode -eq 'launch-failure' -and $flag -eq 'hang-owner')) {
        Set-Content -LiteralPath ($probe + '.' + $flag) -Value ''
      } else { Remove-Item -LiteralPath ($probe + '.' + $flag) -ErrorAction SilentlyContinue }
    }
    $owner = $null
    $command = $null
    try {
      $owner = Start-IdleHarborTestOwner $testHost -Arguments @('--motion', 'off')
      $deadline = [DateTime]::UtcNow.AddSeconds(5)
      while (-not (Test-Path -LiteralPath ($testHost.ConfigPath + '.ready')) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 20 }
      $argv = [IO.File]::ReadAllLines($probe + '.argv')
      Assert-True ($argv.Count -eq 5 -and $argv[2] -ceq $testHost.ConfigPath -and $argv[3] -ceq '--motion' -and $argv[4] -ceq 'off') 'Launch must preserve native argument boundaries and overrides.'
      Assert-True (Test-Path -LiteralPath ($testHost.ConfigPath + '.ready')) "$mode must open the exact spaced settings path."
      $refusal = $null
      try { New-IdleHarborTestHost -Executable $probe -LeafPrefix 'IdleHarbor-help-tips-' -TestName 'host regression' | Out-Null }
      catch { $refusal = $_.Exception.Message }
      Assert-True ($refusal -ceq 'Close every running IdleHarbor instance before the host regression.') 'The single-instance precondition must refuse an existing owner.'
      if ($mode -eq 'graceful') {
        Assert-True ((Wait-IdleHarborTestWindow $owner.Id -TimeoutSeconds 1) -eq [IntPtr]::Zero) 'Discovery must not return an unrelated window.'
      }
      if ($mode -eq 'hang-command') {
        $command = Start-Process -FilePath $probe -ArgumentList '--exit' -WindowStyle Hidden -PassThru
        Assert-True (-not $command.WaitForExit(200)) 'The earlier command must still be running at teardown.'
      }
      if ($mode -eq 'launch-failure') { $testHost.Executable = Join-Path $fixture 'missing.exe' }
      $caught = $null
      $shutdown = $null
      try {
        try { if ($mode -eq 'launch-failure') { throw 'original native assertion' } }
        finally { $shutdown = Stop-IdleHarborTestHost $testHost $owner -LeafPattern 'IdleHarbor-help-tips-*' -RefusalNoun 'host regression directory' -WaitMilliseconds 1000 -Commands @($command, $null) }
      } catch { $caught = $_.Exception.Message }
      Assert-True ($owner.WaitForExit(2000)) "$mode must terminate the owned process before cleanup."
      if ($mode -eq 'graceful') { Assert-True ($owner.ExitCode -eq 0) 'A cooperative owner must exit normally rather than be killed.' }
      Assert-True (-not (Test-Path -LiteralPath $testHost.TempRoot)) "$mode must remove the formerly locked settings directory."
      if ($mode -eq 'hang-command') {
        Assert-True ($null -eq (Get-Process -Id $command.Id -ErrorAction SilentlyContinue)) 'Teardown must reap an earlier hung command too.'
      }
      if ($mode -eq 'launch-failure') {
        Assert-True ($caught -ceq 'original native assertion' -and $shutdown -like 'Could not forward test shutdown:*') 'A shutdown error must be returned without masking the original assertion.'
      } else {
        Assert-True ($null -eq $shutdown) "$mode cleanup must succeed: $shutdown"
        $exitId = [int][IO.File]::ReadAllText($probe + '.exit-pid')
        Assert-True ($null -eq (Get-Process -Id $exitId -ErrorAction SilentlyContinue)) "$mode must reap the exit command too."
      }
    } finally {
      if ($null -ne $command) {
        if (-not $command.HasExited) { $command.Kill(); [void]$command.WaitForExit(5000) }
        $command.Dispose()
      }
      if ($null -ne $owner) {
        if (-not $owner.HasExited) { $owner.Kill(); [void]$owner.WaitForExit(5000) }
        $owner.Dispose()
      }
      if (Test-Path -LiteralPath ($probe + '.exit-pid')) {
        $exitProcess = Get-Process -Id ([int][IO.File]::ReadAllText($probe + '.exit-pid')) -ErrorAction SilentlyContinue
        if ($null -ne $exitProcess -and $exitProcess.Path -eq $probe) { $exitProcess.Kill(); [void]$exitProcess.WaitForExit(5000); $exitProcess.Dispose() }
      }
      $cleanup = Remove-IdleHarborScopedTemp $testHost.TempRoot 'IdleHarbor-help-tips-*' 'host regression directory'
      if ($cleanup) { Write-Warning $cleanup }
    }
  }
} finally {
  $cleanup = Remove-IdleHarborScopedTemp $fixture 'IdleHarbor-host-regression-*' 'host fixture directory'
  if ($cleanup) { Write-Warning $cleanup }
}
Write-Host "Native test host passed ($checks checks)."
