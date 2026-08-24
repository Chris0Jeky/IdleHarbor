# Human decisions

The licence and signing decisions are resolved and apply to every release so far. Two items need an
owner account that the repository must never hold: a marketplace credential, and a Search Console
verification.

- [x] **q-1 — Open-source licence.** On 2026-08-20, the owner selected GNU GPL version 3. The
  repository and release are licensed `GPL-3.0-only`; no copyright holder was inferred.
- [x] **q-2 — Authenticode signing.** On 2026-08-20, the owner confirmed that no signing identity
  is available. `v0.1.0` and `v0.2.0` are explicitly unsigned and accompanied by SHA-256 checksums,
  SPDX SBOMs, and GitHub artifact attestations. A later signed release requires a managed signing account
  or an Authenticode certificate whose private key remains outside the repository.
- [ ] **q-3 — Chocolatey publication credential.** The reviewed package source and locally packed
  `idleharbor.0.2.0.nupkg` are ready, and the pinned digest is verified against the real published
  archive, but a Chocolatey community account/API key is required to push it for moderation. Create
  or sign in to the owner account at `community.chocolatey.org`, obtain an API key, and supply it
  only through a private local environment or credential command. Never add the key to this
  repository, an issue, a pull request, or chat-visible command output.
- [ ] **q-4 - Search Console sitemap submission.** `docs/sitemap.xml` is published at
  <https://chris0jeky.github.io/IdleHarbor/sitemap.xml>, but a project Pages site cannot serve an
  origin-root `robots.txt` -- crawlers only fetch `https://chris0jeky.github.io/robots.txt`, which
  GitHub controls -- so nothing points a crawler at it. Verify ownership of the Pages site in Google
  Search Console and submit the sitemap URL there. This needs the owner's Google account and cannot
  be done from the repository.
