[CmdletBinding()]
param(
    # Allows the same assertions to exercise a helper-disabled mutation offline.
    [string]$CaptureScript = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if ([string]::IsNullOrWhiteSpace($CaptureScript)) {
    $CaptureScript = Join-Path (Split-Path -Parent $PSScriptRoot) 'tools\Capture-IdleHarborScreenshots.ps1'
}
$checks = 0
$clearedPixels = 0
$preservedPixels = 0
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    $script:checks++
}

# Extract only the pure helper. Do not execute the capture workflow/native APIs.
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($CaptureScript, [ref]$tokens, [ref]$errors)
Assert-True ($errors.Count -eq 0) 'The capture script must parse.'
$definition = $ast.Find({ param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Clear-CaptureRoundedCorners'
}, $true)
Assert-True ($null -ne $definition) 'Missing production corner helper.'
. ([scriptblock]::Create($definition.Extent.Text))

# Protect the save seam and both call sites without touching a live desktop.
foreach ($name in @('Save-ScreenRectangle', 'Save-WindowCapture', 'Save-TrayMenuCapture')) {
    $function = $ast.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
    }, $true)
    Assert-True ($null -ne $function) "Missing capture path: $name"
    $body = $function.Extent.Text
    if ($name -eq 'Save-ScreenRectangle') {
        Assert-True ($body -match 'Format32bppArgb') 'Captures must retain a writable alpha channel.'
        $copy = $body.IndexOf('$graphics.CopyFromScreen(', [StringComparison]::Ordinal)
        $mask = $body.IndexOf('Clear-CaptureRoundedCorners -Bitmap $bitmap -Surface $Surface -Dpi $Dpi', [StringComparison]::Ordinal)
        $save = $body.IndexOf('$bitmap.Save(', [StringComparison]::Ordinal)
        Assert-True ($copy -ge 0 -and $mask -gt $copy -and $save -gt $mask) 'Every screen bitmap must be sanitized between copy and save.'
    }
    else {
        $surface = if ($name -eq 'Save-WindowCapture') { 'Window' } else { 'Popup' }
        $handle = if ($surface -eq 'Window') { '$Window' } else { '$menu' }
        Assert-True ($body -match ('Save-ScreenRectangle[^\r\n]+-Surface ' + $surface + ' -Dpi')) "Missing $surface sanitization at the capture call site."
        Assert-True ($body.Contains("::GetDpiForWindow($handle)")) "$surface radius must use its own physical DPI."
    }
}

function Read-BitmapBytes([Drawing.Bitmap]$Bitmap) {
    $bounds = New-Object Drawing.Rectangle(0, 0, $Bitmap.Width, $Bitmap.Height)
    $data = $Bitmap.LockBits($bounds, [Drawing.Imaging.ImageLockMode]::ReadOnly,
        [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        Assert-True ($data.Stride -eq $Bitmap.Width * 4) 'Fixture must have tightly packed BGRA rows.'
        $bytes = New-Object byte[] ($data.Stride * $Bitmap.Height)
        [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
        return ,$bytes
    }
    finally { $Bitmap.UnlockBits($data) }
}

$cases = @(foreach ($surface in @('Window', 'Popup')) {
    foreach ($dpi in @(96, 120, 144, 168, 192, 480)) {
        $radius = if ($surface -eq 'Window') { 8 * $dpi / 96 } else { 4 * $dpi / 96 }
        @{ Surface = $surface; Dpi = $dpi; Width = 2 * $radius + 11; Height = 2 * $radius + 7; Radius = $radius }
    }
}) + @(
    @{ Surface = 'Window'; Dpi = 192; Width = 3; Height = 5; Radius = 1 },
    @{ Surface = 'Popup'; Dpi = 192; Width = 1; Height = 1; Radius = 0 }
)

foreach ($case in $cases) {
    $bitmap = New-Object Drawing.Bitmap($case.Width, $case.Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stream = New-Object IO.MemoryStream
    $decoded = $null
    try {
        for ($y = 0; $y -lt $bitmap.Height; $y++) {
            for ($x = 0; $x -lt $bitmap.Width; $x++) {
                # High contrast even under zero alpha: alpha-only masking must fail.
                $alpha = @(0, 127, 255)[($x + $y) % 3]
                $green = if (($x + $y) % 2 -eq 0) { 255 } else { 0 }
                $bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($alpha, 255, $green, 255 - $green))
            }
        }
        $before = Read-BitmapBytes $bitmap
        Clear-CaptureRoundedCorners -Bitmap $bitmap -Surface $case.Surface -Dpi $case.Dpi
        $after = Read-BitmapBytes $bitmap
        $bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
        $stream.Position = 0
        $decoded = [Drawing.Bitmap]::FromStream($stream)
        $saved = Read-BitmapBytes $decoded
        for ($y = 0; $y -lt $bitmap.Height; $y++) {
            for ($x = 0; $x -lt $bitmap.Width; $x++) {
                # Independent whole-square oracle: use the farthest horizontal
                # and vertical distances from the rounded rectangle's inner box.
                $dx = [Math]::Max(0, [Math]::Max($case.Radius - $x, $x + 1 - ($bitmap.Width - $case.Radius)))
                $dy = [Math]::Max(0, [Math]::Max($case.Radius - $y, $y + 1 - ($bitmap.Height - $case.Radius)))
                $outside = $dx * $dx + $dy * $dy -gt $case.Radius * $case.Radius
                $offset = 4 * ($y * $bitmap.Width + $x)
                for ($channel = 0; $channel -lt 4; $channel++) {
                    $index = $offset + $channel
                    $expected = if ($outside) { 0 } else { $before[$index] }
                    Assert-True ($after[$index] -eq $expected -and $saved[$index] -eq $expected) "Corner bytes differ at $($case.Surface)/$($case.Dpi) DPI ($x,$y), BGRA channel $channel (outside=$outside)."
                }
                if ($outside) { $clearedPixels++ } else { $preservedPixels++ }
            }
        }
    }
    finally {
        if ($null -ne $decoded) { $decoded.Dispose() }
        $stream.Dispose()
        $bitmap.Dispose()
    }
}
Assert-True ($clearedPixels -gt 0 -and $preservedPixels -gt 0) 'Exercise both cleared and unchanged pixels.'
Write-Host "Capture rounded corners passed ($($cases.Count) cases, $checks checks; $clearedPixels cleared / $preservedPixels byte-identical interior pixels, including PNG round-trip)."
