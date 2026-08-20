[CmdletBinding()]
param(
    [string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Get-RunBlockBodies([string[]]$Lines) {
    $bodies = New-Object System.Collections.Generic.List[string]
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        $opening = [regex]::Match($Lines[$i], '^(?<indent>\s*)run:\s*[|>][-+]?\s*$')
        if (-not $opening.Success) { continue }

        $runIndent = $opening.Groups['indent'].Value.Length
        $body = New-Object System.Collections.Generic.List[string]
        $j = $i + 1
        for (; $j -lt $Lines.Count; $j++) {
            $line = $Lines[$j]
            if ([string]::IsNullOrWhiteSpace($line)) {
                $body.Add($line)
                continue
            }
            $lineIndent = $line.Length - $line.TrimStart().Length
            if ($lineIndent -le $runIndent) { break }
            $body.Add($line)
        }
        $bodies.Add(($body -join [Environment]::NewLine))
        $i = $j - 1
    }
    return @($bodies)
}

function Assert-StepReleaseTagEnvironment([string[]]$Lines, [string]$StepName) {
    $headerPattern = '^\s*-\s+name:\s*' + [regex]::Escape($StepName) + '\s*$'
    $start = -1
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i] -match $headerPattern) {
            $start = $i
            break
        }
    }
    Assert-True ($start -ge 0) "Release workflow step '$StepName' was not found."

    $end = $Lines.Count
    for ($i = $start + 1; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i] -match '^\s*-\s+name:') {
            $end = $i
            break
        }
    }
    $stepLines = @($Lines[$start..($end - 1)])
    Assert-True (@($stepLines | Where-Object { $_ -match '^\s+env:\s*$' }).Count -eq 1) `
        "Release workflow step '$StepName' must declare one step-level env block."
    Assert-True (@($stepLines | Where-Object {
            $_ -match '^\s+RELEASE_TAG:\s*\$\{\{\s*github\.ref_name\s*\}\}\s*$'
        }).Count -eq 1) `
        "Release workflow step '$StepName' must pass github.ref_name as RELEASE_TAG."
}

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $scriptPath = $MyInvocation.MyCommand.Path
    $RepositoryRoot = Split-Path -Parent (Split-Path -Parent $scriptPath)
}
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$workflowPath = Join-Path $root '.github\workflows\release.yml'
$lines = @(Get-Content -LiteralPath $workflowPath)
$workflow = Get-Content -Raw -LiteralPath $workflowPath
$runBodies = @(Get-RunBlockBodies $lines)

Assert-StepReleaseTagEnvironment $lines 'Resolve version'
Assert-StepReleaseTagEnvironment $lines 'Publish release'
$releaseTagEnvLines = @($lines | Where-Object {
        $_ -match '^\s+RELEASE_TAG:\s*\$\{\{\s*github\.ref_name\s*\}\}\s*$'
    })
Assert-True ($releaseTagEnvLines.Count -eq 2) `
    'Release workflow must pass github.ref_name through exactly two RELEASE_TAG step environments.'

$resolveBodies = @($runBodies | Where-Object { $_ -match 'Test-ReleaseVersion\.ps1' })
Assert-True ($resolveBodies.Count -eq 1) 'Release workflow must have one version-resolution run block.'
Assert-True ($resolveBodies[0] -match '\$version\s*=\s*\$env:RELEASE_TAG\s*-replace\s+''\^v''') `
    'Resolve version must derive its version from $env:RELEASE_TAG.'
Assert-True ($resolveBodies[0] -match 'Test-ReleaseVersion\.ps1\s+-Tag\s+\$env:RELEASE_TAG') `
    'Resolve version must validate $env:RELEASE_TAG.'
Assert-True ($resolveBodies[0] -match '\$version\s+-notmatch\s+''\^\\d\+\\.\\d\+\\.\\d\+\$''') `
    'Resolve version must retain stable-only semantic tag validation.'

$publishBodies = @($runBodies | Where-Object { $_ -match 'gh\s+release\s+create' })
Assert-True ($publishBodies.Count -eq 1) 'Release workflow must have one GitHub release publish run block.'
Assert-True ($publishBodies[0] -match 'gh\s+release\s+create\s+\$env:RELEASE_TAG\s+--verify-tag') `
    'Publish release must pass $env:RELEASE_TAG to gh release create.'

$directRefInterpolation = @($runBodies | Where-Object { $_ -match '\$\{\{\s*github\.ref_name\s*\}\}' })
Assert-True ($directRefInterpolation.Count -eq 0) `
    'Release run-script source must not interpolate github.ref_name directly.'
$singleLineRunInterpolation = @($lines | Where-Object {
        $_ -match '^\s*run:\s*.*\$\{\{\s*github\.ref_name\s*\}\}'
    })
Assert-True ($singleLineRunInterpolation.Count -eq 0) `
    'Release single-line run commands must not interpolate github.ref_name directly.'

Assert-True ($workflow -match 'Test-ReleaseLicense\.ps1\s+-RepositoryRoot\s+\.') `
    'Release workflow must retain its tracked-LICENSE publication gate.'

Write-Output 'Release workflow contract passed.'
