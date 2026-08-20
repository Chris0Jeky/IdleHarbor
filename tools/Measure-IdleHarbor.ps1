[CmdletBinding()]
param(
    [string]$Executable = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build\Release\IdleHarbor.exe'),

    [ValidateRange(3, 3600)]
    [int]$DurationSeconds = 30,

    [ValidateRange(1, 20)]
    [int]$Runs = 3,

    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ProcessSnapshot([Diagnostics.Process]$Process) {
    $Process.Refresh()
    return [pscustomobject]@{
        WorkingSetBytes = [int64]$Process.WorkingSet64
        PrivateBytes = [int64]$Process.PrivateMemorySize64
        Handles = [int]$Process.HandleCount
        Threads = [int]$Process.Threads.Count
        CpuMilliseconds = [double]$Process.TotalProcessorTime.TotalMilliseconds
    }
}

function Measure-Phase([Diagnostics.Process]$Process, [string]$Name, [int]$Seconds) {
    $samples = [Collections.Generic.List[object]]::new()
    $started = [Diagnostics.Stopwatch]::StartNew()
    $before = Get-ProcessSnapshot $Process
    while ($started.Elapsed.TotalSeconds -lt $Seconds) {
        Start-Sleep -Milliseconds 500
        if ($Process.HasExited) { throw "IdleHarbor exited during the $Name measurement." }
        $samples.Add((Get-ProcessSnapshot $Process))
    }
    $started.Stop()
    $after = Get-ProcessSnapshot $Process
    $logicalProcessors = [Math]::Max([Environment]::ProcessorCount, 1)
    $cpuDelta = [Math]::Max($after.CpuMilliseconds - $before.CpuMilliseconds, 0)
    $normalizedCpu = $cpuDelta / ($started.Elapsed.TotalMilliseconds * $logicalProcessors) * 100

    return [ordered]@{
        phase = $Name
        elapsedSeconds = [Math]::Round($started.Elapsed.TotalSeconds, 3)
        samples = $samples.Count
        cpuTimeMilliseconds = [Math]::Round($cpuDelta, 3)
        normalizedCpuPercent = [Math]::Round($normalizedCpu, 4)
        workingSetMiB = [Math]::Round((($samples | Measure-Object WorkingSetBytes -Average).Average / 1MB), 3)
        peakWorkingSetMiB = [Math]::Round((($samples | Measure-Object WorkingSetBytes -Maximum).Maximum / 1MB), 3)
        privateBytesMiB = [Math]::Round((($samples | Measure-Object PrivateBytes -Average).Average / 1MB), 3)
        handles = [int](($samples | Measure-Object Handles -Maximum).Maximum)
        threads = [int](($samples | Measure-Object Threads -Maximum).Maximum)
    }
}

function Invoke-IdleHarborCommand([string[]]$Arguments) {
    $command = Start-Process -FilePath $script:ExecutablePath -ArgumentList $Arguments -WindowStyle Hidden -PassThru -Wait
    if ($command.ExitCode -ne 0) {
        throw "IdleHarbor command failed with exit code $($command.ExitCode): $($Arguments -join ' ')"
    }
}

$ExecutablePath = (Resolve-Path -LiteralPath $Executable).Path
$existing = @(Get-Process -Name 'IdleHarbor' -ErrorAction SilentlyContinue)
if ($existing.Count -ne 0) {
    throw 'Close existing IdleHarbor instances before running a controlled measurement.'
}

$file = Get-Item -LiteralPath $ExecutablePath
$hash = Get-FileHash -LiteralPath $ExecutablePath -Algorithm SHA256
$results = [Collections.Generic.List[object]]::new()

for ($run = 1; $run -le $Runs; ++$run) {
    $configPath = Join-Path ([IO.Path]::GetTempPath()) "IdleHarbor-benchmark-$PID-$run.ini"
    $process = $null
    try {
        $process = Start-Process `
            -FilePath $ExecutablePath `
            -ArgumentList @('--minimized', '--no-close-to-tray', '--config', $configPath) `
            -WindowStyle Hidden `
            -PassThru
        Start-Sleep -Milliseconds 750
        $process.Refresh()
        if ($process.HasExited) { throw 'IdleHarbor did not remain open for measurement.' }

        $stopped = Measure-Phase $process 'stopped' $DurationSeconds
        Invoke-IdleHarborCommand @(
            '--start',
            '--minimized',
            '--profile', 'long-task',
            '--pause-on-input', '0',
            '--battery-threshold', '0',
            '--no-pause-on-fullscreen'
        )
        Start-Sleep -Milliseconds 250
        $active = Measure-Phase $process 'active-system-request' $DurationSeconds
        Invoke-IdleHarborCommand @('--stop')

        $results.Add([ordered]@{ run = $run; stopped = $stopped; active = $active })
    }
    finally {
        if ($null -ne $process -and -not $process.HasExited) {
            try { Invoke-IdleHarborCommand @('--exit') } catch { }
            $process.WaitForExit(5000) | Out-Null
        }
        if (Test-Path -LiteralPath $configPath -PathType Leaf) {
            Remove-Item -LiteralPath $configPath -Force
        }
    }
}

$document = [ordered]@{
    schema = 1
    measuredAtUtc = [DateTime]::UtcNow.ToString('o')
    product = 'IdleHarbor'
    version = $file.VersionInfo.ProductVersion
    executableBytes = [int64]$file.Length
    executableSha256 = $hash.Hash.ToLowerInvariant()
    architecture = $env:PROCESSOR_ARCHITECTURE
    windowsVersion = [Environment]::OSVersion.VersionString
    logicalProcessors = [Environment]::ProcessorCount
    durationSecondsPerPhase = $DurationSeconds
    runs = $results
}

$json = $document | ConvertTo-Json -Depth 10
if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    $destination = [IO.Path]::GetFullPath($OutputPath)
    $parent = Split-Path -Parent $destination
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    $json | Set-Content -LiteralPath $destination -Encoding UTF8
}
$json
