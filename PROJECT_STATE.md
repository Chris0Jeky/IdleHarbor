# Project state

Last updated: 2026-08-23

## Current milestone

IdleHarbor `0.1.0` is the published first stable release. It is a native C++20/Win32 Windows
mouse-jiggler and keep-awake utility with a platform-neutral policy core, validated local settings,
strict CLI, bounded motion patterns, Windows power requests, genuine-input observation,
battery/fullscreen/session safeguards, visible notification-area controls, an emergency stop, and
transactional per-user installation. It has no telemetry, network service, managed runtime, hidden
mode, or implicit persistence.

The immutable annotated tag
[`v0.1.0`](https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v0.1.0) resolves to merge commit
`d936f1e3d147b98403272e27ce1b3ec8f1cee3eb`. Release workflow
[run 32423221779](https://github.com/Chris0Jeky/IdleHarbor/actions/runs/32423221779) published x64 and
ARM64 portable ZIPs, per-architecture SPDX 2.3 SBOMs, `SHA256SUMS.txt`, and GitHub/Sigstore
provenance attestations. All five public assets were downloaded and independently verified against
their release digests, SBOM contents, tagged source revision, and attestations.

PR [#47](https://github.com/Chris0Jeky/IdleHarbor/pull/47) merged the release UI, exact-build capture
tooling, PowerShell 5.1 release checks, documentation, GPLv3 distribution, and final review fixes.
PR [#31](https://github.com/Chris0Jeky/IdleHarbor/pull/31) was closed as superseded.

## Installed release on the development PC

The verified public x64 build is installed per-user at
`%LOCALAPPDATA%\Programs\IdleHarbor\IdleHarbor.exe`. Its SHA-256 is
`e28bf9d739c9c7d7a20f66f41cf1c6015053d9b3277a5686ea3b5e8219d552e9`, matching the x64 SPDX
SBOM. Existing settings were preserved byte-for-byte. The explicit Task Scheduler action remains
`--start --minimized`, the Start Menu launcher remains visible, and exactly one released process
was restored after final screenshot QA. The executable is intentionally `NotSigned`.

## Repaint fix and final visual evidence

Issue [#34](https://github.com/Chris0Jeky/IdleHarbor/issues/34) recorded disabled controls leaving
stale fragments after viewport movement at 192 DPI, while resizing temporarily cleared the display.
The fix explicitly repaints the settings viewport and descendants after scrolling/state changes and
repaints the parent after layout convergence. The native smoke drives real wheel input and proves
natural and explicit-reference repaint captures are pixel-identical.

The stopped, running, safety-paused, scrolled, and tray-menu screenshots were recaptured from the
installed public release at 192 DPI and visually inspected. `docs/assets/capture-manifest.json`
records release source `d936f1e3d147b98403272e27ce1b3ec8f1cee3eb`, the released executable
hash, exact PNG dimensions, and hashes. The original reported corruption is absent in every state.

## In-flight work after `v0.1.0`

Settings-window interaction fixes are on `agent/ux-combo-interaction`
(PR [#54](https://github.com/Chris0Jeky/IdleHarbor/pull/54)), addressing reported clunkiness when
choosing a profile, motion mode, or power request:

- focus reveal is classified by keyboard versus pointer input, so clicking a combo box no longer
  scrolls the body out from under the pointer; a suppressed reveal is not consumed;
- a combo selection made with the pointer is applied on `CBN_CLOSEUP` rather than into a control that
  is still mid-gesture, and a profile already in effect is never reloaded over edited settings;
- while a list is open the body does not scroll and its repaints are queued rather than forced;
  resize and DPI transactions close the list instead;
- body controls are repositioned in one `BeginDeferWindowPos` batch (fixed header and footer controls
  have a different parent and stay direct).

Two pre-existing intermittent failures in `tests\Test-NativeViewportRepaint.ps1` were fixed with it:
`RefreshControls` published the Save action ahead of the status card, and the test asserted on a
window that was findable from `WM_CREATE` but not yet initialized. The script now passed five
consecutive runs at 192 DPI with an identical capture hash.

`docs/assets` still records the `v0.1.0` release captures. Any settings-window change made after this
point -- a layout change or a renamed label alike -- leaves those screenshots stale until the next
release capture, which must be taken from a released executable. The captures currently show the
former "Power request" and "Motion multiplier (1-120)" labels.

PR [#54](https://github.com/Chris0Jeky/IdleHarbor/pull/54) merged those fixes. Follow-ups
[#55](https://github.com/Chris0Jeky/IdleHarbor/issues/55) and
[#56](https://github.com/Chris0Jeky/IdleHarbor/issues/56) were opened from its review and remain
open.

PR [#57](https://github.com/Chris0Jeky/IdleHarbor/pull/57) added an on-hover description for all 29
interactive controls and renamed four labels ("Keep awake", "Motion size", "Pause after real input",
and the low-battery threshold's unit). Its review found seven descriptions that misstated actual
behaviour; each was checked against the source and corrected. Follow-ups
[#58](https://github.com/Chris0Jeky/IdleHarbor/issues/58) and
[#59](https://github.com/Chris0Jeky/IdleHarbor/issues/59) came out of it.

PR [#60](https://github.com/Chris0Jeky/IdleHarbor/pull/60) added a printed explanation under each of
the eight fields whose label is a noun phrase, moved the body from hard-coded control offsets to a
running layout cursor, and regrouped the maximum session duration under Safeguards (it is a session
setting a profile replaces, unlike everything else that was under "Window & notifications"). It
closed [#58](https://github.com/Chris0Jeky/IdleHarbor/issues/58).

`tests\Test-ControlHelpTips.ps1` proves both surfaces: 29 registered hover descriptions, and 8
printed explanations each tall enough for its own wrapped text. That last assertion was earned --
the first version of it passed against a build with hint heights forced to a single line, because
clipping does not overlap anything. It now measures the required height itself and fails on that
build.

## Settings-window UX state

The three merged PRs above leave the settings window with: pointer-driven focus that does not scroll
the body, combo selections applied after the list closes, a hover description on all 29 interactive
controls, and a printed explanation under each of the eight noun-phrase fields. Four labels were
renamed ("Keep awake", "Motion size", "Pause after real input", and the low-battery threshold's unit),
and the corresponding text in `--help`, `README.md`, the generated INI comment, and `core::validate`'s
out-of-range message was aligned.

Reviews of these three PRs found ten statements that misdescribed actual behaviour -- profile scope,
Zen's motion, Circle versus Linear visibility, the pulse/keep-awake relationship, the low-battery
comparison and its on-battery precondition, and the close-to-tray fallback. Each was checked against
`src/core/core.cpp` or `src/app/main.cpp` and corrected. No automated check can catch that class of
error; only reading the text against the source does.

## Distribution status

- **GitHub Release:** published and verified at
  [`v0.1.0`](https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v0.1.0).
- **WinGet:** three schema-1.12 manifests were independently reviewed and submitted as
  [microsoft/winget-pkgs#421663](https://github.com/microsoft/winget-pkgs/pull/421663). Local
  `winget validate` passes without warnings. A full local WinGet lifecycle was not run because
  `LocalManifestFiles` is an administrator setting on this PC; upstream validation and moderation
  remain authoritative.
- **Chocolatey:** reproducible x64 package source is tracked in `packaging/chocolatey`.
  `choco pack` succeeds; the generated package contains the complete GPL text and verification
  instructions, and its no-op plan selects the correct script. Community publication still needs
  an owner Chocolatey account/API key and an appropriate isolated install/uninstall test.
- **Scoop Extras:** no request was submitted. Its current required request criteria ask for
  popularity evidence (for example, 100 stars or 50 forks), which this new project cannot truthfully
  claim yet.
- **GitHub social preview:** the 1280 x 640 PNG is ready and issue
  [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6) records the remaining upload. GitHub accepts
  the file selection, then its own Settings JavaScript throws in
  `attachmentUploadDidComplete`; repeated attempts leave the preview blank.

## Follow-up queue

Issues [#43](https://github.com/Chris0Jeky/IdleHarbor/issues/43),
[#44](https://github.com/Chris0Jeky/IdleHarbor/issues/44), and
[#45](https://github.com/Chris0Jeky/IdleHarbor/issues/45) closed with PR #47. The bounded post-release
queue remains:

- [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6): upload the prepared social preview after GitHub's Settings UI recovers;
- [#37](https://github.com/Chris0Jeky/IdleHarbor/issues/37): handle Start Menu known-folder relocation without weakening ownership checks;
- [#38](https://github.com/Chris0Jeky/IdleHarbor/issues/38): make installer and uninstaller `-WhatIf` summaries unambiguously preview-only;
- [#40](https://github.com/Chris0Jeky/IdleHarbor/issues/40): pin a complete canonical GPLv3 text check in the publication gate;
- [#42](https://github.com/Chris0Jeky/IdleHarbor/issues/42): deterministic tray-failure dirty-state test;
- [#46](https://github.com/Chris0Jeky/IdleHarbor/issues/46): tray recovery/status refresh follow-up;
- [#48](https://github.com/Chris0Jeky/IdleHarbor/issues/48): restore the scheduler command after packaging tests;
- [#49](https://github.com/Chris0Jeky/IdleHarbor/issues/49): make documentation capture more portable;
- [#50](https://github.com/Chris0Jeky/IdleHarbor/issues/50): sanitize rounded screenshot corners;
- [#51](https://github.com/Chris0Jeky/IdleHarbor/issues/51): complete Chocolatey architecture and isolated lifecycle validation;
- [#55](https://github.com/Chris0Jeky/IdleHarbor/issues/55): classify focus changes that bypass the message loop;
- [#56](https://github.com/Chris0Jeky/IdleHarbor/issues/56): a forwarded command can discard a pending combo selection;
- [#59](https://github.com/Chris0Jeky/IdleHarbor/issues/59): factor the shared setup and teardown out of the native test scripts.

## Human decisions

`HUMAN_TODO.md` is authoritative:

- q-1: resolved 2026-08-20 as `GPL-3.0-only`; no copyright holder was inferred.
- q-2: resolved 2026-08-20 as an explicitly unsigned `v0.1.0`, relying on checksums, SPDX SBOMs,
  and GitHub provenance attestations. Signing can be reconsidered for a later release.
- q-3: open for the owner-only Chocolatey account/API key needed to submit the prepared package;
  the credential must remain outside the repository and public logs.

## Resume order

1. Monitor WinGet PR #421663 and complete the Microsoft CLA if its status check requests owner action.
2. Retry issue #6's social-preview upload after GitHub changes or fixes the Settings UI.
3. Run the Chocolatey install/uninstall test in a suitable environment and publish with the owner's
   Chocolatey API key.
4. Revisit Scoop Extras only after its popularity/repute criterion can be met honestly.
5. Recapture `docs/assets` at the next release: every screenshot predates the settings-window label
   renames and the printed explanations.
5. Continue the small tracked runtime/capture follow-ups without expanding the released safety boundary.

## Proving commands

On this machine, Visual Studio Build Tools 2019 is installed:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 16 2019" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\tests\Test-NativeViewportRepaint.ps1 -Executable .\build\x64\Release\IdleHarbor.exe
.\tests\Test-ControlHelpTips.ps1 -Executable .\build\x64\Release\IdleHarbor.exe
.\packaging\Test-ReleaseWorkflow.ps1
.\packaging\Test-Packaging.ps1
.\packaging\Test-ReleaseVersion.ps1 -Tag v0.1.0
choco pack .\packaging\chocolatey\idleharbor.nuspec --output-directory .\out\chocolatey
```

Run packaging and release-workflow checks sequentially under PowerShell 7 and Windows PowerShell 5.1.
CI additionally builds ARM64 and Win32. Hosted ARM64 evidence proves cross-build packaging, not
representative ARM64 runtime behavior. A true cross-monitor mixed-DPI transition remains unverified
because only one display is attached. `powercfg /requests` also requires an elevated verification
run on this machine.
