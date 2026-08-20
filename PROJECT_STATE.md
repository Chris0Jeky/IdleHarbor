# Project state

Last updated: 2026-08-20

## Current milestone

IdleHarbor `0.1.0` is a native Windows release candidate with a platform-neutral policy core,
validated INI settings, strict CLI, upstream-compatible motion patterns, genuine-input observation,
power requests, battery/fullscreen/session safeguards, visible tray controls, an emergency stop
path, recoverable settings handling, and transactional per-user installation. No licence, tag, or
release has been created or published.

The live merged baseline is `origin/main` at
`b9bfa82a45dd8cd777c3232a4d71b68f8ef9a9c4`. PRs
[#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7),
[#8](https://github.com/Chris0Jeky/IdleHarbor/pull/8),
[#10](https://github.com/Chris0Jeky/IdleHarbor/pull/10),
[#13](https://github.com/Chris0Jeky/IdleHarbor/pull/13),
[#16](https://github.com/Chris0Jeky/IdleHarbor/pull/16),
[#17](https://github.com/Chris0Jeky/IdleHarbor/pull/17),
[#18](https://github.com/Chris0Jeky/IdleHarbor/pull/18),
[#24](https://github.com/Chris0Jeky/IdleHarbor/pull/24), and
[#25](https://github.com/Chris0Jeky/IdleHarbor/pull/25),
[#29](https://github.com/Chris0Jeky/IdleHarbor/pull/29), and
[#30](https://github.com/Chris0Jeky/IdleHarbor/pull/30) landed dependency pinning, settings recovery,
installer/release trust, the high-DPI viewport, rollback recovery, fixed viewport safety regions,
upstream motion-multiplier parity, final viewport accessibility polish, and packaging-test isolation.
Issues #2, #3, #4, #5, #9, #11, #12, #14, #15, #19, #20, #21, #22, #23, #26, #27, and #28 are closed.

The landed installer evidence includes x64 CTest plus complete packaging and real rollback matrices
under PowerShell 7 and Windows PowerShell 5.1. It covers each startup mechanism, fresh failure
cleanup, exact update restoration, preservation of unknown files, Task Scheduler folder ownership,
hard-link rejection, successful update, uninstall, and retained recovery material only when
managed-file restoration is incomplete.

PR #24 passed exact-head hosted x64/ARM64/x86, CodeQL, and C++ analysis plus Visual Studio 2019 x64
Release CTest 7/7. Its descendant-aware desktop harness passed under PowerShell 7 and Windows
PowerShell 5.1 at 192 DPI: viewport-owned scrollbar geometry, fixed status/Start/Stop rectangles,
wheel/combo/line/page routing, resize-triggered focus reveal, state-aware forward and reverse
keyboard navigation, keyboard Start/Stop focus handoff, lifecycle states, and tray restore all
passed. A true cross-monitor mixed-DPI transition remains unverified because only one display is
attached.

## Active integration queue

### Release workflow

PR [#16](https://github.com/Chris0Jeky/IdleHarbor/pull/16), branch
`agent/v0.1.0-release-tag-data`, merged into `main` at
`90977a9be9804a719c853912cca9b0aeaf3524b0` and closed issue
[#12](https://github.com/Chris0Jeky/IdleHarbor/issues/12). Its release-tag workflow contract and
Windows PowerShell 5.1-compatible tests are now part of the merged baseline. No real release is
part of PR #16.

### Portfolio and publication

Issue [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6) tracks final exact-build Running,
Paused, full-window, viewport, and tray captures plus the repository social preview. This branch
starts from merged main `b9bfa82` and records the privacy-safe capture set, high-contrast social
SVG/PNG, and exact x64 demo evidence. The unsigned 0.1.0 executable is 516,608 bytes with SHA-256
`432026b0719671bd33c1b0ca0f7df809fec20e73fc05b38b6e513606584f6d54`; three 60-second stopped and
active system-request runs are documented in `docs/BENCHMARKS.md`. The GitHub social-preview upload
remains a pre-merge publication action and is not claimed complete here.

PR [#29](https://github.com/Chris0Jeky/IdleHarbor/pull/29) merged the issue
[#26](https://github.com/Chris0Jeky/IdleHarbor/issues/26) fix into `main` at
`1aa55b754239e1899b09bb6f785299ed2550879b`. A bounded, abandoned-owner-safe Local mutex serializes
the PowerShell 7 and 5.1 packaging suites while retaining global transaction-residue assertions.
SBOM and checksum generation use in-process .NET hashing. The final concurrent proof returned exit
code `0` for both runtimes with empty stderr and no transaction residue; sequential proofs also
returned exit code `0` under both runtimes.

Issue [#27](https://github.com/Chris0Jeky/IdleHarbor/issues/27) tracks redundant Stop requests that
can change internal action focus without a state transition. Issue
[#28](https://github.com/Chris0Jeky/IdleHarbor/issues/28) tracks control sizing when Windows uses a
scrollbar wider than IdleHarbor's logical body inset. Both were classified non-blocking in PR #24's
bounded final review.

The merged UI-final changes implement both bounded UI fixes: deferred Start focus
is now posted only after an actual active-to-stopped transition, and body layout measures the
settings viewport after sizing it, using its effective client width for breakpoint and fill-width
decisions. `UpdateViewport` now converges layout and scroll-range publication across native
scrollbar-driven reflow, so a 560-logical-pixel breakpoint transition cannot leave a stale thumb
range or unreachable bottom controls. Deterministic layout tests cover 96/120/144/168/192 DPI, the
560 logical boundary, and a 48-logical-pixel scrollbar case. The Visual Studio 2019 x64 Release
build and CTest 7/7 passed. The reusable native desktop harness passed under PowerShell 7 and
Windows PowerShell 5.1 at 192 DPI, including every real body control staying within the viewport
client edge and redundant forwarded `--stop` preserving focus. Only one display is attached for
mixed-DPI verification.

`HUMAN_TODO.md` remains authoritative:

- q-1: explicit open-source licence approval is unresolved. Do not add a `LICENSE`, infer a
  copyright holder, create a tag, publish a release, or submit package manifests until the user
  supplies that decision.
- q-2: Authenticode signing is optional. Without a signing identity, document `0.1.0` as unsigned
  and rely on checksums, SBOMs, and GitHub provenance attestations.

## Resume order

1. Finish issue #6 from the final exact build, including genuine captures, README/benchmark truth,
   and the uploaded GitHub social preview.
2. Run the final native runtime, packaging, installer, performance, security, accessibility, and
   release-artifact audit.
3. Resolve q-1 and q-2 with the user; only then add the approved licence, tag/publish `v0.1.0`, and
   derive package-manager manifests from verified real URLs and hashes.

## Proving commands

On this machine use Visual Studio Build Tools 2019:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 16 2019" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\packaging\Test-ReleaseWorkflow.ps1
.\packaging\Test-Packaging.ps1
```

Run the packaging and release-workflow checks sequentially under PowerShell 7 and Windows
PowerShell 5.1. CI additionally builds ARM64 and Win32. Hosted ARM64 evidence proves build/test, not
representative ARM64 runtime behavior. `powercfg /requests` remains unverified locally because it
requires elevation.
