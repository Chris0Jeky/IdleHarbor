[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$validator = Join-Path (Split-Path -Parent $PSScriptRoot) 'packaging\Test-ReleaseVersion.ps1'
$root = Join-Path ([IO.Path]::GetTempPath()) ('IdleHarbor-version-regression-' + [Guid]::NewGuid().ToString('N'))
$fixture = [ordered]@{
    'CMakeLists.txt' = 'project(IdleHarbor VERSION 1.2.3 LANGUAGES CXX)'
    'include\idleharbor\version.hpp' = 'inline constexpr std::wstring_view kVersion = L"1.2.3";'
    'resources\IdleHarbor.rc' = @'
FILEVERSION 1,2,3,0
PRODUCTVERSION 1,2,3,0
VALUE "FileVersion", "1.2.3\0"
VALUE "ProductVersion", "1.2.3\0"
'@
    'resources\app.manifest' = '<assemblyIdentity name="Chris0Jeky.IdleHarbor" version="1.2.3.0" />'
}

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Reset-Fixture {
    foreach ($path in $fixture.Keys) {
        Set-Content -LiteralPath (Join-Path $root $path) -Value $fixture[$path] -Encoding ASCII
    }
}

function Assert-Rejected([string]$Tag, [string]$Expected) {
    $message = ''
    try { & $validator -Tag $Tag -SourceRoot $root | Out-Null }
    catch { $message = $_.Exception.Message }
    Assert-True ($message -match $Expected) "Expected rejection matching '$Expected' for '$Tag', got '$message'."
}

try {
    New-Item -ItemType Directory -Path (Join-Path $root 'include\idleharbor'), (Join-Path $root 'resources') -Force | Out-Null
    Reset-Fixture
    foreach ($tag in @('v1.2.3-rc.1', '1.2.3')) {
        Assert-Rejected $tag 'not a supported stable SemVer tag'
        Write-Host "PASS: reject unsupported tag $tag."
    }
    Assert-Rejected 'v9.9.9' 'does not exactly match'
    Write-Host 'PASS: reject tag/tree disagreement.'

    $driftCases = @(
        @{ Path = 'CMakeLists.txt'; Old = '1.2.3'; New = '1.2.4'; Label = 'CMake project version' },
        @{ Path = 'include\idleharbor\version.hpp'; Old = '1.2.3'; New = '1.2.4'; Label = 'version.hpp' },
        @{ Path = 'resources\IdleHarbor.rc'; Old = 'FILEVERSION 1,2,3,0'; New = 'FILEVERSION 1,2,4,0'; Label = 'numeric FILEVERSION' },
        @{ Path = 'resources\IdleHarbor.rc'; Old = 'PRODUCTVERSION 1,2,3,0'; New = 'PRODUCTVERSION 1,2,4,0'; Label = 'numeric PRODUCTVERSION' },
        @{ Path = 'resources\IdleHarbor.rc'; Old = '"FileVersion", "1.2.3'; New = '"FileVersion", "1.2.4'; Label = 'resource FileVersion' },
        @{ Path = 'resources\IdleHarbor.rc'; Old = '"ProductVersion", "1.2.3'; New = '"ProductVersion", "1.2.4'; Label = 'resource ProductVersion' },
        @{ Path = 'resources\app.manifest'; Old = '1.2.3.0'; New = '1.2.4.0'; Label = 'app.manifest' }
    )
    foreach ($case in $driftCases) {
        Reset-Fixture
        $changed = $fixture[$case.Path].Replace($case.Old, $case.New)
        Assert-True ($changed -cne $fixture[$case.Path]) 'Invalid drift fixture: replacement changed nothing.'
        Set-Content -LiteralPath (Join-Path $root $case.Path) -Value $changed -Encoding ASCII
        Assert-Rejected 'v1.2.3' ([regex]::Escape($case.Label) + '.*does not exactly match')
        Write-Host "PASS: reject independent drift in $($case.Label)."
    }
    Reset-Fixture
    $output = & $validator -Tag 'v1.2.3' -SourceRoot $root
    Assert-True ($output -ceq 'Release version validated: v1.2.3 -> 1.2.3') 'Matching versions were not accepted.'
    Write-Host 'PASS: accept all matching version sources.'
    Write-Host 'Release version regression checks passed (11 cases).'
}
finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
