# Project state

Last updated: 2026-08-20

## Current milestone

IdleHarbor `0.1.0` is a native Windows release candidate with a platform-neutral policy core,
validated INI settings, strict CLI, upstream-compatible motion patterns, genuine-input observation,
power requests, battery/fullscreen/session safeguards, visible tray controls, an emergency stop
path, recoverable settings handling, and transactional per-user installation. No release, tag, or
licence has been published.

The live merged baseline is `origin/main` at
`0def9779f462f70d53e7329e65919e659af6c9b9`. PRs
[#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7),
[#8](https://github.com/Chris0Jeky/IdleHarbor/pull/8),
[#10](https://github.com/Chris0Jeky/IdleHarbor/pull/10),
[#13](https://github.com/Chris0Jeky/IdleHarbor/pull/13),
[#17](https://github.com/Chris0Jeky/IdleHarbor/pull/17),
[#18](https://github.com/Chris0Jeky/IdleHarbor/pull/18), and
[#25](https://github.com/Chris0Jeky/IdleHarbor/pull/25) landed dependency pinning,
settings recovery, installer/release trust, the high-DPI viewport, rollback recovery, fixed viewport
safety regions, and upstream motion-multiplier parity. Issues #2, #3, #4, #5, #9, #11, #14, #15,
and #23 are closed.

The landed installer evidence includes x64 CTest plus complete packaging and real rollback matrices
under PowerShell 7 and Windows PowerShell 5.1. It covers each startup mechanism, fresh failure
cleanup, exact update restoration, preservation of unknown files, Task Scheduler folder ownership,
hard-link rejection, successful update, uninstall, and retained recovery material only when
managed-file restoration is incomplete.

## Active integration queue

### Final viewport accessibility polish

Ready-for-review PR [#24](https://github.com/Chris0Jeky/IdleHarbor/pull/24), branch
`agent/v0.1.0-ui-polish`, is open and unmerged. It implements issues #19-#22:

- keyboard traversal follows visual order, with Save before fixed Start/Stop;
- keyboard activation transfers focus to the newly enabled Stop or Start action after a session
  transition instead of leaving keyboard focus unset;
- the native vertical scrollbar belongs to only the settings viewport;
- resize/DPI layout changes re-reveal a still-focused clipped body control without reintroducing
  unchanged-focus wheel snap-back;
- stacked body controls remain horizontally bounded at sub-100-logical-pixel widths.

The branch has incorporated `origin/main` after PR #25. Its integrated pre-focus-fix head passed
hosted x64/ARM64/x86, CodeQL, and C++ analysis; the current code passes a Visual Studio 2019 x64
Release build and CTest 7/7. The descendant-aware desktop harness also passes under PowerShell 7 and
Windows PowerShell 5.1 at 192 DPI: viewport-owned scrollbar geometry, fixed status/Start/Stop
rectangles, wheel/combo/line/page routing, resize-triggered focus reveal, state-aware forward and
reverse keyboard navigation, keyboard Start/Stop focus handoff, lifecycle states, and tray restore
all passed. The final pushed head still needs hosted checks and a scoped fresh-context review. A true
cross-monitor mixed-DPI transition is not locally verifiable because only one display is attached.

### Release workflow

PR [#16](https://github.com/Chris0Jeky/IdleHarbor/pull/16) implements issue
[#12](https://github.com/Chris0Jeky/IdleHarbor/issues/12) by carrying the triggering tag through
step-local `RELEASE_TAG` data rather than direct PowerShell interpolation. Its workflow contract,
packaging tests, x64 CTest, hosted checks, and independent review passed after the PR #17/#18 base,
but it must incorporate the final merged UI/motion/docs state once more before merge. No real
release was run.

### Portfolio and publication

Issue [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6) tracks final exact-build Running,
Paused, window, viewport, and tray captures plus the repository social preview. The checkpoint branch
`agent/v0.1.0-portfolio-polish` is pushed but must incorporate final `main`, replace its earlier
captures, and pass visual/privacy review before opening a PR.

Issue [#26](https://github.com/Chris0Jeky/IdleHarbor/issues/26) records a LOW test-isolation
limitation: simultaneous PowerShell 7 and 5.1 packaging suites can observe each other's legitimate
in-flight transaction directory. Sequential suites pass and leave no residue; this is not an
installer cleanup defect.

`HUMAN_TODO.md` remains authoritative:

- q-1: explicit open-source licence approval is unresolved. Do not add a `LICENSE`, infer a
  copyright holder, create a tag, publish a release, or submit package manifests until the user
  supplies that decision.
- q-2: Authenticode signing is optional. Without a signing identity, document `0.1.0` as unsigned
  and rely on checksums, SBOMs, and GitHub provenance attestations.

## Resume order

1. Re-prove and merge PR #24 at its new exact head; confirm #19-#22 close.
2. Incorporate final `main` into PR #16, reconcile this state file once, re-run its scoped
   PowerShell/native/hosted checks and review, then merge and confirm #12 closes.
3. Finish issue #6 with genuine final-build captures, README/benchmark truth, and the uploaded
   GitHub social preview.
4. Run the final native, packaging, installer, performance, security, accessibility, and release
   artifact audit.
5. Resolve q-1 and q-2 with the user; only then tag/publish `v0.1.0` and derive package-manager
   manifests from verified real URLs and hashes.

## Proving commands

On this machine use Visual Studio Build Tools 2019:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 16 2019" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\packaging\Test-Packaging.ps1
```

CI additionally builds ARM64 and Win32. Hosted ARM64 evidence proves build/test, not representative
ARM64 runtime behavior. `powercfg /requests` remains unverified locally because it requires
elevation.
