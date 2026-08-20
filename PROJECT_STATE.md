# Project state

Last updated: 2026-08-20

## Current milestone

IdleHarbor `0.1.0` is a native Windows release candidate with a platform-neutral policy core,
validated INI settings, strict CLI, upstream-compatible motion patterns, genuine-input observation,
power requests, battery/fullscreen/session safeguards, visible tray controls, an emergency stop
path, recoverable settings handling, and transactional per-user installation. No licence, tag, or
release has been created or published.

The live merged baseline is `origin/main` at
`4c0aa98d6ba2091ae073c325ff3cf4183f7de5e3`. PRs
[#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7),
[#8](https://github.com/Chris0Jeky/IdleHarbor/pull/8),
[#10](https://github.com/Chris0Jeky/IdleHarbor/pull/10),
[#13](https://github.com/Chris0Jeky/IdleHarbor/pull/13),
[#17](https://github.com/Chris0Jeky/IdleHarbor/pull/17),
[#18](https://github.com/Chris0Jeky/IdleHarbor/pull/18),
[#24](https://github.com/Chris0Jeky/IdleHarbor/pull/24), and
[#25](https://github.com/Chris0Jeky/IdleHarbor/pull/25) landed dependency pinning, settings recovery,
installer/release trust, the high-DPI viewport, rollback recovery, fixed viewport safety regions,
upstream motion-multiplier parity, and final viewport accessibility polish. Issues #2, #3, #4, #5,
#9, #11, #14, #15, #19, #20, #21, #22, and #23 are closed.

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
`agent/v0.1.0-release-tag-data`, implements issue
[#12](https://github.com/Chris0Jeky/IdleHarbor/issues/12) by carrying the triggering tag through
step-local `RELEASE_TAG` data rather than direct PowerShell interpolation. The resolve and publish
paths consume `$env:RELEASE_TAG`, and `packaging/Test-ReleaseWorkflow.ps1` proves the contract with
Windows PowerShell 5.1-compatible syntax.

The branch has incorporated the final UI/motion baseline. Its current integrated code passes the
Visual Studio 2019 x64 Release build and CTest 7/7 plus the release-workflow and packaging suites
sequentially under PowerShell 7 and Windows PowerShell 5.1. Before merge, the final pushed head still
needs hosted x64/ARM64/x86 plus CodeQL/C++ analysis, a scoped fresh-context review, conversation
resolution, and the three-minute aging floor. No real release is part of PR #16.

### Portfolio and publication

Issue [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6) tracks final exact-build Running,
Paused, full-window, viewport, and tray captures plus the repository social preview. The pushed
checkpoint branch `agent/v0.1.0-portfolio-polish` predates the final UI/motion baseline; it must
incorporate final `main`, replace every earlier capture, refresh performance evidence, and pass
visual/privacy review before its PR is opened.

Issue [#26](https://github.com/Chris0Jeky/IdleHarbor/issues/26) records a LOW packaging-test
isolation limitation: simultaneous PowerShell 7 and 5.1 suites can observe each other's legitimate
in-flight transaction directory. Sequential suites pass and leave no residue.

Issue [#27](https://github.com/Chris0Jeky/IdleHarbor/issues/27) tracks redundant Stop requests that
can change internal action focus without a state transition. Issue
[#28](https://github.com/Chris0Jeky/IdleHarbor/issues/28) tracks control sizing when Windows uses a
scrollbar wider than IdleHarbor's logical body inset. Both were classified non-blocking in PR #24's
bounded final review.

The follow-up branch `agent/v0.1.0-ui-final` implements both bounded UI fixes: deferred Start focus
is now posted only after an actual active-to-stopped transition, and body layout measures the
settings viewport after sizing it, using its effective client width for breakpoint and fill-width
decisions. Deterministic layout tests cover 96/120/144/168/192 DPI and a 48-logical-pixel scrollbar
case. The Visual Studio 2019 x64 Release build, CTest 7/7, and the existing native desktop harness
passed under PowerShell 7 and Windows PowerShell 5.1 at 192 DPI. The external harness does not yet
exercise redundant `--stop` focus preservation as a named assertion, and only one display is
attached for mixed-DPI verification.

`HUMAN_TODO.md` remains authoritative:

- q-1: explicit open-source licence approval is unresolved. Do not add a `LICENSE`, infer a
  copyright holder, create a tag, publish a release, or submit package manifests until the user
  supplies that decision.
- q-2: Authenticode signing is optional. Without a signing identity, document `0.1.0` as unsigned
  and rely on checksums, SBOMs, and GitHub provenance attestations.

## Resume order

1. Re-prove, review, and merge PR #16; confirm issue #12 closes and sweep late review once.
2. Finish issue #6 from the final exact build, including genuine captures, README/benchmark truth,
   and the uploaded GitHub social preview.
3. Integrate and review the `agent/v0.1.0-ui-final` follow-up, then run the final native runtime,
   packaging, installer, performance, security, accessibility, and release-artifact audit. Track or
   finish #26-#28 according to their bounded severity and release impact.
4. Resolve q-1 and q-2 with the user; only then add the approved licence, tag/publish `v0.1.0`, and
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
