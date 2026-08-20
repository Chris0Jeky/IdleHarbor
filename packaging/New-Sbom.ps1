[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$InputPath,

    [Parameter(Mandatory)]
    [string]$OutputPath,

    [string]$Version = '0.1.0',

    [ValidateSet('x64', 'arm64', 'x86')]
    [string]$Architecture = 'x64'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-FileDigestHex([string]$Path, [string]$Algorithm) {
    $hashAlgorithm = [Security.Cryptography.HashAlgorithm]::Create($Algorithm)
    $stream = [IO.File]::OpenRead($Path)
    try {
        return [BitConverter]::ToString($hashAlgorithm.ComputeHash($stream)).Replace('-', '')
    }
    finally {
        $stream.Dispose()
        $hashAlgorithm.Dispose()
    }
}

$inputFile = (Resolve-Path -LiteralPath $InputPath).Path
$fileSha1 = (Get-FileDigestHex $inputFile 'SHA1').ToLowerInvariant()
$hash = (Get-FileDigestHex $inputFile 'SHA256').ToUpperInvariant()
$fileName = Split-Path -Leaf $inputFile
$packageId = "SPDXRef-Package-IdleHarbor-$Architecture"
$fileId = "SPDXRef-File-$($fileName -replace '[^A-Za-z0-9.-]', '-')"
$documentName = "IdleHarbor-$Version-windows-$Architecture"
$sha1 = [Security.Cryptography.SHA1]::Create()
try {
    $packageVerificationCode = [BitConverter]::ToString(
        $sha1.ComputeHash([Text.Encoding]::ASCII.GetBytes($fileSha1))).Replace('-', '').ToLowerInvariant()
}
finally {
    $sha1.Dispose()
}

$document = [ordered]@{
    spdxVersion = 'SPDX-2.3'
    dataLicense = 'CC0-1.0'
    SPDXID = 'SPDXRef-DOCUMENT'
    name = $documentName
    documentNamespace = "https://github.com/Chris0Jeky/IdleHarbor/spdx/$documentName"
    creationInfo = [ordered]@{
        created = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
        creators = @('Tool: IdleHarbor packaging script')
    }
    packages = @(
        [ordered]@{
            SPDXID = $packageId
            name = 'IdleHarbor'
            versionInfo = $Version
            downloadLocation = 'NOASSERTION'
            filesAnalyzed = $true
            packageVerificationCode = [ordered]@{ value = $packageVerificationCode }
            licenseConcluded = 'NOASSERTION'
            licenseDeclared = 'NOASSERTION'
            copyrightText = 'NOASSERTION'
        }
    )
    files = @(
        [ordered]@{
            SPDXID = $fileId
            fileName = $fileName
            checksums = @(
                [ordered]@{ algorithm = 'SHA1'; checksum = $fileSha1 }
                [ordered]@{ algorithm = 'SHA256'; checksum = $hash }
            )
            licenseConcluded = 'NOASSERTION'
            copyrightText = 'NOASSERTION'
        }
    )
    relationships = @(
        [ordered]@{
            spdxElementId = $packageId
            relationshipType = 'CONTAINS'
            relatedSpdxElement = $fileId
        }
    )
}

$destination = [IO.Path]::GetFullPath($OutputPath)
New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
$document | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $destination -Encoding UTF8
Write-Output $destination
