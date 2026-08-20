# Third-party notices and provenance

IdleHarbor is an independent implementation. Its current source, motion paths, artwork, and release
artifacts do not include source code, binaries, or assets from Arkane Systems Mouse Jiggler.

The project began after evaluating the Mouse Jiggler product category. One early pre-release commit
translated the small coordinate sequences in Arkane Systems' `JigglePatterns.cs` into IdleHarbor's
safe-anchor representation. Before IdleHarbor's first licensed release, every translated sequence
was removed and replaced with independently designed and tested IdleHarbor paths. The independence
claim above applies to the current release tree; this historical derivation and replacement boundary
are recorded here for transparency:

- Arkane Systems Mouse Jiggler: https://github.com/arkane-systems/mousejiggler
- Referenced upstream file: `MouseJiggler/JigglePatterns.cs`
- Upstream licence at the time of review: `Microsoft Public License (Ms-PL) / Modified`
- IdleHarbor historical comparison commit: `0602263b7e599eef1421ddcceacdf61295b37707`

Mouse Jiggler and its associated names remain the property of their respective owners. This notice
does not grant trademark rights or imply endorsement or affiliation.
