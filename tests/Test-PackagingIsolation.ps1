[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$suite = Join-Path (Split-Path -Parent $PSScriptRoot) 'packaging\Test-Packaging.ps1'
$beforeCommand = Get-Command Get-ScheduledTask -ListImported -ErrorAction SilentlyContinue
$beforeModule = Get-Module ScheduledTasks
$beforeValue = Get-Variable IdleHarborPackagingTestScheduledTask -Scope Global -ErrorAction SilentlyContinue
$failures = New-Object 'Collections.Generic.List[string]'
function Check([bool]$Condition, [string]$Message) {
    if ($Condition) { Write-Host "PASS: $Message" }
    else { $failures.Add($Message); Write-Host "FAIL: $Message" }
}
try {
    function global:Get-ScheduledTask {
        [CmdletBinding()]
        param([string]$TaskPath, [string]$TaskName)
        return 'caller-owned scheduler command'
    }
    $sentinel = (Get-Command Get-ScheduledTask -ListImported).ScriptBlock.ToString()
    $value = [pscustomobject]@{ Owner = 'caller'; Token = [Guid]::NewGuid().ToString('N') }
    $global:IdleHarborPackagingTestScheduledTask = $value
    $output = @(& $suite)
    Check (($output -join "`n") -match 'Packaging checks passed\.') 'Real packaging suite completes with a caller-owned command'
    $after = Get-Command Get-ScheduledTask -ListImported -ErrorAction SilentlyContinue
    Check ($null -ne $after -and $after.CommandType -eq 'Function' -and $after.ScriptBlock.ToString() -ceq $sentinel) 'Caller-owned scheduler function survives the suite'
    $afterValue = Get-Variable IdleHarborPackagingTestScheduledTask -Scope Global -ErrorAction SilentlyContinue
    Check ($null -ne $afterValue -and [object]::ReferenceEquals($value, $afterValue.Value)) 'Caller-owned scheduler fixture variable survives the suite'

    Remove-Item -LiteralPath Function:\Get-ScheduledTask -ErrorAction SilentlyContinue
    Import-Module ScheduledTasks -Global -Force
    $moduleCommand = Get-Command Get-ScheduledTask -ListImported
    Check ($moduleCommand.ModuleName -eq 'ScheduledTasks') 'Native ScheduledTasks command is available before the suite'
    $output = @(& $suite)
    $after = Get-Command Get-ScheduledTask -ListImported
    Check ($after.ModuleName -eq 'ScheduledTasks') 'Native ScheduledTasks command remains available after the suite'
    # A real read-only query, not just Get-Command lookup, proves the binding works.
    Get-ScheduledTask -ErrorAction Stop | Out-Null
    Check (($output -join "`n") -match 'Packaging checks passed\.') 'Real packaging suite and subsequent ScheduledTasks query both complete'

    if ($failures.Count -gt 0) { throw "$($failures.Count) caller-isolation assertions failed: $($failures -join '; ')" }
    Write-Host 'Packaging caller isolation checks passed (two complete suite executions).'
}
finally {
    Remove-Item -LiteralPath Function:\Get-ScheduledTask -ErrorAction SilentlyContinue
    if ($null -ne $beforeCommand -and $beforeCommand.CommandType -eq 'Function' -and [string]::IsNullOrEmpty($beforeCommand.ModuleName)) {
        Set-Item -Path Function:global:Get-ScheduledTask -Value $beforeCommand.ScriptBlock
    }
    elseif ($null -ne $beforeModule) { Import-Module ScheduledTasks -Global -Force }
    else { Remove-Module ScheduledTasks -ErrorAction SilentlyContinue }
    if ($null -ne $beforeValue) {
        Set-Variable -Name IdleHarborPackagingTestScheduledTask -Scope Global -Value $beforeValue.Value
    }
    else { Remove-Variable IdleHarborPackagingTestScheduledTask -Scope Global -ErrorAction SilentlyContinue }
}
