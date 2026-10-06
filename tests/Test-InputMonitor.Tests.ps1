[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$cppPath = Join-Path $repoRoot 'src/platform/windows/input_monitor.cpp'
$hppPath = Join-Path $repoRoot 'include/idleharbor/platform/windows/input_monitor.hpp'

Assert-True (Test-Path -LiteralPath $cppPath -PathType Leaf) "Missing $cppPath."
Assert-True (Test-Path -LiteralPath $hppPath -PathType Leaf) "Missing $hppPath."

$cpp = Get-Content -Raw -LiteralPath $cppPath
$hpp = Get-Content -Raw -LiteralPath $hppPath

$startMarker = 'InputMonitor::Start'
$refreshMarker = 'InputMonitor::Refresh'
$startIndex = $cpp.IndexOf($startMarker, [StringComparison]::Ordinal)
Assert-True ($startIndex -ge 0) 'Start definition was not found in input_monitor.cpp.'
$refreshIndex = $cpp.IndexOf($refreshMarker, $startIndex, [StringComparison]::Ordinal)
Assert-True ($refreshIndex -gt $startIndex) 'Refresh definition was not found after Start in input_monitor.cpp.'
$startBody = $cpp.Substring($startIndex, $refreshIndex - $startIndex)

$nullCheck = $startBody.IndexOf('notification_window == nullptr', [StringComparison]::Ordinal)
if ($nullCheck -lt 0) {
    $nullCheck = $startBody.IndexOf('notification_window==nullptr', [StringComparison]::Ordinal)
}
Assert-True ($nullCheck -ge 0) 'Start lacks a null-window guard (notification_window == nullptr).'

$zeroCheck = $startBody.IndexOf('notification_message == 0', [StringComparison]::Ordinal)
if ($zeroCheck -lt 0) {
    $zeroCheck = $startBody.IndexOf('notification_message==0', [StringComparison]::Ordinal)
}
Assert-True ($zeroCheck -ge 0) 'Start lacks a zero-message guard (notification_message == 0).'

$guardIndex = [Math]::Min($nullCheck, $zeroCheck)

$returnIndex = $startBody.IndexOf('return {}', $guardIndex, [StringComparison]::Ordinal)
Assert-True ($returnIndex -gt $guardIndex) 'Start guard does not return {} after the null/zero check.'

$refreshCallIndex = $startBody.IndexOf('Refresh()', [StringComparison]::Ordinal)
Assert-True ($refreshCallIndex -gt 0) 'Start no longer calls Refresh() for valid arguments.'
Assert-True ($returnIndex -lt $refreshCallIndex) 'Start guard must return {} before calling Refresh().'

$firstHookIndex = $cpp.IndexOf('SetWindowsHookEx', [StringComparison]::Ordinal)
Assert-True ($firstHookIndex -gt 0) 'SetWindowsHookEx was not found; Refresh hook installation may have been reworked.'
$guardAbsoluteIndex = $startIndex + $guardIndex
$returnAbsoluteIndex = $startIndex + $returnIndex
Assert-True ($returnAbsoluteIndex -lt $firstHookIndex) 'Start guard must return {} before any SetWindowsHookEx hook installation.'

$afterGuard = $startBody.Substring($guardIndex)
Assert-True (($afterGuard.IndexOf('Stop()', [StringComparison]::Ordinal) -ge 0) -or `
    ($afterGuard.IndexOf('active_monitor_', [StringComparison]::Ordinal) -ge 0)) `
    'Start guard must release active_monitor_ (via Stop() or active_monitor_) with no hooks installed.'

Assert-True ($startBody.IndexOf('notification_window_ = notification_window', [StringComparison]::Ordinal) -gt $returnIndex) `
    'Start must store notification state only after the guard (valid-args behavior changed).'
Assert-True ($startBody.IndexOf('return capabilities()', [StringComparison]::Ordinal) -ge 0) `
    'Start must still return capabilities() for valid arguments.'

$headerDocuments = $hpp -match '(?s)non-null.*notification|notification.*non-null|nonzero.*message|message.*nonzero|Null/zero.*capabilit|precondition.*Start|Start.*requires'
Assert-True $headerDocuments 'Header must document the Start non-null window / nonzero message precondition.'

Write-Host 'InputMonitor Start contract checks passed.'
