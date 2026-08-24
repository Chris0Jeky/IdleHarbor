[CmdletBinding(SupportsShouldProcess)]
param(
    # Canonical URLs to announce. The default is the project site itself, which
    # is the only page it has.
    [string[]]$Url = @('https://chris0jeky.github.io/IdleHarbor/'),

    # Report what would be submitted without contacting IndexNow.
    [switch]$WhatIfOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# IndexNow is an open protocol: one POST tells Bing, Yandex, Seznam, and Naver
# that a page changed. Google does not participate, so a Google Search Console
# submission is still owner-only work -- see HUMAN_TODO.md.
#
# Ownership is proved by serving a key file. Ours lives under docs/, which
# GitHub Pages publishes at the canonical site. IndexNow scopes a key file to
# its own directory, so a key at /IdleHarbor/<key>.txt authorises exactly the
# URLs beneath /IdleHarbor/ -- which is every URL this project has.

$docsRoot = Join-Path (Split-Path -Parent $PSScriptRoot) 'docs'
$keyFiles = @(Get-ChildItem -LiteralPath $docsRoot -Filter '*.txt' -File |
    Where-Object { $_.Name -ne 'robots.txt' })
if ($keyFiles.Count -ne 1) {
    throw "Expected exactly one IndexNow key file in docs/, found $($keyFiles.Count)."
}

$key = [IO.Path]::GetFileNameWithoutExtension($keyFiles[0].Name)
$keyLocation = "https://chris0jeky.github.io/IdleHarbor/$($keyFiles[0].Name)"

# A key that is not reachable yet is the usual reason for a 403. Merging to main
# publishes it, but GitHub Pages takes a moment to rebuild.
Write-Host "Checking the key is published at $keyLocation"
try {
    $published = (Invoke-WebRequest -Uri $keyLocation -UseBasicParsing).Content.Trim()
}
catch {
    # Almost always means the commit adding the key has not reached GitHub Pages
    # yet: the merge has to land and Pages has to rebuild before this works.
    throw "Could not read the key file at $keyLocation ($($_.Exception.Message.Trim())). IndexNow proves control by fetching it, so submitting now would return 403."
}
if ($published -cne $key) {
    throw "The key file at $keyLocation contains '$published', expected '$key'. IndexNow would reject the submission with 403."
}

foreach ($candidate in $Url) {
    if (-not $candidate.StartsWith('https://chris0jeky.github.io/IdleHarbor/')) {
        throw "$candidate is outside the directory the key file authorises; IndexNow would answer 422."
    }
}

$payload = [ordered]@{
    host        = 'chris0jeky.github.io'
    key         = $key
    keyLocation = $keyLocation
    urlList     = $Url
}
$body = $payload | ConvertTo-Json -Depth 4

Write-Host "Submitting $($Url.Count) URL(s) with key $key"
if ($WhatIfOnly) {
    Write-Host $body
    return
}

if (-not $PSCmdlet.ShouldProcess('api.indexnow.org', "submit $($Url.Count) URL(s)")) {
    return
}

try {
    $response = Invoke-WebRequest -Uri 'https://api.indexnow.org/IndexNow' -Method Post `
        -ContentType 'application/json; charset=utf-8' -Body $body -UseBasicParsing
    $status = [int]$response.StatusCode
}
catch {
    if ($null -eq $_.Exception.Response) { throw }
    $status = [int]$_.Exception.Response.StatusCode
}

# 200 accepted, 202 accepted with the key still being validated. Everything else
# is a real failure worth reading aloud rather than swallowing.
switch ($status) {
    200 { Write-Host 'IndexNow accepted the submission (200).' }
    202 { Write-Host 'IndexNow accepted the submission; key validation pending (202).' }
    400 { throw 'IndexNow rejected the request as malformed (400).' }
    403 { throw "IndexNow could not validate the key (403). Confirm $keyLocation serves exactly '$key'." }
    422 { throw 'IndexNow rejected the URLs as outside the key location scope (422).' }
    429 { throw 'IndexNow rate-limited the submission (429). Try again later.' }
    default { throw "IndexNow returned an unexpected status $status." }
}
