[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$root = Join-Path ([IO.Path]::GetTempPath()) ('IdleHarbor-lag-test-' + [Guid]::NewGuid().ToString('N'))
$failures = New-Object 'Collections.Generic.List[string]'
$cases = @(
    @{ Name = 'current'; Project = '0.2.0'; History = @('0.2.0', '0.1.0'); Error = '' },
    @{ Name = 'previous-published'; Project = '0.3.0'; History = @('0.3.0', '0.2.0', '0.1.0'); Error = '' },
    @{ Name = 'next-unpublished'; Project = '0.3.0'; History = @('0.2.0', '0.1.0'); Error = '' },
    @{ Name = 'patch-unpublished'; Project = '0.2.1'; History = @('0.2.0', '0.1.0'); Error = '' },
    @{ Name = 'ahead'; Project = '0.1.0'; History = @('0.1.0'); Error = 'ahead' },
    @{ Name = 'two-published-behind'; Project = '0.4.0'; History = @('0.4.0', '0.3.0', '0.2.0'); Error = 'previous release' },
    @{ Name = 'stale-unpublished'; Project = '0.4.0'; History = @('0.3.0', '0.2.0'); Error = 'previous release' },
    @{ Name = 'no-history'; Project = '0.3.0'; History = @(); Error = 'dated release' },
    @{ Name = 'missing-history'; Project = '0.3.0'; History = $null; Error = 'CHANGELOG' },
    @{ Name = 'duplicate-history'; Project = '0.3.0'; History = @('0.3.0', '0.2.0', '0.2.0'); Error = 'descending' },
    @{ Name = 'unordered-history'; Project = '0.3.0'; History = @('0.3.0', '0.1.0', '0.2.0'); Error = 'descending' },
    @{ Name = 'future-release'; Project = '0.3.0'; History = @('0.4.0', '0.2.0'); Error = 'ahead' },
    @{ Name = 'invalid-date'; Project = '0.3.0'; History = @('0.3.0', '0.2.0'); Error = 'date' },
    @{ Name = 'empty-previous'; Project = '0.3.0'; History = @('0.3.0'); Error = 'previous release' }
)
try {
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'packaging') -Destination $root -Recurse
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'LICENSE') -Destination $root
    foreach ($case in $cases) {
        Set-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Value "project(IdleHarbor VERSION $($case.Project) LANGUAGES CXX)" -Encoding ASCII
        $changelog = Join-Path $root 'CHANGELOG.md'
        if (Test-Path -LiteralPath $changelog) { Remove-Item -LiteralPath $changelog }
        if ($null -ne $case.History) {
            $lines = @('# Changelog', '', '## [Unreleased]', '')
            foreach ($version in $case.History) { $lines += "## [$version] - 2026-08-24" }
            if ($case.Name -eq 'invalid-date') { $lines = @($lines | ForEach-Object { $_.Replace('2026-08-24', '2026-02-30') }) }
            Set-Content -LiteralPath $changelog -Value $lines -Encoding ASCII
        }
        $message = ''
        try { & (Join-Path $root 'packaging\Test-ChocolateyPackage.ps1') | Out-Null }
        catch { $message = $_.Exception.Message }
        $passed = ($case.Error.Length -eq 0 -and $message.Length -eq 0) -or
            ($case.Error.Length -gt 0 -and $message -match $case.Error)
        if (-not $passed) {
            $failures.Add("$($case.Name): expected '$($case.Error)', got '$message'.")
            Write-Host "FAIL: $($failures[$failures.Count - 1])"
        }
        else { Write-Host "PASS: Chocolatey lag / $($case.Name)" }
    }
    if ($failures.Count -gt 0) { throw "$($failures.Count) release-lag cases failed: $($failures -join '; ')" }
    Write-Host "Chocolatey release-lag checks passed ($($cases.Count) cases)."
}
finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
