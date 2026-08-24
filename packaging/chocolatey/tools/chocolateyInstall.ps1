$ErrorActionPreference = 'Stop'

$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$packageArgs = @{
    packageName = $env:ChocolateyPackageName
    unzipLocation = $toolsDir
    url64bit = 'https://github.com/Chris0Jeky/IdleHarbor/releases/download/v0.2.0/IdleHarbor-0.2.0-windows-x64-portable.zip'
    checksum64 = '18b4a517ef767c005a6d01ba53c13a79ff50e4a956bd7c43f3635b17b80c75f7'
    checksumType64 = 'sha256'
}

Install-ChocolateyZipPackage @packageArgs
