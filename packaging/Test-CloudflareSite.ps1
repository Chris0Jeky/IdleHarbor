[CmdletBinding()]
param(
    # Also check the deployed mirror over the network. Off by default so the
    # file-level checks stay runnable in CI and offline, matching the other
    # release checks in this directory.
    [switch]$Live,
    [string]$MirrorUrl = 'https://idleharbor.commit-atlas.workers.dev'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        throw $Message
    }
}

# The canonical site is GitHub Pages. The Cloudflare Worker is a mirror that
# declares that URL, so it consolidates into the canonical host instead of
# competing with it. If that ever stops being true the mirror becomes duplicate
# content, which is what most of this file exists to prevent.
$canonicalUrl = 'https://chris0jeky.github.io/IdleHarbor/'
$canonicalHost = 'chris0jeky.github.io'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$docsRoot = Join-Path $repositoryRoot 'docs'
$wranglerPath = Join-Path $PSScriptRoot 'cloudflare/wrangler.jsonc'
$headersPath = Join-Path $docsRoot '_headers'
$redirectsPath = Join-Path $docsRoot '_redirects'
$robotsPath = Join-Path $docsRoot 'robots.txt'
$indexPath = Join-Path $docsRoot 'index.html'
$sitemapPath = Join-Path $docsRoot 'sitemap.xml'

foreach ($required in $wranglerPath, $headersPath, $redirectsPath, $robotsPath, $indexPath, $sitemapPath) {
    Assert-True (Test-Path -LiteralPath $required -PathType Leaf) "Missing Cloudflare mirror file: $required"
}

# Cloudflare splits _headers and _redirects on newlines, so a CR that this
# repository's CRLF default would otherwise add lands inside the last value on
# every line. .gitattributes pins these to LF; this proves it held.
foreach ($lfOnly in $headersPath, $redirectsPath, $robotsPath) {
    $bytes = [IO.File]::ReadAllBytes($lfOnly)
    Assert-True (-not ($bytes -contains 13)) `
        "$lfOnly contains a CR byte, which Cloudflare would parse into the value that precedes it. Check .gitattributes."
}

# --- wrangler configuration -------------------------------------------------

$wranglerRaw = Get-Content -LiteralPath $wranglerPath -Raw
# Only whole-line comments are stripped, so a // inside a URL string survives.
$wranglerJson = ($wranglerRaw -split "`n" | Where-Object { $_.TrimStart() -notmatch '^//' }) -join "`n"
$wrangler = $wranglerJson | ConvertFrom-Json

Assert-True ($wrangler.name -eq 'idleharbor') `
    "wrangler.jsonc names the Worker '$($wrangler.name)'; the deployed mirror is 'idleharbor' and renaming it would move the site to a different URL."

Assert-True (-not ($wrangler.PSObject.Properties.Name -contains 'main')) `
    'wrangler.jsonc declares a "main" entrypoint. The mirror is deliberately static-assets-only: _headers and _redirects do the work, so there is no Worker code to review or maintain.'

$assetDirectory = $wrangler.assets.directory
Assert-True ($null -ne $assetDirectory) 'wrangler.jsonc does not declare assets.directory.'
$resolvedAssets = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $wranglerPath) $assetDirectory))
$resolvedDocs = [IO.Path]::GetFullPath($docsRoot)
Assert-True ($resolvedAssets.TrimEnd('\', '/') -ieq $resolvedDocs.TrimEnd('\', '/')) `
    "wrangler.jsonc serves '$resolvedAssets' but the site lives in '$resolvedDocs'."

# --- _headers ---------------------------------------------------------------

$headerLines = @(Get-Content -LiteralPath $headersPath)
foreach ($line in $headerLines) {
    Assert-True ($line.Length -le 2000) "A _headers line exceeds Cloudflare's 2,000 character limit."
}
$headerRules = @($headerLines | Where-Object { $_ -match '^\S' -and $_ -notmatch '^#' })
Assert-True ($headerRules.Count -le 100) `
    "_headers declares $($headerRules.Count) rules; Cloudflare allows at most 100."
Assert-True ($headerRules -contains '/*') '_headers has no /* rule, so the security headers would not cover the page.'

$headersText = Get-Content -LiteralPath $headersPath -Raw
foreach ($requiredHeader in 'Content-Security-Policy', 'X-Content-Type-Options', 'Referrer-Policy', 'X-Frame-Options') {
    Assert-True ($headersText -match [regex]::Escape($requiredHeader)) "_headers no longer sets $requiredHeader."
}
# The page is self-contained: no external script, style, font, or image. A CSP
# that permits a remote origin means that stopped being true without this file
# being revisited.
Assert-True ($headersText -notmatch '(?m)^\s*Content-Security-Policy:.*https?://') `
    '_headers allows a remote origin in the CSP. The site loads nothing external, so a remote origin is either a mistake or a new dependency nobody recorded.'

# --- _redirects -------------------------------------------------------------

$redirectLines = @(Get-Content -LiteralPath $redirectsPath | Where-Object { $_ -match '^\S' -and $_ -notmatch '^#' })
Assert-True ($redirectLines.Count -le 2000) `
    "_redirects declares $($redirectLines.Count) rules; Cloudflare allows at most 2,000 static rules."
foreach ($line in $redirectLines) {
    Assert-True ($line.Length -le 1000) "A _redirects line exceeds Cloudflare's 1,000 character limit: $line"
    $parts = @($line -split '\s+' | Where-Object { $_ })
    Assert-True ($parts.Count -ge 2) "Malformed _redirects rule: $line"
    Assert-True ($parts[0].StartsWith('/')) "_redirects source must start with a '/': $line"
    if ($parts.Count -ge 3) {
        Assert-True ($parts[2] -in '301', '302', '303', '307', '308') `
            "_redirects status code '$($parts[2])' is not one Cloudflare accepts: $line"
    }
}

# --- robots.txt -------------------------------------------------------------

$robotsText = Get-Content -LiteralPath $robotsPath -Raw
# A crawler blocked here never reads the canonical tag, and is left treating the
# mirror as an independent copy of the site. Blocking is the one thing this file
# must not do.
Assert-True ($robotsText -notmatch '(?im)^\s*Disallow:\s*/\s*$') `
    'robots.txt disallows the whole mirror. A crawler that cannot fetch the pages never sees <link rel="canonical">, so the mirror would be treated as a separate site instead of folded into the canonical URL.'

# --- IndexNow key -----------------------------------------------------------

# The key is public by design: IndexNow proves control of a host by serving it
# there. It is not a secret, and it belongs in the repository beside the site.
$keyFiles = @(Get-ChildItem -LiteralPath $docsRoot -Filter '*.txt' -File |
    Where-Object { $_.Name -ne 'robots.txt' })
Assert-True ($keyFiles.Count -eq 1) `
    "Expected exactly one IndexNow key file in docs/, found $($keyFiles.Count). A superseded key keeps working until its file is deleted."
$keyFile = $keyFiles[0]
$keyName = [IO.Path]::GetFileNameWithoutExtension($keyFile.Name)
$keyContent = (Get-Content -LiteralPath $keyFile.FullName -Raw).Trim()
Assert-True ($keyContent -ceq $keyName) `
    "IndexNow key file $($keyFile.Name) contains '$keyContent'; IndexNow requires the file to be named after the key it holds."
Assert-True ($keyName -match '^[A-Za-z0-9-]{8,128}$') `
    "IndexNow key '$keyName' is outside the permitted 8-128 character alphanumeric-and-dash range."
$keyBytes = [IO.File]::ReadAllBytes($keyFile.FullName)
Assert-True (-not ($keyBytes -contains 13)) `
    "$($keyFile.Name) contains a CR byte, which some IndexNow endpoints compare literally against the submitted key."

# --- the canonical stays on GitHub Pages ------------------------------------

$indexText = Get-Content -LiteralPath $indexPath -Raw
$canonicalMatch = [regex]::Match($indexText, '(?i)<link\s+rel="canonical"\s+href="([^"]+)"')
Assert-True ($canonicalMatch.Success) 'docs/index.html declares no canonical URL.'
Assert-True ($canonicalMatch.Groups[1].Value -eq $canonicalUrl) `
    "docs/index.html canonical is '$($canonicalMatch.Groups[1].Value)', expected '$canonicalUrl'. The Cloudflare mirror serves this same file, so if the canonical stops naming GitHub Pages the two hosts become duplicates of one another."

$mirrorHost = ([Uri]$MirrorUrl).Host
Assert-True ($indexText -notmatch [regex]::Escape($mirrorHost)) `
    "docs/index.html references the mirror host '$mirrorHost'. The mirror serves this file verbatim, so every absolute URL in it must name the canonical host."

$sitemapText = Get-Content -LiteralPath $sitemapPath -Raw
foreach ($loc in [regex]::Matches($sitemapText, '<loc>([^<]+)</loc>')) {
    $locHost = ([Uri]$loc.Groups[1].Value).Host
    Assert-True ($locHost -ieq $canonicalHost) `
        "sitemap.xml lists '$($loc.Groups[1].Value)', which is not on the canonical host $canonicalHost."
}

Write-Host "Cloudflare mirror configuration validated: $($headerRules.Count) header rule(s), $($redirectLines.Count) redirect(s), IndexNow key $keyName"

# --- optional live checks ---------------------------------------------------

if ($Live) {
    $base = $MirrorUrl.TrimEnd('/')

    $page = Invoke-WebRequest -Uri "$base/" -UseBasicParsing
    Assert-True ($page.StatusCode -eq 200) "Mirror returned $($page.StatusCode) for /."

    $sha = [Security.Cryptography.SHA256]::Create()
    $localHash = [BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($indexPath)))
    $servedHash = [BitConverter]::ToString($sha.ComputeHash($page.RawContentStream.ToArray()))
    Assert-True ($localHash -eq $servedHash) `
        'The deployed mirror is not serving the current docs/index.html. Redeploy it; see packaging/cloudflare/README.md.'

    foreach ($liveHeader in 'Content-Security-Policy', 'X-Content-Type-Options', 'Referrer-Policy', 'X-Frame-Options') {
        Assert-True ($page.Headers.Keys -contains $liveHeader) `
            "Mirror response is missing $liveHeader; the _headers file did not take effect."
    }

    $servedKey = (Invoke-WebRequest -Uri "$base/$($keyFile.Name)" -UseBasicParsing).Content.Trim()
    Assert-True ($servedKey -ceq $keyName) "Mirror serves '$servedKey' at /$($keyFile.Name), expected '$keyName'."

    # _headers and _redirects are consumed by the edge and must never be readable.
    foreach ($hidden in '_headers', '_redirects') {
        $status = 0
        try { $status = (Invoke-WebRequest -Uri "$base/$hidden" -UseBasicParsing).StatusCode }
        catch { $status = [int]$_.Exception.Response.StatusCode }
        Assert-True ($status -eq 404) "Mirror serves /$hidden as an asset (status $status); the edge should consume it instead."
    }

    Write-Host "Live mirror validated at $base"
}
