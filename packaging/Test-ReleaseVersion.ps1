[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Tag,

    [string]$SourceRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Windows PowerShell 5.1 evaluates parameter defaults before populating
# $PSScriptRoot. Resolve the script-relative default after parameter binding so
# direct `-File` invocation behaves the same in both supported editions.
if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $SourceRoot = Split-Path -Parent $PSScriptRoot
}

$tagMatch = [regex]::Match($Tag, '^v(?<version>\d+\.\d+\.\d+)$')
if (-not $tagMatch.Success) {
    throw "Release tag is not a supported stable SemVer tag: $Tag"
}
$tagVersion = $tagMatch.Groups['version'].Value

function Get-RequiredMatch([string]$Text, [string]$Pattern, [string]$Description) {
    $match = [regex]::Match($Text, $Pattern)
    if (-not $match.Success) {
        throw "Could not determine $Description."
    }
    return $match.Groups[1].Value
}

$cmake = Get-Content -Raw -LiteralPath (Join-Path $SourceRoot 'CMakeLists.txt')
$versionHeader = Get-Content -Raw -LiteralPath (Join-Path $SourceRoot 'include/idleharbor/version.hpp')
$resource = Get-Content -Raw -LiteralPath (Join-Path $SourceRoot 'resources/IdleHarbor.rc')
$manifest = Get-Content -Raw -LiteralPath (Join-Path $SourceRoot 'resources/app.manifest')

$projectVersion = Get-RequiredMatch $cmake '(?is)project\s*\(\s*IdleHarbor\s+VERSION\s+([0-9A-Za-z.+-]+)' 'CMake project version'
$headerVersion = Get-RequiredMatch $versionHeader 'kVersion\s*=\s*L"([^"]+)"' 'include/idleharbor/version.hpp version'
$fileVersion = Get-RequiredMatch $resource 'VALUE\s+"FileVersion"\s*,\s*"([^"\\]+)(?:\\0)?"' 'resource FileVersion'
$productVersion = Get-RequiredMatch $resource 'VALUE\s+"ProductVersion"\s*,\s*"([^"\\]+)(?:\\0)?"' 'resource ProductVersion'
$numericFileVersion = Get-RequiredMatch $resource 'FILEVERSION\s+([0-9]+,[0-9]+,[0-9]+,[0-9]+)' 'resource numeric FILEVERSION'
$numericProductVersion = Get-RequiredMatch $resource 'PRODUCTVERSION\s+([0-9]+,[0-9]+,[0-9]+,[0-9]+)' 'resource numeric PRODUCTVERSION'
$manifestVersion = Get-RequiredMatch $manifest '(?is)<assemblyIdentity\b[^>]*\bversion="([0-9]+(?:\.[0-9]+){3})"' 'resources/app.manifest version'

foreach ($source in @{
    'CMake project version' = $projectVersion
    'include/idleharbor/version.hpp kVersion' = $headerVersion
    'resource FileVersion' = $fileVersion
    'resource ProductVersion' = $productVersion
}.GetEnumerator()) {
    if ($source.Value -cne $tagVersion) {
        throw "$($source.Key) '$($source.Value)' does not exactly match tag version '$tagVersion'."
    }
}

$versionParts = $tagVersion -split '\.'
$numericExpected = "$($versionParts[0]),$($versionParts[1]),$($versionParts[2]),0"
$manifestExpected = "$($versionParts[0]).$($versionParts[1]).$($versionParts[2]).0"
foreach ($source in @{
    'resource numeric FILEVERSION' = $numericFileVersion
    'resource numeric PRODUCTVERSION' = $numericProductVersion
}.GetEnumerator()) {
    if ($source.Value -cne $numericExpected) {
        throw "$($source.Key) '$($source.Value)' does not exactly match '$numericExpected'."
    }
}
if ($manifestVersion -cne $manifestExpected) {
    throw "resources/app.manifest version '$manifestVersion' does not exactly match '$manifestExpected'."
}

Write-Output "Release version validated: $Tag -> $tagVersion"
