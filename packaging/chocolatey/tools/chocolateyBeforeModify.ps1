$ErrorActionPreference = 'Stop'

# Chocolatey runs this from the *installed* package before an upgrade or an
# uninstall. chocolateyUninstall.ps1 is not run on upgrade, so without this the
# running executable keeps a lock on its own package directory and the upgrade
# fails or leaves a mixed install behind.
$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$executable = Join-Path $toolsDir 'IdleHarbor-0.2.0-windows-x64-portable\IdleHarbor.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    return
}

function Get-PackageOwnedProcess {
    return @(Get-CimInstance Win32_Process -Filter "Name='IdleHarbor.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.ExecutablePath -eq $executable })
}

if ((Get-PackageOwnedProcess).Count -eq 0) {
    return
}

# --exit is IdleHarbor's own visible shutdown command: it stops any session,
# clears the power request, and removes the notification-area icon.
Start-Process -FilePath $executable -ArgumentList '--exit' -Wait
$deadline = [DateTime]::UtcNow.AddSeconds(10)
do {
    if ((Get-PackageOwnedProcess).Count -eq 0) {
        return
    }
    Start-Sleep -Milliseconds 200
} while ([DateTime]::UtcNow -lt $deadline)

throw 'IdleHarbor is still running from the Chocolatey package. Close it and retry.'
