$ErrorActionPreference = 'Stop'

# Only the x64 portable archive is packaged. Chocolatey's ZIP helper would not
# stop an x86 or ARM64 install on its own, so refuse before anything downloads.
# Read the machine-wide registry value, not the emulation-adjusted process
# environment: Prism can report AMD64 without PROCESSOR_ARCHITEW6432 on ARM64.
function Assert-IdleHarborX64Os {
    [CmdletBinding()]
    param(
        [string]$NativeArchitecture,
        [string]$ProcessorArchitecture,
        [string]$ProcessorArchitectureW6432
    )

    # Process values are injectable for regression coverage, never authoritative.
    # Fail closed even with W6432=AMD64 if the native value cannot be confirmed.
    if ($NativeArchitecture -ine 'AMD64') {
        throw ("The IdleHarbor Chocolatey package installs the 64-bit x64 build only; this Windows reports " +
            "an unsupported or unreadable machine-wide architecture. " +
            "Use the architecture-matched portable archive from the GitHub release instead.")
    }
}

$nativeArchitecture = $null
try {
    $nativeArchitecture = Get-ItemPropertyValue `
        -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment' `
        -Name 'PROCESSOR_ARCHITECTURE' -ErrorAction Stop
}
catch {
    # Missing key/value or denied access must reach the same fail-closed decision.
}
Assert-IdleHarborX64Os -NativeArchitecture $nativeArchitecture `
    -ProcessorArchitecture $env:PROCESSOR_ARCHITECTURE `
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
