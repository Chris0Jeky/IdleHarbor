# Human decisions

The licence and signing decisions are resolved and apply to every release so far. One external
marketplace credential remains owner-only and must never be committed.

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
