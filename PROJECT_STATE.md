# Project state

Last updated: 2026-08-20

## Current milestone

IdleHarbor `0.1.0` is a native Windows release candidate with a platform-neutral policy core,
validated INI settings, strict CLI, bounded motion, genuine-input observation, power requests,
battery/fullscreen/session safeguards, visible tray controls, an emergency stop path, recoverable
settings handling, and transactional per-user installation. No release or tag has been published.

`origin/main` is `71093ac16c20f5859f57e7ca54fb69fbf217199b`. Pull requests
[#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7),
[#8](https://github.com/Chris0Jeky/IdleHarbor/pull/8), and
[#10](https://github.com/Chris0Jeky/IdleHarbor/pull/10) landed pinned action updates,
settings-recovery safeguards, and installer/release-trust hardening. Each passed exact-head hosted
checks, the required review gate, and the three-minute aging floor. Issues #3, #4, #5, and #9 are
closed.

PR #10's local evidence included x64 CTest 6/6 plus complete packaging and real seven-scenario
rollback matrices under both PowerShell 7.6.4 and Windows PowerShell 5.1. The matrix covered every
startup mechanism, fresh failure cleanup, exact update restoration, preservation of unknown files,
Task Scheduler folder ownership, hard-link rejection, successful update, and uninstall. Issue
[#11](https://github.com/Chris0Jeky/IdleHarbor/issues/11) is complete: installer rollback recovery
material is retained only when managed-file restoration is incomplete, and first-time same-source
installs require a valid ownership marker. Follow-up edge hardening remains tracked in
[#12](https://github.com/Chris0Jeky/IdleHarbor/issues/12).

## Active integration queue

### High-DPI viewport

The merged baseline is `origin/main` at `7b1bcf3`. Ready-for-review PR
[#18](https://github.com/Chris0Jeky/IdleHarbor/pull/18), on branch
`agent/v0.1.0-viewport-safety`, remains open and unmerged. It carries the viewport hardening
slice for issues #14 and #15, including:

- `be6b42d`: canonical DPI-scaled geometry, work-area clamping, resize layout, vertical scrolling,
  focus reveal, and pure layout/scroll tests;
- `c6eaa53`: child-targeted wheel routing, native combo-popup preservation, system wheel-setting
  support, and retained high-resolution wheel deltas.
- Fixed safety regions keep live status and immediate Start/Stop visible while the settings body
  scrolls; narrow widths reflow controls and action buttons.
- Focus reveal is gated on actual focus changes, and fractional-DPI width decisions use exact
  physical/logical conversion at 120%, 150%, and 175% scaling.

The current local x64 Release build passes CTest 7/7, including deterministic safety-region,
focus-gating, fractional-DPI, and short-client Stop-bound tests. A real 200%-DPI desktop smoke at
the earlier PR head resized the window, exercised three partial wheel messages over a child control,
preserved native wheel handling for an open combo box, used native line/page scrolling, preserved an
in-range scroll position across resize, revealed an off-screen control by direct focus and
forward/reverse keyboard traversal, activated Save with Space, started/stopped a session, and
hid/restored the window through the notification area. The fractional-DPI and short-client follow-up
has not had a new desktop smoke. Only one display is attached, so a true cross-monitor mixed-DPI
transition is not locally verifiable and must remain explicit.

Before issue [#2](https://github.com/Chris0Jeky/IdleHarbor/issues/2) is closed, update user-facing
docs and genuine screenshots, run a final exact-head native/desktop proof, obtain hosted CI and an
independent review, and merge after normal aging.

### Portfolio and publication

Issue [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6) tracks genuine Running, Paused, and
tray captures plus the repository social-preview polish. The current preview asset is 1280x640 but
needs a contrast pass. Repository description/topics, README presentation, and distribution
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

1. Finish and ship the viewport slice for #2 with native, hosted, review, and real desktop evidence.
2. Complete #6 using genuine application captures and polish the GitHub presentation.
3. Resolve the bounded [#12](https://github.com/Chris0Jeky/IdleHarbor/issues/12) release-hardening follow-up.
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
