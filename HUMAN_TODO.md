# Human decisions

The publication decisions for `v0.1.0` are resolved. Future signing or store-account work can be
reopened as a new decision when the required identity exists.

- [x] **q-1 — Open-source licence.** On 2026-08-20, the owner selected GNU GPL version 3. The
  repository and release are licensed `GPL-3.0-only`; no copyright holder was inferred.
- [x] **q-2 — Authenticode signing.** On 2026-08-20, the owner confirmed that no signing identity
  is available. `v0.1.0` will be explicitly unsigned and accompanied by SHA-256 checksums, SPDX
  SBOMs, and GitHub artifact attestations. A later signed release requires a managed signing account
  or an Authenticode certificate whose private key remains outside the repository.
