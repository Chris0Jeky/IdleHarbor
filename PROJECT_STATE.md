# Project state

Last updated: 2026-08-24

## Current milestone

IdleHarbor `0.2.0` is the current published stable release. The immutable annotated tag
[`v0.2.0`](https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v0.2.0) resolves to merge commit
`2f0fe875f84023c049e82ea6cc963cf7ca938c71`. Release workflow
[run 32719629776](https://github.com/Chris0Jeky/IdleHarbor/actions/runs/32719629776) published x64
and ARM64 portable ZIPs, per-architecture SPDX 2.3 SBOMs, `SHA256SUMS.txt`, and GitHub/Sigstore
provenance attestations. All four public assets were downloaded and independently verified against
their release digests; `gh attestation verify` succeeds for both archives and binds to
`refs/tags/v0.2.0` at that source digest. The released x64 executable is 544,256 bytes, SHA-256
`bbada75845a1832aa5907e81003ebfd52761cbc7cac1c234d61263be7baf5e5a`, reports `FileVersion 0.2.0`, and
is `NotSigned` by intent.

`0.2.0` is the settings-window release. It adds a printed explanation under every field and a hover
description on all 29 interactive controls, applies a combo selection immediately and on the first
click, renames four labels, and regroups the maximum session duration under Safeguards. Its
`packaging/chocolatey` package and `packaging/winget/0.2.0` manifests are repointed at the published
archives and verified against them.

IdleHarbor `0.1.0` was the first stable release. It is a native C++20/Win32 Windows
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
`%LOCALAPPDATA%\Programs\IdleHarbor\IdleHarbor.exe`. It was upgraded from `0.1.0` to `0.2.0` on
2026-08-24 with the released `install.ps1` run as `-Startup TaskScheduler -StartMenu Create
-NoLaunch`. Its SHA-256 is
`bbada75845a1832aa5907e81003ebfd52761cbc7cac1c234d61263be7baf5e5a`, matching the released archive
and the x64 SPDX SBOM. `%LOCALAPPDATA%\IdleHarbor\settings.ini` is byte-for-byte unchanged across
the upgrade, the explicit Task Scheduler action remains `--start --minimized`, and the Start Menu
launcher remains visible. The executable is intentionally `NotSigned`.

## Repaint fix and final visual evidence

Issue [#34](https://github.com/Chris0Jeky/IdleHarbor/issues/34) recorded disabled controls leaving
stale fragments after viewport movement at 192 DPI, while resizing temporarily cleared the display.
The fix explicitly repaints the settings viewport and descendants after scrolling/state changes and
repaints the parent after layout convergence. The native smoke drives real wheel input and proves
natural and explicit-reference repaint captures are pixel-identical.

The stopped, running, safety-paused, scrolled, and tray-menu screenshots were recaptured from the
installed public `0.2.0` release at 192 DPI and visually inspected. `docs/assets/capture-manifest.json`
records source revision `b69c7b56ba5cd89419f849b25312b0b4cb92a5d4`, the released executable's
identity and `NotSigned` state, exact PNG dimensions, and per-image hashes. The original reported
corruption is absent in every state.

## Settings-window work after `v0.1.0`

Every pull request named in this section has merged.
PR [#54](https://github.com/Chris0Jeky/IdleHarbor/pull/54) addressed reported clunkiness when
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

`docs/assets` records the `v0.2.0` release captures, taken on 2026-08-24 from the installed public
release at 192 DPI and visually inspected. Any settings-window change made after this point -- a
layout change or a renamed label alike -- leaves those screenshots stale until the next release
capture, which must be taken from a released executable.

Follow-ups [#55](https://github.com/Chris0Jeky/IdleHarbor/issues/55) and
[#56](https://github.com/Chris0Jeky/IdleHarbor/issues/56) were opened from PR #54's review and remain
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

Reviews of these three PRs found these statements misdescribing actual behaviour, each then checked
against `src/core/core.cpp` or `src/app/main.cpp` and corrected:

- a profile replaces every setting below it (it replaces `settings_.session` only);
- Zen "moves least" (it emits virtual input intended not to move the pointer at all);
- Circle is less visible than Linear (Circle's path is roughly twice the travel);
- descriptions are unavailable during a session (the labels, status card, and Stop stay enabled);
- motion Off with keep awake None is a supported pair (`core::validate` refuses it);
- a pulse refreshes the keep-awake request (that request is continuous; `ApplyPower` runs on a mode
  change);
- the low-battery threshold pauses *below* the value (the comparison is `<=`) and does so regardless
  of power source (it also requires `on_battery`);
- closing always hides to the notification area (with the icon unavailable it exits and stops the
  session);
- the queued viewport repaint waits for the list to close (the list is a separate popup;
  `RDW_ALLCHILDREN` never reaches it).

No automated check can catch that class of error; only reading the text against the source does.

## Distribution status

- **GitHub Release:** published and verified at
  [`v0.2.0`](https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v0.2.0), with
  [`v0.1.0`](https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v0.1.0) retained.
- **WinGet:** the `0.1.0` manifests were submitted as
  [microsoft/winget-pkgs#421663](https://github.com/microsoft/winget-pkgs/pull/421663); every
  upstream validation check passed and it is waiting on a community moderator. `0.2.0` manifests are
  prepared in `packaging/winget/0.2.0` and pass `winget validate` without warnings, but are
  deliberately unsubmitted: `#421663` is still a `New-Package` request, so the package does not exist
  upstream and a version update has nothing to target. Both versions are mirrored in
  `packaging/winget` so a submission can be reproduced from this repository. A full local WinGet
  lifecycle was not run because `LocalManifestFiles` is an administrator setting on this PC;
  upstream validation and moderation remain authoritative.
- **Chocolatey:** reproducible x64-only package source is tracked in `packaging/chocolatey` and
  targets the published `v0.2.0` archive. `Test-ChocolateyPackage.ps1 -VerifyPublishedChecksum`
  downloaded that archive and confirmed the pinned digest against it. `choco pack` produces
  `idleharbor.0.2.0.nupkg`; the generated package contains the complete GPL text and verification
  instructions, and `chocolateyBeforeModify.ps1` closes a running instance before an upgrade, which
  `chocolateyUninstall.ps1` is not run for. Community publication still needs an owner Chocolatey
  account/API key (`HUMAN_TODO.md` q-3) and an isolated install/upgrade/uninstall test
  ([#51](https://github.com/Chris0Jeky/IdleHarbor/issues/51)).
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
6. Continue the small tracked runtime/capture follow-ups without expanding the released safety boundary.

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
.\packaging\Test-ReleaseVersion.ps1 -Tag v0.2.0
choco pack .\packaging\chocolatey\idleharbor.nuspec --output-directory .\out\chocolatey
```

Run packaging and release-workflow checks sequentially under PowerShell 7 and Windows PowerShell 5.1.
CI additionally builds ARM64 and Win32. Hosted ARM64 evidence proves cross-build packaging, not
representative ARM64 runtime behavior. A true cross-monitor mixed-DPI transition remains unverified
because only one display is attached. `powercfg /requests` also requires an elevated verification
run on this machine.
