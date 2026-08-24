# Cloudflare Workers mirror

`docs/` is published twice. GitHub Pages serves it at
<https://chris0jeky.github.io/IdleHarbor/>, and a Cloudflare Worker serves the same
directory at <https://idleharbor.commit-atlas.workers.dev>.

**GitHub Pages is canonical.** That URL is what `README.md`, the v0.2.0 release notes,
the repository homepage, and `docs/sitemap.xml` all name, and every page carries
`<link rel="canonical">` pointing at it. The mirror serves those bytes unmodified, so a
crawler that reaches the mirror folds it into the canonical URL rather than indexing a
second copy. `Test-CloudflareSite.ps1` fails if that canonical ever stops naming Pages,
because at that moment the two hosts become duplicates of each other.

## Why a mirror at all

GitHub Pages serves this project from a subdirectory, which puts three things out of
reach. The mirror sits at a host root and gets all three:

- **`robots.txt`.** Crawlers only read it at the origin root. On Pages it resolves to
  `/IdleHarbor/robots.txt`, which nothing fetches.
- **Response headers.** Pages offers no control over them. `docs/_headers` applies a
  content security policy, `nosniff`, framing and referrer controls, and a longer cache
  lifetime for `/assets/*`.
- **Redirects.** `docs/_redirects` sends `/download` to whichever release is current.

It is also a second live copy of the site if Pages has an outage.

The IndexNow key in `docs/` is deliberately *not* one of these. It is served from the
canonical Pages path, because IndexNow scopes a key file to its own directory and the
URLs we submit are the canonical ones. See `packaging/README.md`.

## Shape

Static assets only — there is no JavaScript entrypoint. `wrangler.jsonc` declares
`assets` with no `main`, so Cloudflare serves `docs/` directly and applies `_headers`
and `_redirects` at the edge. Those two files are consumed by the edge rather than
served, so they return 404. Nothing here has to be reviewed as running code, and
`Test-CloudflareSite.ps1` fails if a `main` entrypoint is ever added.

`_headers`, `_redirects`, and the IndexNow key file are pinned to LF in `.gitattributes`.
This repository checks out CRLF everywhere else, and Cloudflare splits these files on
newlines: a CR would end up inside the last value on every line.

## Deploying

The mirror is not deployed by CI — no workflow holds a Cloudflare credential. Redeploy
by hand after any change under `docs/`:

```powershell
$env:CLOUDFLARE_API_TOKEN = '<a token with Workers Scripts:Edit>'
$env:CLOUDFLARE_ACCOUNT_ID = '<account id>'
npx --yes wrangler@4 deploy --config packaging/cloudflare/wrangler.jsonc
```

Then confirm the deployed bytes match the working tree:

```powershell
.\packaging\Test-CloudflareSite.ps1 -Live
```

`-Live` compares the served `index.html` against `docs/index.html` byte for byte, so it
fails when the mirror has fallen behind Pages. Without `-Live` the script only checks
the files, which is what CI runs.

Because the deploy is manual, the mirror can lag `main`. That is tolerable precisely
because it is not canonical: a stale mirror never outranks Pages, and its canonical tag
still points at the current site.
