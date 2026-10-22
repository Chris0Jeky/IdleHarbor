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

function Invoke-ConfirmationRace {
    $race.Fired = $true
    if ($race.Kind -eq 'leaf') {
        Remove-Item -LiteralPath $race.Link -Force
        [IO.File]::WriteAllBytes($race.Link, $race.Replacement)
    }
    else {
        Move-Item -LiteralPath $race.Programs -Destination $race.Backup
        New-Item -ItemType Junction -Path $race.Programs -Target $race.Backing | Out-Null
        $junctions.Add($race.Programs)
    }
}

function Add-ConfirmationRace([scriptblock]$Script) {
    # Test-only injection at the true return from the real ShouldProcess call.
    # There is no interactive prompt or production fault-injection switch.
    $confirmation = "if (`$null -ne `$link -and `$PSCmdlet.ShouldProcess(`$link, 'Remove installer-owned Start Menu shortcut')) {"
    if (-not $Script.ToString().Contains($confirmation)) { throw 'Missing marker removal confirmation seam.' }
    return [scriptblock]::Create($Script.ToString().Replace($confirmation, "$confirmation`n        Invoke-ConfirmationRace"))
}

try {
    foreach ($operation in @('update', 'update-none', 'uninstall')) {
      foreach ($kind in @('leaf', 'junction')) {
        $caseRoot = Join-Path $root "confirmation-$operation-$kind"
        $source = Join-Path $caseRoot 'source'
        $target = Join-Path $caseRoot 'IdleHarbor'
        $programs = Join-Path $caseRoot 'old Programs'
        New-Item -ItemType Directory -Path $source -Force | Out-Null
        [IO.File]::WriteAllBytes((Join-Path $source 'IdleHarbor.exe'), [byte[]](0x4d, 0x5a, 0, 1))
        $parameters = @{ SourcePath = $source; InstallRoot = $target; Startup = 'None'; NoLaunch = $true; Confirm = $false }
        & (Get-IsolatedScript 'install') @parameters | Out-Null
        $oldLink = Join-Path $programs 'IdleHarbor.lnk'
        $replacement = Join-Path $caseRoot 'replacement\IdleHarbor.lnk'
        New-Item -ItemType Directory -Path (Split-Path -Parent $replacement) -Force | Out-Null
        Copy-Item -LiteralPath $oldLink -Destination $replacement
        $shell = New-Object -ComObject WScript.Shell
        $shortcut = $shell.CreateShortcut($replacement); $shortcut.Arguments = '--user-created'; $shortcut.Save()
        $race = @{
            Kind = $kind; Fired = $false; Link = $oldLink; Programs = $programs
            Backup = (Join-Path $caseRoot 'original Programs'); Backing = (Split-Path -Parent $replacement)
            Replacement = [IO.File]::ReadAllBytes($replacement)
        }
        $replacementHash = (Get-FileHash -LiteralPath $replacement -Algorithm SHA256).Hash
        $programs = Join-Path $caseRoot 'new Programs'
        if ($operation -eq 'update-none') { $parameters.StartMenu = 'None' }
        $records = $(if ($operation -like 'update*') {
            @(& (Add-ConfirmationRace (Get-IsolatedScript 'install')) @parameters 3>&1)
        } else {
            @(& (Add-ConfirmationRace (Get-IsolatedScript 'uninstall')) -InstallRoot $target -Confirm:$false 3>&1)
        })
        Check $race.Fired "$operation/$kind swaps only after removal is approved"
        $survives = Test-Path -LiteralPath $oldLink -PathType Leaf
        Check $survives "$operation/$kind preserves replacement after confirmation"
        Check ($survives -and (Get-FileHash -LiteralPath $oldLink -Algorithm SHA256).Hash -eq $replacementHash) `
            "$operation/$kind preserves replacement bytes after confirmation"
        Check (@($records | Where-Object { $_ -is [Management.Automation.WarningRecord] -and
            $_.Message -like '*Preserving*Start Menu shortcut*' }).Count -gt 0) "$operation/$kind warns about preserved leaf"
      }
    }
    # Exercise the handle handoff separately: identical bytes cannot authorize
    # deleting a different file object. The validation lock also denies mutation.
    $identityRoot = Join-Path $root 'deletion-identity'
    New-Item -ItemType Directory -Path $identityRoot -Force | Out-Null
    $original = Join-Path $identityRoot 'original.lnk'
    $replacement = Join-Path $identityRoot 'replacement.lnk'
    [IO.File]::WriteAllBytes($original, $race.Replacement)
    Copy-Item -LiteralPath $original -Destination $replacement
    $validated = [IO.File]::Open($original, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        $writeDenied = $false
        try { $writer = [IO.File]::OpenWrite($original); $writer.Dispose() } catch { $writeDenied = $true }
        Check $writeDenied 'validation handle denies concurrent writes'
        $renameDenied = $false
        try { [IO.File]::Move($original, (Join-Path $identityRoot 'moved.lnk')) } catch { $renameDenied = $true }
        Check $renameDenied 'validation handle denies concurrent leaf replacement'
        $identityDenied = $false
        try { [IdleHarbor.Packaging.FileIdentity]::DeleteValidated($replacement, $validated) }
        catch { $identityDenied = $_.Exception.Message -like '*identity*changed*' }
        Check $identityDenied 'deletion rejects a different file ID even with identical shortcut bytes'
        Check ((Test-Path -LiteralPath $original) -and (Test-Path -LiteralPath $replacement)) `
            'identity mismatch preserves both file objects'
    }
    finally { $validated.Dispose() }

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
