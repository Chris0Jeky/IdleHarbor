[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

# Load Format-CaptureManifestJson without executing the capture script.
function Read-Function([string]$Path, [string]$Name) {
    $tokens = $null
    $errors = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseFile($Path, [ref]$tokens, [ref]$errors)
    Assert-True ($errors.Count -eq 0) "Parse failed: $Path"
    $definition = $ast.Find({ param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $Name
    }, $true)
    Assert-True ($null -ne $definition) "Missing function $Name in $Path"
    return [scriptblock]::Create($definition.Extent.Text)
}

$captureScript = Join-Path $repositoryRoot 'tools\Capture-IdleHarborScreenshots.ps1'
. (Read-Function $captureScript 'Format-CaptureManifestJson')

$sample = [ordered]@{
    schema = 1
    note = 'say "hi" \ path'
    dpi = 192
    flag = $true
    missing = $null
    executable = [ordered]@{
        file = 'IdleHarbor.exe'
        sizeBytes = 42
    }
    captures = @(
        [ordered]@{
            file = 'idleharbor-window.png'
            state = 'Stopped'
        }
    )
    empty = @()
}

$expected = @'
{
  "schema": 1,
  "note": "say \"hi\" \\ path",
  "dpi": 192,
  "flag": true,
  "missing": null,
  "executable": {
    "file": "IdleHarbor.exe",
    "sizeBytes": 42
  },
  "captures": [
    {
      "file": "idleharbor-window.png",
      "state": "Stopped"
    }
  ],
  "empty": []
}
'@
# Here-strings take the checkout's line endings; the formatter always emits LF.
$expected = $expected -replace "`r`n", "`n"

$actual = Format-CaptureManifestJson $sample
Assert-True ($actual -ceq $expected) "Capture manifest JSON differed from the expected layout.`n$actual"
Assert-True (-not $actual.Contains([char]13)) 'Format-CaptureManifestJson emitted a carriage return.'
Assert-True (-not ($actual -match '(?m)^    "schema":')) 'Top-level property used 4-space indentation.'
Assert-True (-not ($actual -match '":  ')) 'A colon was followed by two spaces.'

$parsed = $actual | ConvertFrom-Json
Assert-True ($parsed.schema -eq 1) 'schema did not round-trip.'
Assert-True ($parsed.note -ceq 'say "hi" \ path') 'note did not round-trip.'
Assert-True ($parsed.dpi -eq 192) 'dpi did not round-trip.'
Assert-True ($parsed.flag -eq $true) 'flag did not round-trip.'
Assert-True ($null -eq $parsed.missing) 'null did not round-trip.'
Assert-True ($parsed.executable.file -ceq 'IdleHarbor.exe') 'executable.file did not round-trip.'
Assert-True ($parsed.executable.sizeBytes -eq 42) 'executable.sizeBytes did not round-trip.'
$captures = @($parsed.captures)
Assert-True ($captures.Count -eq 1) 'captures did not round-trip as one element.'
Assert-True ($captures[0].file -ceq 'idleharbor-window.png') 'captures[0].file did not round-trip.'
Assert-True ($captures[0].state -ceq 'Stopped') 'captures[0].state did not round-trip.'
Assert-True (@($parsed.empty).Count -eq 0) 'empty array did not round-trip as empty.'

# A ConvertFrom-Json object (PSCustomObject), as read back from a committed manifest, formats the same way.
$reformatted = Format-CaptureManifestJson ($actual | ConvertFrom-Json)
Assert-True ($reformatted -ceq $actual) "Re-formatting a parsed manifest changed it.`n$reformatted"

Write-Host 'Capture manifest JSON layout passed.'
