# Project state

Last updated: 2026-08-20

## Current milestone

IdleHarbor `0.1.0` is a native Windows release candidate with a platform-neutral policy core,
validated INI settings, strict CLI, bounded motion, genuine-input observation, power requests,
battery/fullscreen/session safeguards, visible tray controls, an emergency stop path, recoverable
settings handling, and transactional per-user installation. No licence, tag, or release has been
created or published.

The merged baseline is `origin/main` at
`974d8406d14fead3b7ffbdd4ed88534473f16c97`. Pull requests [#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7),
[#8](https://github.com/Chris0Jeky/IdleHarbor/pull/8),
[#10](https://github.com/Chris0Jeky/IdleHarbor/pull/10),
[#13](https://github.com/Chris0Jeky/IdleHarbor/pull/13),
[#17](https://github.com/Chris0Jeky/IdleHarbor/pull/17), and
[#18](https://github.com/Chris0Jeky/IdleHarbor/pull/18) have landed. The landed work includes
installer rollback recovery (#11), high-DPI viewport and safety hardening (#14 and #15),
settings-recovery safeguards, and installer/release-trust hardening. Issues #2, #3, #4, #5, #9,
#11, #14, and #15 are closed. Confirm the live base and hosted checks before any merge or release
decision.

PR #10's local evidence included x64 CTest 6/6 plus complete packaging and real seven-scenario
rollback matrices under both PowerShell 7.6.4 and Windows PowerShell 5.1. The matrix covered every
startup mechanism, fresh failure cleanup, exact update restoration, preservation of unknown files,
Task Scheduler folder ownership, hard-link rejection, successful update, and uninstall.

The current branch, `agent/v0.1.0-release-tag-data`, carries PR
[#16](https://github.com/Chris0Jeky/IdleHarbor/pull/16), which implements issue
[#12](https://github.com/Chris0Jeky/IdleHarbor/issues/12) and is pending merge. The release workflow
now passes the triggering tag through step-level `RELEASE_TAG` data; the resolve and publish paths
consume `$env:RELEASE_TAG`; `packaging/Test-ReleaseWorkflow.ps1` proves that contract under
Windows PowerShell 5.1-compatible syntax; and packaging docs/CHANGELOG record the boundary. Do not
redo this slice while PR #16 is open, and do not create a tag or release as part of its review or
merge. Issue [#11](https://github.com/Chris0Jeky/IdleHarbor/issues/11) is complete: installer
rollback recovery material is retained only when managed-file restoration is incomplete, and
first-time same-source installs require a valid ownership marker. Issue [#12](https://github.com/Chris0Jeky/IdleHarbor/issues/12)
remains open pending PR #16.

## Active integration queue

### High-DPI viewport

The merged viewport work in PRs #13 and #18 closed issues #14 and #15. It includes:

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

The remaining UI follow-ups are [#19](https://github.com/Chris0Jeky/IdleHarbor/issues/19),
keyboard tab order; [#20](https://github.com/Chris0Jeky/IdleHarbor/issues/20), scrollbar-track
confinement; [#21](https://github.com/Chris0Jeky/IdleHarbor/issues/21), focus reveal after layout
changes; and [#22](https://github.com/Chris0Jeky/IdleHarbor/issues/22), stacked settings below
100 logical pixels. All four remain open. Do not reopen the landed #14/#15 work unless new evidence
points to a regression.

### Portfolio and publication

Issue [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6) remains open and tracks genuine Running, Paused, and
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

1. Complete the scoped review/merge of PR [#16](https://github.com/Chris0Jeky/IdleHarbor/pull/16)
   for issue #12; do not create a tag or release as part of that merge.
2. Resolve the open UI follow-ups [#19](https://github.com/Chris0Jeky/IdleHarbor/issues/19),
   [#20](https://github.com/Chris0Jeky/IdleHarbor/issues/20),
   [#21](https://github.com/Chris0Jeky/IdleHarbor/issues/21), and
   [#22](https://github.com/Chris0Jeky/IdleHarbor/issues/22).
3. Complete portfolio and publication evidence in [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6).
4. Resolve q-1 and q-2 with the user, then run the final tag/release/install/distribution audit.

## Proving commands

On this machine use Visual Studio Build Tools 2019:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 16 2019" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\packaging\Test-ReleaseWorkflow.ps1
.\packaging\Test-Packaging.ps1
```

Run the packaging and release-workflow checks under both PowerShell 7 and Windows PowerShell 5.1.
CI additionally builds ARM64 and Win32. Hosted ARM64 evidence proves build/test, not representative
ARM64 runtime behavior. `powercfg /requests` also remains not verified because it requires elevation
on this machine.
