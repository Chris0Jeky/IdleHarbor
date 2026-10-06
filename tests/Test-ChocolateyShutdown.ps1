[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$tools = Join-Path $repositoryRoot 'packaging\chocolatey\tools'
[xml]$nuspec = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'packaging\chocolatey\idleharbor.nuspec')
$version = [string]$nuspec.package.metadata.version
$root = Join-Path ([IO.Path]::GetTempPath()) ('IdleHarbor-shutdown-regression-' + [Guid]::NewGuid().ToString('N'))
$shutdownState = @{}
$shutdownState.target = Join-Path $root "IdleHarbor-$version-windows-x64-portable\IdleHarbor.exe"
$shutdownState.session = [Diagnostics.Process]::GetCurrentProcess().SessionId
$failures = New-Object 'Collections.Generic.List[string]'
$caseCount = 0

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

# Script-local doubles share a reference object with the copied entry points.
# Do not use script: variables: that qualifier rebinds in a called script.
# No global functions, live processes or installed package files are changed.
function Get-CimInstance {
    [CmdletBinding()]
    param([string]$ClassName, [string]$Filter)
    $shutdownState.queries++
    $shutdownState.queryModes += [string]$PSBoundParameters['ErrorAction']
    Assert-True ($ClassName -eq 'Win32_Process') 'Unexpected CIM class.'
    Assert-True ($Filter -eq "Name='IdleHarbor.exe'") 'Unexpected CIM filter.'
    if ($shutdownState.scenario -eq 'query-error') { Write-Error 'Process query failed'; return }
    if ($shutdownState.scenario -eq 'none') { return }
    if ($shutdownState.queries -gt 1) { return }
    $path = $shutdownState.target
    $sessionId = $shutdownState.session
    if ($shutdownState.scenario -eq 'unrelated') { $path += '.other' }
    if ($shutdownState.scenario -eq 'foreign') { $sessionId++ }
    if ($shutdownState.scenario -eq 'unknown-path') { $path = $null }
    [pscustomobject]@{ ExecutablePath = $path; SessionId = $sessionId; ProcessId = 1234 }
}

function Start-Process {
    [CmdletBinding()]
    param([string]$FilePath, [string[]]$ArgumentList, [switch]$PassThru, [switch]$Wait)
    $shutdownState.launches++
    $shutdownState.unbounded = [bool]$Wait
    $shutdownState.passThru = [bool]$PassThru
    Assert-True ($FilePath -ceq $shutdownState.target) 'Attempted to launch an unrelated executable.'
    Assert-True ($ArgumentList.Count -eq 1 -and $ArgumentList[0] -ceq '--exit') 'Unexpected shutdown command.'
    if ($shutdownState.scenario -ne 'no-child') { return $shutdownState.child }
}

function Start-Sleep {
    param([int]$Milliseconds)
    Assert-True ($Milliseconds -gt 0 -and $Milliseconds -le 200) 'Unexpected poll interval.'
}

try {
    New-Item -ItemType Directory -Path (Split-Path -Parent $shutdownState.target) -Force | Out-Null
    $scripts = @('chocolateyBeforeModify.ps1', 'chocolateyUninstall.ps1')
    foreach ($name in $scripts) { Copy-Item -LiteralPath (Join-Path $tools $name) -Destination (Join-Path $root $name) }
    $helper = Join-Path $tools 'Stop-IdleHarborPackage.ps1'
    if (Test-Path -LiteralPath $helper) { Copy-Item -LiteralPath $helper -Destination $root }

    $cases = @(
        @{ Name = 'absent'; Error = ''; Launches = 0 },
        @{ Name = 'none'; Error = ''; Launches = 0 },
        @{ Name = 'unrelated'; Error = ''; Launches = 0 },
        @{ Name = 'foreign'; Error = 'Windows session'; Launches = 0 },
        @{ Name = 'unknown-path'; Error = 'executable path'; Launches = 0 },
        @{ Name = 'query-error'; Error = 'Process query failed'; Launches = 0 },
        @{ Name = 'success'; Error = ''; Launches = 1 },
        @{ Name = 'hung'; Error = 'did not finish'; Launches = 1 },
        @{ Name = 'bad-exit'; Error = 'exit code'; Launches = 1 },
        @{ Name = 'no-child'; Error = 'could not be started'; Launches = 1 },
        @{ Name = 'wait-error'; Error = 'Wait failed'; Launches = 1 }
    )
    foreach ($name in $scripts) {
        foreach ($case in $cases) {
            $caseCount++
            $shutdownState.scenario = $case.Name
            $shutdownState.queries = 0
            $shutdownState.queryModes = @()
            $shutdownState.launches = 0
            $shutdownState.unbounded = $false
            $shutdownState.passThru = $false
            $shutdownState.child = [pscustomobject]@{
                WaitResult = ($case.Name -ne 'hung'); ThrowOnWait = ($case.Name -eq 'wait-error')
                WaitMilliseconds = 0; Kills = 0; Disposed = $false; ExitCode = 0
            }
            if ($case.Name -eq 'bad-exit') { $shutdownState.child.ExitCode = 1 }
            $shutdownState.child | Add-Member ScriptMethod WaitForExit {
                param([int]$Milliseconds)
                $this.WaitMilliseconds = $Milliseconds
                if ($this.ThrowOnWait) { throw 'Wait failed' }
                return $this.WaitResult
            }
            $shutdownState.child | Add-Member ScriptMethod Kill { $this.Kills++ }
            $shutdownState.child | Add-Member ScriptMethod Dispose { $this.Disposed = $true }
            Set-Content -LiteralPath $shutdownState.target -Value 'Not an executable; Start-Process is doubled.' -Encoding ASCII
            if ($case.Name -eq 'absent') { Remove-Item -LiteralPath $shutdownState.target }
            $message = ''
            try { & (Join-Path $root $name) | Out-Null }
            catch { $message = $_.Exception.Message }
            try {
                if ($case.Error.Length -eq 0) { Assert-True ($message.Length -eq 0) "Unexpected exception: $message" }
                else { Assert-True ($message -match $case.Error) "Expected '$($case.Error)', got '$message'." }
                Assert-True ($shutdownState.launches -eq $case.Launches) 'Wrong number of exit-command launches.'
                Assert-True (-not $shutdownState.unbounded) 'Start-Process -Wait is unbounded.'
                Assert-True (@($shutdownState.queryModes | Where-Object { $_ -ne 'Stop' }).Count -eq 0) 'Process discovery must fail closed.'
                if ($case.Name -eq 'absent') { Assert-True ($shutdownState.queries -eq 0) 'Missing executable must not query processes.' }
                if ($case.Launches -gt 0 -and $case.Name -ne 'no-child') {
                    Assert-True $shutdownState.passThru 'Exit command must return a process handle.'
                    Assert-True ($shutdownState.child.WaitMilliseconds -eq 10000) 'Exit-command wait must be bounded to 10 seconds.'
                    Assert-True $shutdownState.child.Disposed 'Exit-command process handle was not disposed.'
                }
                $expectedKills = 0
                if ($case.Name -eq 'hung') { $expectedKills = 1 }
                Assert-True ($shutdownState.child.Kills -eq $expectedKills) 'Only a timed-out exit-command child may be killed.'
                Write-Host "PASS: $name / $($case.Name)"
            }
            catch {
                $failure = "$name / $($case.Name): $($_.Exception.Message)"
                $failures.Add($failure)
                Write-Host "FAIL: $failure"
            }
        }
    }
    if ($failures.Count -gt 0) { throw "$($failures.Count) of $caseCount Chocolatey shutdown cases failed: $($failures -join '; ')" }
    Write-Host "Chocolatey shutdown regression checks passed ($caseCount cases)."
}
finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
