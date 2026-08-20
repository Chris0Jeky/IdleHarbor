# Project state

Last updated: 2026-08-20

## Current milestone

IdleHarbor `0.1.0` is a native Windows release candidate with a platform-neutral policy core,
validated INI settings, strict CLI, upstream-compatible motion patterns, genuine-input observation,
power requests, battery/fullscreen/session safeguards, visible tray controls, an emergency stop
path, recoverable settings handling, and transactional per-user installation. No licence, tag, or
release has been created or published.

The live merged baseline is `origin/main` at
`90977a9be9804a719c853912cca9b0aeaf3524b0`. PRs
[#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7),
[#8](https://github.com/Chris0Jeky/IdleHarbor/pull/8),
[#10](https://github.com/Chris0Jeky/IdleHarbor/pull/10),
[#13](https://github.com/Chris0Jeky/IdleHarbor/pull/13),
[#16](https://github.com/Chris0Jeky/IdleHarbor/pull/16),
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
`agent/v0.1.0-release-tag-data`, merged into `main` at
`90977a9be9804a719c853912cca9b0aeaf3524b0` and closed issue
[#12](https://github.com/Chris0Jeky/IdleHarbor/issues/12). Its release-tag workflow contract and
Windows PowerShell 5.1-compatible tests are now part of the merged baseline. No real release is
part of PR #16.

### Portfolio and publication

Issue [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6) tracks final exact-build Running,
Paused, full-window, viewport, and tray captures plus the repository social preview. The pushed
checkpoint branch `agent/v0.1.0-portfolio-polish` predates the final UI/motion baseline; it must
incorporate final `main`, replace every earlier capture, refresh performance evidence, and pass
visual/privacy review before its PR is opened.

Issue [#26](https://github.com/Chris0Jeky/IdleHarbor/issues/26) has an unmerged checkpoint on branch
`agent/v0.1.0-packaging-isolation`: a bounded, abandoned-owner-safe Local mutex serializes the
PowerShell 7 and 5.1 packaging suites while retaining the existing global transaction-residue
assertions. The sequential suite passes under both runtimes. The first concurrent proof returned
exit code `0` for PowerShell 7 and `1` for Windows PowerShell 5.1; its failure output was not
captured, and no transaction residue remained. Treat the slice as NOT verified until that failure
is reproduced with separate stdout/stderr capture, fixed, and the concurrent matrix passes.

Issue [#27](https://github.com/Chris0Jeky/IdleHarbor/issues/27) tracks redundant Stop requests that
can change internal action focus without a state transition. Issue
[#28](https://github.com/Chris0Jeky/IdleHarbor/issues/28) tracks control sizing when Windows uses a
scrollbar wider than IdleHarbor's logical body inset. Both were classified non-blocking in PR #24's
bounded final review.

`HUMAN_TODO.md` remains authoritative:

- q-1: explicit open-source licence approval is unresolved. Do not add a `LICENSE`, infer a
  copyright holder, create a tag, publish a release, or submit package manifests until the user
  supplies that decision.
- q-2: Authenticode signing is optional. Without a signing identity, document `0.1.0` as unsigned
  and rely on checksums, SBOMs, and GitHub provenance attestations.

## Resume order

1. Reproduce issue #26's concurrent Windows PowerShell 5.1 failure with separate stdout/stderr
   capture, fix the smallest confirmed cause, then pass the sequential and concurrent matrix before
   review or PR creation.
2. Finish issue #6 from the final exact build, including genuine captures, README/benchmark truth,
   and the uploaded GitHub social preview.
3. Run the final native runtime, packaging, installer, performance, security, accessibility, and
   release-artifact audit. Track or finish #26-#28 according to their bounded severity and release
   impact.
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
