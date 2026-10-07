$ErrorActionPreference = 'Stop'

# Do not rely on beforeModify having stopped the app: it can start again.
$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$executable = Join-Path $toolsDir 'IdleHarbor-0.2.0-windows-x64-portable\IdleHarbor.exe'
. (Join-Path $toolsDir 'Stop-IdleHarborPackage.ps1')
Stop-IdleHarborPackage -Executable $executable
