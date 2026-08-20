# Project state

Last updated: 2026-08-20

## Current milestone

IdleHarbor `0.1.0` is a native Windows release candidate with a platform-neutral policy core,
validated INI settings, strict CLI, bounded motion, genuine-input observation, power requests,
battery/fullscreen/session safeguards, visible tray controls, an emergency stop path, and
recoverable settings handling. No release or tag has been published.

`origin/main` is `b4bf3dd850bf4abe408fbece4d473c2117bf2bef`. Pull request
[#8](https://github.com/Chris0Jeky/IdleHarbor/pull/8) landed settings-recovery safeguards and pull
request [#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7) updated pinned GitHub Actions. Their
exact-head checks and required reviews passed.

## Active integration queue

### Installer and release trust

Pull request [#10](https://github.com/Chris0Jeky/IdleHarbor/pull/10), branch
`agent/v0.1.0-installer-trust`, has exact head
`166f3da203a0038cc377b5f0df8d5e9347324078`. It implements transactional install/update rollback,
owned-startup restoration, ownership-safe Task Scheduler folder cleanup, single-link managed-file
validation, Windows PowerShell 5.1-compatible packaging checks, stable-tag validation, and tracked
root-`LICENSE` publication guards.

Local x64 Release build and CTest pass 6/6. The complete packaging suite and a seven-scenario real
rollback matrix pass under both PowerShell 7.6.4 and Windows PowerShell 5.1. The matrix covers all
three startup mechanisms, fresh failure cleanup, exact update restoration, preservation of unknown
files, successful update/uninstall, Task Scheduler folder ownership, and hard-link rejection. Hosted
exact-head CI and the final independent review remain to be reconciled before merge. The PR is
intended to close [#4](https://github.com/Chris0Jeky/IdleHarbor/issues/4),
[#5](https://github.com/Chris0Jeky/IdleHarbor/issues/5), and
[#9](https://github.com/Chris0Jeky/IdleHarbor/issues/9).

### High-DPI viewport

The current branch, `agent/v0.1.0-ui-viewport`, now incorporates `origin/main`. Implementation
commit `be6b42d77e73cac563e7bce76abe4a14fedbd4a4` adds canonical DPI-scaled geometry, work-area
clamping, resize layout, vertical scrolling, focus reveal, and pure layout/scroll tests. Its saved
local x64 Release run passed CTest 7/7.

Before issue [#2](https://github.com/Chris0Jeky/IdleHarbor/issues/2) is merge-ready, finish
child-targeted and high-resolution wheel routing without swallowing combo-popup input, then run a
fresh native build/test and genuine desktop QA for resize, scrollbar thumb, wheel, focus reveal,
keyboard traversal, start/stop, and tray behavior. Mixed-DPI runtime behavior must be tested where
the available display setup permits it or reported as not verified.

### Portfolio and publication

Issue [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6) tracks genuine Running, Paused, and
tray screenshots plus the repository social-preview polish. The current preview asset is 1280x640
but needs a contrast pass. Repository description/topics, README presentation, and distribution
instructions must be audited against the final shipped behavior.

`HUMAN_TODO.md` remains authoritative:

- q-1: explicit open-source licence approval is unresolved. Do not add a `LICENSE`, infer a
  copyright holder, create a tag, publish a release, or submit package manifests until the user
  supplies that decision.
- q-2: Authenticode signing is optional. Without a signing identity, document `0.1.0` as unsigned
  and rely on checksums, SBOMs, and GitHub provenance attestations.

After q-1 is answered, land the exact approved licence and SBOM metadata through a focused reviewed
change. Only then may the release workflow create `v0.1.0`; verify every published archive,
checksum, SBOM, attestation, and clean installation path before generating WinGet or Scoop manifests
from real URLs and hashes.

## Resume order

1. Close PR #10 only after exact-head hosted checks, independent review, review-thread triage, and
   the three-minute head-aging floor pass; then perform one late-review sweep.
2. Finish and ship the viewport slice for #2 with native and real desktop evidence.
3. Complete #6 using genuine application captures and polish the GitHub presentation.
4. Resolve q-1 and q-2 with the user, then run the final tag/release/install/distribution audit.

## Proving commands

On this machine use Visual Studio Build Tools 2019:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 16 2019" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\packaging\Test-Packaging.ps1
```

CI additionally builds ARM64 and Win32. Hosted ARM64 evidence proves build/test, not representative
ARM64 runtime behavior. `powercfg /requests` also remains not verified because it requires elevation
on this machine.
