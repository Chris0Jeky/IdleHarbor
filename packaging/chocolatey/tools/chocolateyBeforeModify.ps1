$ErrorActionPreference = 'Stop'

# Chocolatey runs this from the *installed* package before an upgrade or an
# uninstall. chocolateyUninstall.ps1 is not run on upgrade, so without this the
# running executable keeps a lock on its own package directory and the upgrade
# fails or leaves a mixed install behind.
$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$executable = Join-Path $toolsDir 'IdleHarbor-0.2.0-windows-x64-portable\IdleHarbor.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    return
}

function Get-PackageOwnedProcess {
    return @(Get-CimInstance Win32_Process -Filter "Name='IdleHarbor.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.ExecutablePath -eq $executable })
}

$running = Get-PackageOwnedProcess
if ($running.Count -eq 0) {
    return
}

# That CIM query sees every session on the machine, but --exit only reaches an
# instance in this one: IdleHarbor guards itself with the session-local mutex
# Local\IdleHarbor.Singleton.v1 and hands the command to a window on the caller's
# own desktop. An unattended upgrade running as SYSTEM, or one started from a
# second signed-in account, would launch a process that finds no instance and
# stops on a modal "IdleHarbor is not currently running" dialog -- on session 0
# there is not even a desktop to dismiss it from. Say so now instead of waiting.
$session = [System.Diagnostics.Process]::GetCurrentProcess().SessionId
$foreign = @($running | Where-Object { [int]$_.SessionId -ne $session })
if ($foreign.Count -gt 0) {
    $sessions = ($foreign | ForEach-Object { [int]$_.SessionId } | Sort-Object -Unique) -join ', '
    throw ("IdleHarbor from this Chocolatey package is running in Windows session $sessions, not in " +
        "session $session where this is running, so it cannot be closed from here. Close IdleHarbor " +
        'in that session and retry.')
}

# --exit is IdleHarbor's own visible shutdown command: it stops any session,
# clears the power request, and removes the notification-area icon. Wait for a
# bounded time rather than indefinitely -- it also reports failures in a modal
# dialog, and an unattended upgrade has nobody to close one.
$exitProcess = Start-Process -FilePath $executable -ArgumentList '--exit' -PassThru
if ($null -eq $exitProcess) {
    throw 'The IdleHarbor exit command could not be started. Close IdleHarbor and retry.'
}
if (-not $exitProcess.WaitForExit(10000)) {
    try { $exitProcess.Kill() } catch { }
    throw 'The IdleHarbor exit command did not finish within 10 seconds. Close IdleHarbor and retry.'
}

$deadline = [DateTime]::UtcNow.AddSeconds(10)
do {
    if ((Get-PackageOwnedProcess).Count -eq 0) {
        return
    }
    Start-Sleep -Milliseconds 200
} while ([DateTime]::UtcNow -lt $deadline)

throw 'IdleHarbor is still running from the Chocolatey package. Close it and retry.'
