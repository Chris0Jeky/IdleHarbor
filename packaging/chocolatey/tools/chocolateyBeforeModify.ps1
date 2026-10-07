$ErrorActionPreference = 'Stop'

# Chocolatey invokes the installed package before both upgrade and uninstall.
# Keep the shutdown behavior identical to the uninstall entry point.
$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$executable = Join-Path $toolsDir 'IdleHarbor-0.2.0-windows-x64-portable\IdleHarbor.exe'
. (Join-Path $toolsDir 'Stop-IdleHarborPackage.ps1')
Stop-IdleHarborPackage -Executable $executable
