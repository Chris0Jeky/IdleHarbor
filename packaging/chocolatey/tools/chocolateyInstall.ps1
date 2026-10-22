$ErrorActionPreference = 'Stop'

# Only the x64 portable archive is packaged. Chocolatey's ZIP helper would not
# stop an x86 or ARM64 install on its own, so refuse before anything downloads.
# PROCESSOR_ARCHITEW6432 holds the OS architecture inside a 32-bit process (a
# 32-bit PowerShell on x64 reports x86 in PROCESSOR_ARCHITECTURE; an x64-emulated
# process on ARM64 reports AMD64 there), so it wins whenever it is set.
function Assert-IdleHarborX64Os {
    [CmdletBinding()]
    param(
        [string]$ProcessorArchitecture,
        [string]$ProcessorArchitectureW6432
    )

    $native = $ProcessorArchitecture
    if (-not [string]::IsNullOrWhiteSpace($ProcessorArchitectureW6432)) { $native = $ProcessorArchitectureW6432 }
    if ($native -ine 'AMD64') {
        throw ("The IdleHarbor Chocolatey package installs the 64-bit x64 build only; this Windows reports " +
            "architecture '$native'. Use the architecture-matched portable archive from the GitHub release instead.")
    }
}

Assert-IdleHarborX64Os -ProcessorArchitecture $env:PROCESSOR_ARCHITECTURE `
    -ProcessorArchitectureW6432 $env:PROCESSOR_ARCHITEW6432

$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$packageArgs = @{
    packageName = $env:ChocolateyPackageName
    unzipLocation = $toolsDir
    url64bit = 'https://github.com/Chris0Jeky/IdleHarbor/releases/download/v0.2.0/IdleHarbor-0.2.0-windows-x64-portable.zip'
    checksum64 = '18b4a517ef767c005a6d01ba53c13a79ff50e4a956bd7c43f3635b17b80c75f7'
    checksumType64 = 'sha256'
}

Install-ChocolateyZipPackage @packageArgs
