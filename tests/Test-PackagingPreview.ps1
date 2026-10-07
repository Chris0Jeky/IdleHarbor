[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$packaging = Join-Path $repositoryRoot 'packaging'
$root = Join-Path ([IO.Path]::GetTempPath()) ('IdleHarbor-preview-test-' + [Guid]::NewGuid().ToString('N'))
$source = Join-Path $root 'source with spaces'
$target = Join-Path $root 'IdleHarbor'
$failures = New-Object 'Collections.Generic.List[string]'

# Keep discovery isolated in child scripts without replacing global commands.
# This fixture uses Startup None and StartMenu None and never runs a real app.
function Get-ScheduledTask {
    [CmdletBinding()]
    param([string]$TaskPath, [string]$TaskName)
    return
}
function Get-Process {
    [CmdletBinding()]
    param([string]$Name)
    return
}
function Start-Process { throw 'Preview fixture must not launch a process.' }
function Register-ScheduledTask { throw 'Preview fixture must not register a task.' }
function Unregister-ScheduledTask { throw 'Preview fixture must not remove a task.' }
function Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { $failures.Add($Message); Write-Host "FAIL: $Message" }
    else { Write-Host "PASS: $Message" }
}
function Snapshot([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return '<absent>' }
    return (@(Get-ChildItem -LiteralPath $Path -Recurse -Force | Sort-Object FullName | ForEach-Object {
        $relative = $_.FullName.Substring($Path.Length)
        if ($_.PSIsContainer) { "directory:$relative" }
        else { "${relative}:$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)" }
    }) -join "`n")
}
function Text([object[]]$Records) { return ($Records | ForEach-Object { $_.ToString() }) -join "`n" }
function Check-Preview([object[]]$Records, [string]$Action) {
    $text = Text $Records
    Check ($text -match '(?im)^Previewed|^Would ') "$Action summary is explicitly preview-only"
    Check ($text -notmatch '(?im)^Installed |^Uninstalled ') "$Action does not claim completed mutation"
}
try {
    New-Item -ItemType Directory -Path $source -Force | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $source 'IdleHarbor.exe'), [byte[]](0x4d, 0x5a, 0, 1))
    Set-Content -LiteralPath (Join-Path $source 'README.md') -Value 'fixture readme' -Encoding ASCII
    $parameters = @{ SourcePath = $source; InstallRoot = $target; Startup = 'None'; StartMenu = 'None'; NoLaunch = $true; Confirm = $false }
    $sourceBefore = Snapshot $source

    $output = @(& (Join-Path $packaging 'install.ps1') @parameters -WhatIf 3>&1 6>$null)
    Check-Preview $output 'Fresh install'
    Check (-not (Test-Path -LiteralPath $target)) 'Fresh preview creates no target directory'
    Check ((Snapshot $source) -ceq $sourceBefore) 'Fresh preview preserves every source byte'

    $output = @(& (Join-Path $packaging 'install.ps1') @parameters 3>&1 6>$null)
    Check ((Text $output) -match '(?m)^Installed IdleHarbor to ') 'Real install keeps completed summary'
    Check (Test-Path -LiteralPath (Join-Path $target '.idleharbor-managed.json') -PathType Leaf) 'Real install creates ownership marker'
    $before = Snapshot $target
    $output = @(& (Join-Path $packaging 'install.ps1') @parameters -WhatIf 3>&1 6>$null)
    Check-Preview $output 'Update'
    Check ((Snapshot $target) -ceq $before) 'Update preview preserves managed files and marker bytes'

    $output = @(& (Join-Path $packaging 'uninstall.ps1') -InstallRoot $target -WhatIf -Confirm:$false 3>&1 6>$null)
    Check-Preview $output 'Clean uninstall'
    Check ((Snapshot $target) -ceq $before) 'Clean uninstall preview preserves the complete target'
    Check ((Text $output) -notmatch 'unexpected files') 'Retained managed files are not reported as unexpected in preview'

    $foreign = Join-Path $target 'user-note.txt'
    Set-Content -LiteralPath $foreign -Value 'unowned data' -Encoding ASCII
    $before = Snapshot $target
    $output = @(& (Join-Path $packaging 'uninstall.ps1') -InstallRoot $target -WhatIf -Confirm:$false 3>&1 6>$null)
    Check-Preview $output 'Uninstall with foreign file'
    Check ((Snapshot $target) -ceq $before) 'Foreign-file preview preserves all bytes'
    $warnings = Text @($output | Where-Object { $_ -is [Management.Automation.WarningRecord] })
    Check ($warnings -match 'user-note.txt') 'Preview still warns about unowned files'
    Check ($warnings -notmatch 'IdleHarbor.exe|\.idleharbor-managed.json|README.md') 'Preview warning excludes simulated managed removals'

    $output = @(& (Join-Path $packaging 'uninstall.ps1') -InstallRoot $target -Confirm:$false 3>&1 6>$null)
    Check ((Text $output) -match '(?m)^Uninstalled IdleHarbor from ') 'Real uninstall keeps completed summary'
    Check ((Get-Content -Raw -LiteralPath $foreign).Trim() -ceq 'unowned data') 'Real uninstall preserves unowned data'
    Check (-not (Test-Path -LiteralPath (Join-Path $target 'IdleHarbor.exe'))) 'Real uninstall removes the owned executable'
    Check (-not (Test-Path -LiteralPath (Join-Path $target '.idleharbor-managed.json'))) 'Real uninstall removes its ownership marker'
    Check ((Snapshot $source) -ceq $sourceBefore) 'All operations preserve source bytes'
    if ($failures.Count -gt 0) { throw "$($failures.Count) preview assertions failed: $($failures -join '; ')" }
    Write-Host 'Packaging preview lifecycle checks passed (six operations).'
}
finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
