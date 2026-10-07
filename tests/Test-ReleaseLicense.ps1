[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$validator = Join-Path $repositoryRoot 'packaging\Test-ReleaseLicense.ps1'
$license = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'LICENSE')).Replace("`r`n", "`n")
$utf8 = New-Object Text.UTF8Encoding($false)
$utf8Bom = New-Object Text.UTF8Encoding($true)
$root = Join-Path ([IO.Path]::GetTempPath()) ('IdleHarbor-license-test-' + [Guid]::NewGuid().ToString('N'))
$failures = New-Object 'Collections.Generic.List[string]'

$withoutSection = [regex]::Replace($license, '(?ms)^  17\..*?(?=^\s*END OF TERMS AND CONDITIONS)', '')
if ($withoutSection -ceq $license) { throw 'Section-deletion fixture did not change the license.' }
$edited = $license.Replace('freedom', 'restriction')
if ($edited -ceq $license) { throw 'Interior-edit fixture did not change the license.' }
$preamble = $license.IndexOf('Preamble', [StringComparison]::Ordinal)
if ($preamble -lt 0) { throw 'Cannot find preamble for the truncated fixture.' }
$cases = @(
    @{ Name = 'complete-lf'; Text = $license; Bom = $false; Tracked = $true; Error = '' },
    @{ Name = 'complete-crlf'; Text = $license.Replace("`n", "`r`n"); Bom = $false; Tracked = $true; Error = '' },
    @{ Name = 'complete-utf8-bom'; Text = $license; Bom = $true; Tracked = $true; Error = '' },
    @{ Name = 'header-only'; Text = $license.Substring(0, $preamble); Bom = $false; Tracked = $true; Error = 'complete' },
    @{ Name = 'missing-section'; Text = $withoutSection; Bom = $false; Tracked = $true; Error = 'complete' },
    @{ Name = 'edited-clause'; Text = $edited; Bom = $false; Tracked = $true; Error = 'complete' },
    @{ Name = 'appended-text'; Text = ($license + 'Additional restriction.'); Bom = $false; Tracked = $true; Error = 'complete' },
    @{ Name = 'untracked'; Text = $license; Bom = $false; Tracked = $false; Error = 'tracked|pathspec' },
    @{ Name = 'missing'; Text = $null; Bom = $false; Tracked = $false; Error = 'none was found' }
)

try {
    foreach ($case in $cases) {
        $fixture = Join-Path $root $case.Name
        New-Item -ItemType Directory -Path $fixture -Force | Out-Null
        & git -C $fixture init --quiet
        if ($LASTEXITCODE -ne 0) { throw 'Could not initialize fixture repository.' }
        $path = Join-Path $fixture 'LICENSE'
        if ($null -ne $case.Text) {
            $encoding = $utf8
            if ($case.Bom) { $encoding = $utf8Bom }
            [IO.File]::WriteAllText($path, $case.Text, $encoding)
        }
        if ($case.Tracked) {
            & git -C $fixture -c core.autocrlf=false -c core.safecrlf=false add -- LICENSE
            if ($LASTEXITCODE -ne 0) { throw 'Could not stage fixture license.' }
        }
        $message = ''
        try { & $validator -RepositoryRoot $fixture | Out-Null }
        catch { $message = $_.Exception.Message }
        if (($case.Error.Length -eq 0 -and $message.Length -ne 0) -or
            ($case.Error.Length -ne 0 -and $message -notmatch $case.Error)) {
            $failures.Add("$($case.Name): expected '$($case.Error)', got '$message'.")
            Write-Host "FAIL: $($failures[$failures.Count - 1])"
        }
        else { Write-Host "PASS: license / $($case.Name)" }
    }
    if ($failures.Count -ne 0) { throw "$($failures.Count) license cases failed: $($failures -join '; ')" }
    # The negative fixtures may leave a native nonzero code on Windows PowerShell.
    # Restore a successful native result so a passing suite does not poison CI.
    & git --version | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Git became unavailable during license tests.' }
    Write-Host "Release license regression checks passed ($($cases.Count) cases)."
}
finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
