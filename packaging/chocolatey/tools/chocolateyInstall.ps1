$ErrorActionPreference = 'Stop'

$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$packageArgs = @{
    packageName = $env:ChocolateyPackageName
    unzipLocation = $toolsDir
    url64bit = 'https://github.com/Chris0Jeky/IdleHarbor/releases/download/v0.1.0/IdleHarbor-0.1.0-windows-x64-portable.zip'
    checksum64 = 'b3bc7e5714543cee3877e94f3c74ecfedb30d3599decef316e57715fdf9f6d28'
    checksumType64 = 'sha256'
}

Install-ChocolateyZipPackage @packageArgs
