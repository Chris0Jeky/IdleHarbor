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

# DWM's corner preference selects the radius in DIPs; 0 means no masking.
$decision = $ast.Find({ param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-CaptureCornerRadiusDip'
}, $true)
Assert-True ($null -ne $decision) 'Missing production corner-radius helper.'
. ([scriptblock]::Create($decision.Extent.Text))
Assert-True ((Get-CaptureCornerRadiusDip -Result 0 -Preference 3) -eq 4) 'DWMWCP_ROUNDSMALL must use a 4 DIP radius.'
foreach ($preference in 0, 2) {
    Assert-True ((Get-CaptureCornerRadiusDip -Result 0 -Preference $preference) -eq 8) "DWM preference $preference must use an 8 DIP radius."
}
# DWMWCP_DEFAULT leaves the radius to the surface: popup menus round at 4 DIPs. An explicit preference wins.
Assert-True ((Get-CaptureCornerRadiusDip -Result 0 -Preference 0 -DefaultDip 4) -eq 4) 'DWMWCP_DEFAULT on a popup menu must use its 4 DIP default.'
Assert-True ((Get-CaptureCornerRadiusDip -Result 0 -Preference 2 -DefaultDip 4) -eq 8) 'DWMWCP_ROUND must use 8 DIPs even on a popup surface.'
Assert-True ((Get-CaptureCornerRadiusDip -Result 0 -Preference 1 -DefaultDip 4) -eq 0) 'DWMWCP_DONOTROUND must skip masking on a popup surface.'
# DWMWCP_DONOTROUND and an unsupported attribute (Windows 10) must skip masking.
Assert-True ((Get-CaptureCornerRadiusDip -Result 0 -Preference 1) -eq 0) 'DWMWCP_DONOTROUND must skip masking.'
Assert-True ((Get-CaptureCornerRadiusDip -Result ([int]0x80070057) -Preference 0) -eq 0) 'An unsupported attribute (Windows 10) must skip masking.'
Assert-True ((Get-CaptureCornerRadiusDip -Result ([int]0x80070057) -Preference 3) -eq 0) 'A failed read must skip masking even when the preference would round.'

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
        $mask = $body.IndexOf('Clear-CaptureRoundedCorners -Bitmap $bitmap -RadiusDip $RadiusDip -Dpi $Dpi', [StringComparison]::Ordinal)
        $save = $body.IndexOf('$bitmap.Save(', [StringComparison]::Ordinal)
        Assert-True ($copy -ge 0 -and $mask -gt $copy -and $save -gt $mask) 'Every screen bitmap must be sanitized between copy and save.'
        Assert-True ($body -match 'if \(\$RadiusDip -gt 0\) \{\s+Clear-CaptureRoundedCorners') 'Masking must be gated on the DWM corner radius.'
    }
    else {
        $surface = if ($name -eq 'Save-WindowCapture') { 'Window' } else { 'Popup' }
        $handle = if ($surface -eq 'Window') { '$Window' } else { '$menu' }
        # A popup passes its 4 DIP system default for DWMWCP_DEFAULT; a window keeps the 8 DIP default.
        $read = if ($surface -eq 'Window') { "Read-CaptureCornerRadiusDip $handle)" } else { "Read-CaptureCornerRadiusDip $handle -DefaultDip 4)" }
        Assert-True ($body -match 'Save-ScreenRectangle[^\r\n]+-RadiusDip \(Read-CaptureCornerRadiusDip ') "Missing $surface sanitization at the capture call site."
        Assert-True ($body.Contains("::GetDpiForWindow($handle)")) "$surface radius must use its own physical DPI."
        Assert-True ($body.Contains("-RadiusDip ($read")) "$surface must ask DWM for its corner radius."
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

$cases = @(foreach ($radiusDip in @(4, 8)) {
    foreach ($dpi in @(96, 120, 144, 168, 192, 480)) {
        $radius = $radiusDip * $dpi / 96
        @{ RadiusDip = $radiusDip; Dpi = $dpi; Width = 2 * $radius + 11; Height = 2 * $radius + 7; Radius = $radius; Keyed = $true }
    }
}) + @(
    @{ RadiusDip = 8; Dpi = 192; Width = 3; Height = 5; Radius = 1 },
    @{ RadiusDip = 4; Dpi = 192; Width = 1; Height = 1; Radius = 0 },
    # DONOTROUND and failed reads select radius 0: nothing may be cleared.
    @{ RadiusDip = 0; Dpi = 192; Width = 9; Height = 7; Radius = 0; Keyed = $true }
)
$clearedByCase = @{}

foreach ($case in $cases) {
    $caseCleared = 0
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
        Clear-CaptureRoundedCorners -Bitmap $bitmap -RadiusDip $case.RadiusDip -Dpi $case.Dpi
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
                    Assert-True ($after[$index] -eq $expected -and $saved[$index] -eq $expected) "Corner bytes differ at $($case.RadiusDip) DIP/$($case.Dpi) DPI ($x,$y), BGRA channel $channel (outside=$outside)."
                }
                if ($outside) { $clearedPixels++; $caseCleared++ } else { $preservedPixels++ }
            }
        }
        if ($case.ContainsKey('Keyed')) { $clearedByCase["$($case.RadiusDip)@$($case.Dpi)"] = $caseCleared }
    }
    finally {
        if ($null -ne $decoded) { $decoded.Dispose() }
        $stream.Dispose()
        $bitmap.Dispose()
    }
}
Assert-True ($clearedPixels -gt 0 -and $preservedPixels -gt 0) 'Exercise both cleared and unchanged pixels.'
Assert-True ($clearedByCase['0@192'] -eq 0) 'A zero radius (DONOTROUND or failed read) must clear no pixels.'
# A ROUNDSMALL (4 DIP) window must clear strictly fewer corner pixels than the 8 DIP case.
foreach ($dpi in @(96, 120, 144, 168, 192, 480)) {
    $roundSmall = $clearedByCase["4@$dpi"]
    $roundDefault = $clearedByCase["8@$dpi"]
    Assert-True ($roundSmall -gt 0 -and $roundSmall -lt $roundDefault) "A 4 DIP radius must clear fewer pixels than 8 DIP at $dpi DPI ($roundSmall vs $roundDefault)."
}
Write-Host "Capture rounded corners passed ($($cases.Count) cases, $checks checks; $clearedPixels cleared / $preservedPixels byte-identical interior pixels, including PNG round-trip)."
