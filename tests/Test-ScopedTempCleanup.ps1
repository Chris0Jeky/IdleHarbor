[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'Remove-IdleHarborScopedTemp.ps1')

$checks = 0
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    $script:checks++
}

$tempPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'

$matching = Join-Path ([IO.Path]::GetTempPath()) ("IdleHarbor-help-tips-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $matching | Out-Null
Set-Content -LiteralPath (Join-Path $matching 'settings.ini') -Value "profile=balanced`r`n" -Encoding ascii
$removed = Remove-IdleHarborScopedTemp -Path $matching -LeafPattern 'IdleHarbor-help-tips-*' -RefusalNoun 'help-tip test directory'
Assert-True ($null -eq $removed) 'A matching temp directory must be removed without a refusal.'
Assert-True (-not (Test-Path -LiteralPath $matching)) 'A matching temp directory must be gone.'

$missing = Remove-IdleHarborScopedTemp -Path $matching -LeafPattern 'IdleHarbor-help-tips-*' -RefusalNoun 'help-tip test directory'
Assert-True ($null -eq $missing) 'A missing matching directory is already clean.'

$wrongLeaf = Join-Path ([IO.Path]::GetTempPath()) ("IdleHarbor-other-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $wrongLeaf | Out-Null
try {
    $wrongResolved = [IO.Path]::GetFullPath($wrongLeaf)
    $refusal = Remove-IdleHarborScopedTemp -Path $wrongLeaf -LeafPattern 'IdleHarbor-help-tips-*' -RefusalNoun 'help-tip test directory'
    Assert-True ($refusal -ceq "Refusing to remove an unexpected help-tip test directory: $wrongResolved") 'Wrong leaf must keep the existing refusal sentence.'
    Assert-True (Test-Path -LiteralPath $wrongLeaf) 'A wrong leaf must not be deleted.'
}
finally {
    Remove-Item -LiteralPath $wrongLeaf -Recurse -Force -ErrorAction SilentlyContinue
}

$outsideRoot = Join-Path $env:USERPROFILE ("IdleHarbor-scoped-temp-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outsideRoot | Out-Null
try {
    $outside = Join-Path $outsideRoot ("IdleHarbor-native-repaint-" + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $outside | Out-Null
    $outsideResolved = [IO.Path]::GetFullPath($outside)
    Assert-True (-not $outsideResolved.StartsWith($tempPrefix, [StringComparison]::OrdinalIgnoreCase)) 'The outside fixture must not sit in the user temp path.'
    $outsideRefusal = Remove-IdleHarborScopedTemp -Path $outside -LeafPattern 'IdleHarbor-native-repaint-*' -RefusalNoun 'native-test directory'
    Assert-True ($outsideRefusal -ceq "Refusing to remove an unexpected native-test directory: $outsideResolved") 'A path outside temp must be refused.'
    Assert-True (Test-Path -LiteralPath $outside) 'A path outside temp must remain.'
}
finally {
    Remove-Item -LiteralPath $outsideRoot -Recurse -Force -ErrorAction SilentlyContinue
}

$locked = Join-Path ([IO.Path]::GetTempPath()) ("IdleHarbor-native-repaint-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $locked | Out-Null
$lockedFile = Join-Path $locked 'settings.ini'
$stream = [IO.File]::Open($lockedFile, [IO.FileMode]::CreateNew, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    $lockedResult = Remove-IdleHarborScopedTemp -Path $locked -LeafPattern 'IdleHarbor-native-repaint-*' -RefusalNoun 'native-test directory'
    Assert-True ($null -ne $lockedResult -and $lockedResult.StartsWith('Could not remove native-test directory:', [StringComparison]::Ordinal)) 'A locked settings file must be reported, not thrown.'
    Assert-True (Test-Path -LiteralPath $locked) 'A locked temp directory must remain.'
}
finally {
    $stream.Dispose()
    Remove-Item -LiteralPath $locked -Recurse -Force -ErrorAction SilentlyContinue
}

$maskDir = Join-Path $env:USERPROFILE ("IdleHarbor-scoped-temp-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $maskDir | Out-Null
$caught = $null
$maskRefusal = $null
try {
    try {
        try { throw 'original failure' }
        finally {
            $maskRefusal = Remove-IdleHarborScopedTemp -Path $maskDir -LeafPattern 'IdleHarbor-help-tips-*' -RefusalNoun 'help-tip test directory'
        }
    }
    catch {
        $caught = $_.Exception.Message
    }
    Assert-True ($caught -ceq 'original failure') 'A refusal returned from finally must not replace the original failure.'
    Assert-True ($null -ne $maskRefusal -and $maskRefusal.StartsWith('Refusing to remove an unexpected help-tip test directory:', [StringComparison]::Ordinal)) 'The refusal is still available to the caller.'
    Assert-True (Test-Path -LiteralPath $maskDir) 'The refused directory must remain after the original failure.'
}
finally {
    Remove-Item -LiteralPath $maskDir -Recurse -Force -ErrorAction SilentlyContinue
}

foreach ($suite in @(
    @{ Name = 'Test-ControlHelpTips.ps1'; Pattern = 'IdleHarbor-help-tips-*'; Noun = 'help-tip test directory' },
    @{ Name = 'Test-NativeViewportRepaint.ps1'; Pattern = 'IdleHarbor-native-repaint-*'; Noun = 'native-test directory' }
)) {
    $path = Join-Path $PSScriptRoot $suite.Name
    $tokens = $null
    $parseErrors = $null
    [void][Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$parseErrors)
    Assert-True ($parseErrors.Count -eq 0) "$($suite.Name) must still parse."
    $text = Get-Content -LiteralPath $path -Raw
    Assert-True ($text.Contains("Import-Module (Join-Path `$PSScriptRoot 'IdleHarborTestHost.psm1')")) "$($suite.Name) must load the shared native host."
    Assert-True ($text.Contains("-LeafPattern '$($suite.Pattern)'")) "$($suite.Name) must keep its own temp leaf."
    Assert-True ($text.Contains("-RefusalNoun '$($suite.Noun)'")) "$($suite.Name) must keep its own refusal noun."
    Assert-True ($text -notmatch 'throw "Refusing to remove an unexpected') "$($suite.Name) must not throw the path refusal from finally."
}

Write-Host "Scoped temp cleanup passed ($checks checks)."
