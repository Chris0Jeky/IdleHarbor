[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$packager = Join-Path (Split-Path -Parent $PSScriptRoot) 'packaging\New-ReleasePackage.ps1'
$root = Join-Path ([IO.Path]::GetTempPath()) ('IdleHarbor-package-regression-' + [Guid]::NewGuid().ToString('N'))
$payload = [ordered]@{
    'packaging\install.ps1' = 'install.ps1'
    'packaging\uninstall.ps1' = 'uninstall.ps1'
    'packaging\install.cmd' = 'install.cmd'
    'README.md' = 'README.md'
    'LICENSE' = 'LICENSE'
    'THIRD-PARTY-NOTICES.md' = 'THIRD-PARTY-NOTICES.md'
    'packaging\README.md' = 'DISTRIBUTION.md'
}

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function New-Fixture([string]$Name, [string]$Missing = '') {
    $caseRoot = Join-Path $root $Name
    $repo = Join-Path $caseRoot 'repo with spaces'
    $build = Join-Path $caseRoot 'build'
    $output = Join-Path $caseRoot 'dist'
    New-Item -ItemType Directory -Path (Join-Path $repo 'packaging'), $build, $output -Force | Out-Null
    foreach ($source in $payload.Keys) {
        if ($source -cne $Missing) {
            Set-Content -LiteralPath (Join-Path $repo $source) -Value "fixture: $source" -Encoding ASCII
        }
    }
    [IO.File]::WriteAllBytes((Join-Path $build 'IdleHarbor.exe'), [byte[]](0x4d, 0x5a, 0, 1))
    return @{ RepositoryRoot = $repo; BuildDirectory = $build; OutputDirectory = $output; Version = '1.2.3'; Architecture = 'x64' }
}

try {
    $caseNumber = 0
    foreach ($missing in $payload.Keys) {
        $caseNumber++
        $parameters = New-Fixture "missing-$caseNumber" $missing
        $archive = Join-Path $parameters.OutputDirectory 'IdleHarbor-1.2.3-windows-x64-portable.zip'
        [IO.File]::WriteAllText($archive, 'existing archive must survive failed preflight')
        $before = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
        $message = ''
        try { & $packager @parameters | Out-Null }
        catch { $message = $_.Exception.Message }
        Assert-True ($message -like "Expected required release file '*") "Missing $missing did not fail preflight: $message"
        Assert-True ($message.Contains("'$missing'")) "Wrong missing-file diagnostic for ${missing}: $message"
        Assert-True ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ceq $before) "Failed preflight replaced the existing archive for $missing."
        Assert-True (@(Get-ChildItem -LiteralPath $parameters.OutputDirectory).Count -eq 1) 'Failed preflight produced extra output.'
        Write-Host "PASS: missing $missing is rejected without replacing an archive."
    }

    $parameters = New-Fixture 'complete'
    $archive = & $packager @parameters
    Assert-True (Test-Path -LiteralPath $archive -PathType Leaf) 'Complete fixture did not produce an archive.'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    try {
        $prefix = 'IdleHarbor-1.2.3-windows-x64-portable/'
        $files = @($zip.Entries | Where-Object { $_.Name.Length -gt 0 })
        $names = @($files | ForEach-Object { $_.FullName.Replace('\', '/') })
        Assert-True ($files.Count -eq ($payload.Count + 2)) 'Archive contains an unexpected payload.'
        foreach ($destination in $payload.Values) {
            Assert-True ($names -ccontains ($prefix + $destination)) "Archive omits $destination."
        }
        Assert-True ($names -ccontains ($prefix + 'IdleHarbor.exe')) 'Archive omits the executable.'
        $entries = @($files | Where-Object { $_.FullName.Replace('\', '/') -ceq ($prefix + 'package-manifest.json') })
        Assert-True ($entries.Count -eq 1) 'Archive must contain one manifest.'
        $reader = New-Object IO.StreamReader($entries[0].Open())
        try { $manifest = $reader.ReadToEnd() | ConvertFrom-Json }
        finally { $reader.Dispose() }
        Assert-True ($manifest.licenseFile -ceq 'LICENSE') 'Manifest does not declare LICENSE.'
        Assert-True ($manifest.version -ceq '1.2.3') 'Manifest version is incorrect.'
        Assert-True ($manifest.architecture -ceq 'x64') 'Manifest architecture is incorrect.'
    }
    finally { $zip.Dispose() }
    Write-Host 'PASS: complete archive contains all seven required files, executable and manifest.'
    Write-Host 'Release package regression checks passed (8 cases).'
}
finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
