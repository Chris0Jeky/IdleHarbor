[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$failures = New-Object 'Collections.Generic.List[string]'
$count = 0

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

# Load the actual shutdown and ShouldProcess functions, without executing an
# installer or touching any installed files, registry values, shortcuts or tasks.
function Read-Function([string]$Path, [string]$Name) {
    $tokens = $null
    $errors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile($Path, [ref]$tokens, [ref]$errors)
    Assert-True ($errors.Count -eq 0) "Parse failed: $Path"
    $definition = $ast.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $Name
    }, $true)
    Assert-True ($null -ne $definition) "Missing function $Name in $Path"
    return [scriptblock]::Create($definition.Extent.Text)
}

foreach ($scriptName in @('install.ps1', 'uninstall.ps1')) {
    & {
        param($ScriptName)
        $path = Join-Path (Join-Path $repositoryRoot 'packaging') $ScriptName
        . (Read-Function $path 'Confirm-Change')
        . (Read-Function $path 'Stop-OwnedApplicationIfRunning')
        $current = [Diagnostics.Process]::GetCurrentProcess()
        try { $session = $current.SessionId }
        finally { $current.Dispose() }
        $state = @{ Target = 'C:\test with spaces\IdleHarbor\IdleHarbor.exe' }
        function Get-OwnedProcesses([string]$Executable) {
            Assert-True ($Executable -ceq $state.Target) 'Shutdown changed the exact executable path.'
            $state.Queries++
            if ($state.Case -eq 'none') { return }
            if ($state.Queries -gt 1 -and $state.Case -ne 'still-running') { return }
            $sessionId = $session
            if ($state.Case -eq 'foreign') { $sessionId++ }
            if ($state.Case -eq 'unknown-session') { $sessionId = $null }
            $process = [pscustomobject]@{ SessionId = $sessionId; Path = $state.Target; Kills = 0 }
            $process | Add-Member ScriptMethod Kill { throw 'Discovered applications must never be killed.' }
            return $process
        }
        function Start-Process {
            [CmdletBinding()]
            param([string]$FilePath, [string[]]$ArgumentList, [string]$WindowStyle, [switch]$PassThru, [switch]$Wait)
            $state.Launches++
            Assert-True ($FilePath -ceq $state.Target) 'Launched the wrong executable.'
            Assert-True ($ArgumentList.Count -eq 1 -and $ArgumentList[0] -ceq '--exit') 'Changed shutdown arguments.'
            $state.Unbounded = [bool]$Wait
            $state.PassThru = [bool]$PassThru
            if ($state.Case -eq 'start-error') { throw 'Start failed' }
            if ($state.Case -ne 'no-child') { return $state.Child }
        }
        function Start-Sleep([int]$Milliseconds) {
            Assert-True ($Milliseconds -gt 0 -and $Milliseconds -le 200) 'Unexpected polling interval.'
            if ($state.Elapsed.ElapsedMilliseconds -gt 7000) { throw 'Polling exceeded the test watchdog.' }
            Microsoft.PowerShell.Utility\Start-Sleep -Milliseconds $Milliseconds
        }
        $cases = @(
            @{ Name = 'none'; Error = ''; Launches = 0 },
            @{ Name = 'preview'; Error = ''; Launches = 0 },
            @{ Name = 'foreign'; Error = 'Windows session'; Launches = 0 },
            @{ Name = 'unknown-session'; Error = 'Windows session'; Launches = 0 },
            @{ Name = 'success'; Error = ''; Launches = 1 },
            @{ Name = 'timeout'; Error = 'did not finish'; Launches = 1 },
            @{ Name = 'kill-error'; Error = 'did not finish'; Launches = 1 },
            @{ Name = 'bad-exit'; Error = 'exit code'; Launches = 1 },
            @{ Name = 'no-child'; Error = 'could not be started'; Launches = 1 },
            @{ Name = 'start-error'; Error = 'Start failed'; Launches = 1 },
            @{ Name = 'wait-error'; Error = 'Wait failed'; Launches = 1 },
            @{ Name = 'still-running'; Error = 'did not exit'; Launches = 1 }
        )
        foreach ($case in $cases) {
            $state.Case = $case.Name
            $state.Queries = 0
            $state.Launches = 0
            $state.PassThru = $false
            $state.Unbounded = $false
            $state.Elapsed = [Diagnostics.Stopwatch]::StartNew()
            $state.Child = [pscustomobject]@{
                TimedOut = ($case.Name -in @('timeout', 'kill-error'))
                WaitError = ($case.Name -eq 'wait-error'); KillError = ($case.Name -eq 'kill-error')
                WaitMilliseconds = 0; Kills = 0; Disposed = $false; ExitCode = 0
            }
            if ($case.Name -eq 'bad-exit') { $state.Child.ExitCode = 1 }
            $state.Child | Add-Member ScriptMethod WaitForExit {
                param([int]$Milliseconds)
                $this.WaitMilliseconds = $Milliseconds
                if ($this.WaitError) { throw 'Wait failed' }
                return -not $this.TimedOut
            }
            $state.Child | Add-Member ScriptMethod Kill {
                $this.Kills++
                if ($this.KillError) { throw 'Kill failed' }
            }
            $state.Child | Add-Member ScriptMethod Dispose { $this.Disposed = $true }
            $WhatIfPreference = ($case.Name -eq 'preview')
            $message = ''
            try { Stop-OwnedApplicationIfRunning $state.Target }
            catch { $message = $_.Exception.Message }
            finally { $WhatIfPreference = $false }
            try {
                if ($case.Error.Length -eq 0) { Assert-True ($message.Length -eq 0) "Unexpected error: $message" }
                else { Assert-True ($message -match $case.Error) "Expected '$($case.Error)', got '$message'." }
                Assert-True ($state.Launches -eq $case.Launches) 'Wrong number of exit commands.'
                Assert-True (-not $state.Unbounded) 'The exit-command wait is unbounded.'
                if ($case.Launches -gt 0 -and $case.Name -notin @('no-child', 'start-error')) {
                    Assert-True $state.PassThru 'No process handle requested.'
                    Assert-True ($state.Child.WaitMilliseconds -eq 10000) 'Exit-command wait must be 10 seconds.'
                    Assert-True $state.Child.Disposed 'Exit-command handle leaked.'
                }
                $expectedKills = 0
                if ($case.Name -in @('timeout', 'kill-error')) { $expectedKills = 1 }
                Assert-True ($state.Child.Kills -eq $expectedKills) 'Only a timed-out command child may be killed.'
                if ($case.Name -eq 'still-running') {
                    Assert-True ($state.Elapsed.ElapsedMilliseconds -ge 4900 -and $state.Elapsed.ElapsedMilliseconds -lt 7000) 'Post-command polling did not respect its five-second budget.'
                }
                Write-Host "PASS: $ScriptName / $($case.Name)"
            }
            catch {
                $failures.Add("$ScriptName / $($case.Name): $($_.Exception.Message)")
                Write-Host "FAIL: $($failures[$failures.Count - 1])"
            }
        }
    } $scriptName
    $count += 12
}
if ($failures.Count -gt 0) { throw "$($failures.Count) of $count per-user shutdown cases failed: $($failures -join '; ')" }
Write-Host "Per-user shutdown checks passed ($count cases)."
