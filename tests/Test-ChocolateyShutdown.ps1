[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$tools = Join-Path $repositoryRoot 'packaging\chocolatey\tools'
[xml]$nuspec = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'packaging\chocolatey\idleharbor.nuspec')
$version = [string]$nuspec.package.metadata.version
$root = Join-Path ([IO.Path]::GetTempPath()) ('IdleHarbor-shutdown-regression-' + [Guid]::NewGuid().ToString('N'))
$script:target = Join-Path $root "IdleHarbor-$version-windows-x64-portable\IdleHarbor.exe"
$script:session = [Diagnostics.Process]::GetCurrentProcess().SessionId
$failures = New-Object 'Collections.Generic.List[string]'
$caseCount = 0

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

# Script-local doubles are inherited by the copied production entry points.
# No global functions, live processes or installed package files are changed.
function Get-CimInstance {
    [CmdletBinding()]
    param([string]$ClassName, [string]$Filter)
    $script:queries++
    $script:queryModes += [string]$PSBoundParameters['ErrorAction']
    Assert-True ($ClassName -eq 'Win32_Process') 'Unexpected CIM class.'
    Assert-True ($Filter -eq "Name='IdleHarbor.exe'") 'Unexpected CIM filter.'
    if ($script:scenario -eq 'query-error') { Write-Error 'Process query failed'; return }
    if ($script:scenario -eq 'none') { return }
    if ($script:queries -gt 1) { return }
    $path = $script:target
    $sessionId = $script:session
    if ($script:scenario -eq 'unrelated') { $path += '.other' }
    if ($script:scenario -eq 'foreign') { $sessionId++ }
    if ($script:scenario -eq 'unknown-path') { $path = $null }
    [pscustomobject]@{ ExecutablePath = $path; SessionId = $sessionId; ProcessId = 1234 }
}

function Start-Process {
    [CmdletBinding()]
    param([string]$FilePath, [string[]]$ArgumentList, [switch]$PassThru, [switch]$Wait)
    $script:launches++
    $script:unbounded = [bool]$Wait
    $script:passThru = [bool]$PassThru
    Assert-True ($FilePath -ceq $script:target) 'Attempted to launch an unrelated executable.'
    Assert-True ($ArgumentList.Count -eq 1 -and $ArgumentList[0] -ceq '--exit') 'Unexpected shutdown command.'
    if ($script:scenario -ne 'no-child') { return $script:child }
}

function Start-Sleep {
    param([int]$Milliseconds)
    Assert-True ($Milliseconds -gt 0 -and $Milliseconds -le 200) 'Unexpected poll interval.'
}

try {
    New-Item -ItemType Directory -Path (Split-Path -Parent $script:target) -Force | Out-Null
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
            $script:scenario = $case.Name
            $script:queries = 0
            $script:queryModes = @()
            $script:launches = 0
            $script:unbounded = $false
            $script:passThru = $false
            $script:child = [pscustomobject]@{
                WaitResult = ($case.Name -ne 'hung'); ThrowOnWait = ($case.Name -eq 'wait-error')
                WaitMilliseconds = 0; Kills = 0; Disposed = $false; ExitCode = 0
            }
            if ($case.Name -eq 'bad-exit') { $script:child.ExitCode = 1 }
            $script:child | Add-Member ScriptMethod WaitForExit {
                param([int]$Milliseconds)
                $this.WaitMilliseconds = $Milliseconds
                if ($this.ThrowOnWait) { throw 'Wait failed' }
                return $this.WaitResult
            }
            $script:child | Add-Member ScriptMethod Kill { $this.Kills++ }
            $script:child | Add-Member ScriptMethod Dispose { $this.Disposed = $true }
            Set-Content -LiteralPath $script:target -Value 'Not an executable; Start-Process is doubled.' -Encoding ASCII
            if ($case.Name -eq 'absent') { Remove-Item -LiteralPath $script:target }
            $message = ''
            try { & (Join-Path $root $name) | Out-Null }
            catch { $message = $_.Exception.Message }
            try {
                if ($case.Error.Length -eq 0) { Assert-True ($message.Length -eq 0) "Unexpected exception: $message" }
                else { Assert-True ($message -match $case.Error) "Expected '$($case.Error)', got '$message'." }
                Assert-True ($script:launches -eq $case.Launches) 'Wrong number of exit-command launches.'
                Assert-True (-not $script:unbounded) 'Start-Process -Wait is unbounded.'
                Assert-True (@($script:queryModes | Where-Object { $_ -ne 'Stop' }).Count -eq 0) 'Process discovery must fail closed.'
                if ($case.Name -eq 'absent') { Assert-True ($script:queries -eq 0) 'Missing executable must not query processes.' }
                if ($case.Launches -gt 0 -and $case.Name -ne 'no-child') {
                    Assert-True $script:passThru 'Exit command must return a process handle.'
                    Assert-True ($script:child.WaitMilliseconds -eq 10000) 'Exit-command wait must be bounded to 10 seconds.'
                    Assert-True $script:child.Disposed 'Exit-command process handle was not disposed.'
                }
                $expectedKills = 0
                if ($case.Name -eq 'hung') { $expectedKills = 1 }
                Assert-True ($script:child.Kills -eq $expectedKills) 'Only a timed-out exit-command child may be killed.'
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
