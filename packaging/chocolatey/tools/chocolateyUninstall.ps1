$ErrorActionPreference = 'Stop'

$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$executable = Join-Path $toolsDir 'IdleHarbor-0.1.0-windows-x64-portable\IdleHarbor.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    return
}

$owners = @(Get-CimInstance Win32_Process -Filter "Name='IdleHarbor.exe'" -ErrorAction SilentlyContinue |
    Where-Object { $_.ExecutablePath -eq $executable })
if ($owners.Count -eq 0) {
    return
}

Start-Process -FilePath $executable -ArgumentList '--exit' -Wait
$deadline = [DateTime]::UtcNow.AddSeconds(10)
do {
    $owners = @(Get-CimInstance Win32_Process -Filter "Name='IdleHarbor.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.ExecutablePath -eq $executable })
    if ($owners.Count -eq 0) {
        return
    }
    Start-Sleep -Milliseconds 200
} while ([DateTime]::UtcNow -lt $deadline)

throw 'IdleHarbor is still running from the Chocolatey package. Close it and retry the uninstall.'
