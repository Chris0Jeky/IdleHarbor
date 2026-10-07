# Shared by the installed package's upgrade and uninstall entry points.
function Stop-IdleHarborPackage {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [ValidateNotNullOrEmpty()]
        [string]$Executable,

        [ValidateRange(1, 60000)]
        [int]$TimeoutMilliseconds = 10000
    )

    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) { return }

    function Get-PackageOwnedProcess {
        # A failed or incomplete query cannot prove that package files are free.
        foreach ($process in @(Get-CimInstance Win32_Process -Filter "Name='IdleHarbor.exe'" -ErrorAction Stop)) {
            if ([string]::IsNullOrWhiteSpace([string]$process.ExecutablePath)) {
                throw 'Cannot verify the executable path of a running IdleHarbor process. Close IdleHarbor and retry.'
            }
            if ($process.ExecutablePath -ieq $Executable) {
                if ($null -eq $process.SessionId) {
                    throw 'Cannot verify the Windows session of the package-owned IdleHarbor process. Close it and retry.'
                }
                $process
            }
        }
    }

    # Wrap every call: PowerShell otherwise unwraps empty/single-item results.
    $running = @(Get-PackageOwnedProcess)
    if ($running.Count -eq 0) { return }
    $currentProcess = [Diagnostics.Process]::GetCurrentProcess()
    try { $session = $currentProcess.SessionId }
    finally { $currentProcess.Dispose() }
    $foreign = @($running | Where-Object { [int]$_.SessionId -ne $session })
    if ($foreign.Count -gt 0) {
        $sessions = ($foreign | ForEach-Object { [int]$_.SessionId } | Sort-Object -Unique) -join ', '
        throw ("IdleHarbor from this Chocolatey package is running in Windows session $sessions, not in " +
            "session $session where this is running, so it cannot be closed from here. Close IdleHarbor " +
            'in that session and retry.')
    }

    # --exit uses a session-local mutex/window. Even in the same session, a
    # desktop mismatch or a concurrent exit can strand the child on a dialog.
    $exitProcess = Start-Process -FilePath $Executable -ArgumentList '--exit' -PassThru -ErrorAction Stop
    if ($null -eq $exitProcess) {
        throw 'The IdleHarbor exit command could not be started. Close IdleHarbor and retry.'
    }
    try {
        if (-not $exitProcess.WaitForExit($TimeoutMilliseconds)) {
            # Only the newly launched command child is eligible for this kill.
            # Never force-terminate a discovered running application instance.
            try { $exitProcess.Kill() } catch { }
            throw "The IdleHarbor exit command did not finish within $TimeoutMilliseconds milliseconds. Close IdleHarbor and retry."
        }
        if ($exitProcess.ExitCode -ne 0) {
            throw "The IdleHarbor exit command returned exit code $($exitProcess.ExitCode). Close IdleHarbor and retry."
        }
    }
    finally { $exitProcess.Dispose() }

    # Use elapsed time rather than a wall clock that can move backwards.
    $elapsed = [Diagnostics.Stopwatch]::StartNew()
    do {
        if (@(Get-PackageOwnedProcess).Count -eq 0) { return }
        $remaining = $TimeoutMilliseconds - $elapsed.ElapsedMilliseconds
        if ($remaining -le 0) { break }
        Start-Sleep -Milliseconds ([int][Math]::Min(200, $remaining))
    } while ($elapsed.ElapsedMilliseconds -lt $TimeoutMilliseconds)
    throw 'IdleHarbor is still running from the Chocolatey package. Close it and retry.'
}
