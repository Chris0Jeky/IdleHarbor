[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $repositoryRoot 'tools\Capture-IdleHarborScreenshots.ps1'), [ref]$tokens, [ref]$errors)
Assert-True ($errors.Count -eq 0) 'Capture script must parse before extracting its formatter.'
$definition = $ast.Find({ param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Format-CaptureManifestJson'
}, $true)
Assert-True ($null -ne $definition) 'Missing production formatter.'
. ([scriptblock]::Create($definition.Extent.Text))

Assert-True ((Format-CaptureManifestJson $null) -ceq 'null') 'Null scalar is not canonical.'
Assert-True ((Format-CaptureManifestJson $false) -ceq 'false') 'False scalar is not canonical.'
Assert-True ((Format-CaptureManifestJson '') -ceq '""') 'Empty string is not canonical.'
Assert-True ((Format-CaptureManifestJson ([ordered]@{})) -ceq '{}') 'Empty dictionary is not canonical.'
Assert-True ((Format-CaptureManifestJson ([pscustomobject]@{})) -ceq '{}') 'Empty object is not canonical.'
Assert-True ((Format-CaptureManifestJson @()) -ceq '[]') 'Empty array is not canonical.'
Assert-True ((Format-CaptureManifestJson ([long]::MaxValue)) -ceq '9223372036854775807') 'Large integer loses precision.'
$control = [string][char]0 + "`t`r`n"
Assert-True ((Format-CaptureManifestJson $control) -ceq '"\u0000\u0009\u000d\u000a"') 'Control characters are not JSON escaped.'
$unicode = 'caf' + [char]0x00e9
Assert-True (((Format-CaptureManifestJson $unicode) | ConvertFrom-Json) -ceq $unicode) 'Unicode string does not round-trip.'
$oldCulture = [Threading.Thread]::CurrentThread.CurrentCulture
try {
    [Threading.Thread]::CurrentThread.CurrentCulture = [Globalization.CultureInfo]::GetCultureInfo('fr-FR')
    Assert-True ((Format-CaptureManifestJson ([long]1234567)) -ceq '1234567') 'Locale changes integer serialization.'
}
finally { [Threading.Thread]::CurrentThread.CurrentCulture = $oldCulture }
Write-Host 'Capture formatter edge cases passed (10 checks).'

# Read-only verification of existing evidence. Never launch the capture tool.
$assets = Join-Path $repositoryRoot 'docs\assets'
$manifest = Get-Content -Raw -LiteralPath (Join-Path $assets 'capture-manifest.json') | ConvertFrom-Json
$names = @()
foreach ($capture in @($manifest.captures)) {
    $name = [string]$capture.file
    Assert-True ($name -cmatch '^idleharbor-[a-z-]+\.png$') 'Unsafe or unsupported capture filename.'
    Assert-True ($names -notcontains $name) 'Duplicate capture filename.'
    $names += $name
    $path = Join-Path $assets $name
    $bytes = [IO.File]::ReadAllBytes($path)
    Assert-True ($bytes.Length -ge 24) "Truncated PNG: $name"
    Assert-True ([BitConverter]::ToString($bytes, 0, 8) -ceq '89-50-4E-47-0D-0A-1A-0A') "Invalid PNG signature: $name"
    Assert-True ([Text.Encoding]::ASCII.GetString($bytes, 12, 4) -ceq 'IHDR') "Missing PNG dimensions: $name"
    $width = ([int]$bytes[16] -shl 24) -bor ([int]$bytes[17] -shl 16) -bor ([int]$bytes[18] -shl 8) -bor [int]$bytes[19]
    $height = ([int]$bytes[20] -shl 24) -bor ([int]$bytes[21] -shl 16) -bor ([int]$bytes[22] -shl 8) -bor [int]$bytes[23]
    Assert-True ($width -eq $capture.width -and $height -eq $capture.height) "PNG dimensions disagree with manifest: $name"
    Assert-True ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ieq [string]$capture.sha256) "PNG hash disagrees with manifest: $name"
    Write-Host "PASS: committed capture bytes and dimensions / $name"
}
Assert-True ($names.Count -eq 5) 'Expected the five committed product captures.'
Write-Host 'Committed capture evidence passed (five PNGs); no desktop capture was run.'
