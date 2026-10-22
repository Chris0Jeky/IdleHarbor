[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$root = Join-Path ([IO.Path]::GetTempPath()) ('IdleHarbor-relocation-test-' + [Guid]::NewGuid().ToString('N'))
$failures = New-Object 'Collections.Generic.List[string]'
$junctions = New-Object 'Collections.Generic.List[string]'
. (Join-Path $PSScriptRoot 'Get-IsolatedPackagingScript.ps1')

function Check([bool]$Condition, [string]$Message) {
    if ($Condition) { Write-Host "PASS: $Message" }
    else { $failures.Add($Message); Write-Host "FAIL: $Message" }
}

function Get-IsolatedScript([string]$Name) {
    return Get-IsolatedPackagingScript (Join-Path $repository "packaging\$Name.ps1") $caseRoot $programs
}
function Get-ScheduledTask {
    [CmdletBinding()]
    param([string]$TaskPath, [string]$TaskName)
    return
}
function Start-Process { throw 'Relocation tests must never launch an application.' }
function Register-ScheduledTask { throw 'Relocation tests must never register a task.' }
function Unregister-ScheduledTask { throw 'Relocation tests must never remove a task.' }

try {
    foreach ($operation in @('update', 'update-none', 'uninstall')) {
      foreach ($scenario in @('intact', 'bytes-modified', 'bytes-current', 'foreign', 'outside-boundary', 'sibling-boundary',
          'bad-hash', 'partial-evidence', 'legacy-relocated', 'legacy-current', 'unowned', 'hardlink', 'junction', 'directory')) {
        $caseRoot = Join-Path $root "$operation-$scenario"
        $source = Join-Path $caseRoot 'source'
        $target = Join-Path $caseRoot 'IdleHarbor'
        $programs = Join-Path $caseRoot 'old Programs'
        New-Item -ItemType Directory -Path $source -Force | Out-Null
        [IO.File]::WriteAllBytes((Join-Path $source 'IdleHarbor.exe'), [byte[]](0x4d, 0x5a, 0, 1))
        $parameters = @{ SourcePath = $source; InstallRoot = $target; Startup = 'None'; NoLaunch = $true; Confirm = $false }
        $install = Get-IsolatedScript 'install'
        & $install @parameters | Out-Null
        $oldLink = Join-Path $programs 'IdleHarbor.lnk'
        $markerPath = Join-Path $target '.idleharbor-managed.json'
        $marker = Get-Content -Raw -LiteralPath $markerPath | ConvertFrom-Json
        Check (($marker.startMenuProgramsPath -eq $programs) -and
            ($marker.startMenuLinkSha256 -eq (Get-FileHash -LiteralPath $oldLink -Algorithm SHA256).Hash)) "$operation/$scenario records install-time boundary and actual shortcut hash"
        switch ($scenario) {
            { $_ -in @('bytes-modified', 'bytes-current') } {
                $shell = New-Object -ComObject WScript.Shell
                $link = $shell.CreateShortcut($oldLink); $link.Hotkey = 'CTRL+ALT+I'; $link.Save()
            }
            'foreign' {
                $shell = New-Object -ComObject WScript.Shell
                $link = $shell.CreateShortcut($oldLink); $link.Arguments = '--foreign'; $link.Save()
                # Even a matching digest must not bypass semantic identity.
                $marker.startMenuLinkSha256 = (Get-FileHash -LiteralPath $oldLink -Algorithm SHA256).Hash
            }
            'outside-boundary' { $marker.startMenuProgramsPath = Join-Path $caseRoot 'elsewhere' }
            'sibling-boundary' { $marker.startMenuProgramsPath = $programs.Substring(0, $programs.Length - 1) }
            'bad-hash' { $marker.startMenuLinkSha256 = 'not-a-sha256' }
            'partial-evidence' { $marker.PSObject.Properties.Remove('startMenuLinkSha256') }
            { $_ -like 'legacy-*' } {
                $marker.PSObject.Properties.Remove('startMenuProgramsPath')
                $marker.PSObject.Properties.Remove('startMenuLinkSha256')
            }
            'unowned' { $marker.startMenuLinkOwned = $false }
            'hardlink' { New-Item -ItemType HardLink -Path (Join-Path $caseRoot 'witness.lnk') -Target $oldLink | Out-Null }
            'junction' {
                $backing = Join-Path $caseRoot 'backing'
                Move-Item -LiteralPath $programs -Destination $backing
                New-Item -ItemType Junction -Path $programs -Target $backing | Out-Null
                $junctions.Add($programs)
            }
            'directory' {
                Remove-Item -LiteralPath $oldLink -Force
                New-Item -ItemType Directory -Path $oldLink | Out-Null
            }
        }
        $marker | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $markerPath -Encoding UTF8
        $before = $(if ($scenario -ne 'directory') { (Get-FileHash -LiteralPath $oldLink -Algorithm SHA256).Hash } else { 'directory' })
        if ($scenario -notin @('legacy-current', 'bytes-current')) { $programs = Join-Path $caseRoot 'new Programs' }
        $install = Get-IsolatedScript 'install'
        $uninstall = Get-IsolatedScript 'uninstall'
        if ($operation -eq 'update-none') { $parameters.StartMenu = 'None' }
        $records = $(if ($operation -like 'update*') { @(& $install @parameters 3>&1) }
            else { @(& $uninstall -InstallRoot $target -Confirm:$false 3>&1) })
        $shouldRemove = ($scenario -eq 'intact' -or ($scenario -eq 'legacy-current' -and $operation -ne 'update'))
        if ($shouldRemove) {
            Check (-not (Test-Path -LiteralPath $oldLink)) "$operation/$scenario removes owned shortcut"
        }
        else {
            Check (Test-Path -LiteralPath $oldLink) "$operation/$scenario preserves former shortcut"
            if ($scenario -ne 'directory') {
                Check ((Get-FileHash -LiteralPath $oldLink -Algorithm SHA256).Hash -eq $before) "$operation/$scenario preserves every byte"
            }
            if ($scenario -notin @('legacy-current', 'unowned')) {
                Check (@($records | Where-Object { $_ -is [Management.Automation.WarningRecord] }).Count -gt 0) "$operation/$scenario reports preservation"
            }
        }
        if ($operation -eq 'update') {
            Check (Test-Path -LiteralPath (Join-Path $programs 'IdleHarbor.lnk')) "$operation/$scenario has current Programs shortcut"
            $updatedMarker = Get-Content -Raw -LiteralPath $markerPath | ConvertFrom-Json
            Check ($updatedMarker.startMenuLinkOwned -eq ($scenario -ne 'bytes-current')) "$operation/$scenario records current launcher ownership"
        }
      }
    }
    foreach ($fault in @('AfterCopy', 'AfterStartupRemoval', 'AfterStartup', 'AfterMarker', 'Preview')) {
        $caseRoot = Join-Path $root "transaction-$fault"
        $source = Join-Path $caseRoot 'source'
        $target = Join-Path $caseRoot 'IdleHarbor'
        $programs = Join-Path $caseRoot 'old Programs'
        New-Item -ItemType Directory -Path $source -Force | Out-Null
        [IO.File]::WriteAllBytes((Join-Path $source 'IdleHarbor.exe'), [byte[]](0x4d, 0x5a, 0, 1))
        $parameters = @{ SourcePath = $source; InstallRoot = $target; Startup = 'None'; NoLaunch = $true; Confirm = $false }
        $install = Get-IsolatedScript 'install'
        & $install @parameters | Out-Null
        $oldLink = Join-Path $programs 'IdleHarbor.lnk'
        $oldHash = (Get-FileHash -LiteralPath $oldLink -Algorithm SHA256).Hash
        $markerPath = Join-Path $target '.idleharbor-managed.json'
        $markerHash = (Get-FileHash -LiteralPath $markerPath -Algorithm SHA256).Hash
        $programs = Join-Path $caseRoot 'new Programs'
        $install = Get-IsolatedScript 'install'
        if ($fault -eq 'Preview') {
            & $install @parameters -WhatIf 6>$null | Out-Null
            $uninstall = Get-IsolatedScript 'uninstall'
            & $uninstall -InstallRoot $target -WhatIf -Confirm:$false 6>$null | Out-Null
        }
        else {
            $message = ''
            try { & $install @parameters -InjectFailureAt $fault | Out-Null }
            catch { $message = $_.Exception.Message }
            Check ($message -like '*Injected installer failure*') "$fault reaches intended rollback fault"
        }
        Check ((Get-FileHash -LiteralPath $oldLink -Algorithm SHA256).Hash -eq $oldHash) "$fault preserves former shortcut bytes"
        Check ((Get-FileHash -LiteralPath $markerPath -Algorithm SHA256).Hash -eq $markerHash) "$fault preserves marker bytes"
        Check (-not (Test-Path -LiteralPath (Join-Path $programs 'IdleHarbor.lnk'))) "$fault leaves no current shortcut"
    }
    if ($failures.Count) { throw "$($failures.Count) relocation assertions failed: $($failures -join '; ')" }
    Write-Host 'Start Menu relocation lifecycle checks passed.'
}
finally {
    foreach ($junction in $junctions) {
        # Remove the junction itself before recursive fixture cleanup on PS 5.1.
        [IO.Directory]::Delete($junction)
    }
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
