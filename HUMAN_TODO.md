# Human decisions

IdleHarbor can be built and tested without these decisions. They gate only the named publication
or trust surface.

- [ ] **q-1 — Open-source licence.** Recommended action: approve the MIT licence before v0.1.0 is
  published. Adding a licence grants durable downstream rights and is therefore not inferred.
- [ ] **q-2 — Authenticode signing.** Optional for v0.1.0. If a suitable code-signing certificate
  or managed signing account exists, provide its non-secret identifier; otherwise the release will
  be clearly documented as unsigned with SHA-256 checksums and provenance attestations.
