# Project state

Last updated: 2026-08-20

## Current milestone

IdleHarbor `0.1.0` is a native Windows release candidate with a platform-neutral policy core,
validated local settings, strict CLI, independently designed bounded motion patterns, Windows power
requests, genuine-input observation, battery/fullscreen/session safeguards, visible tray controls,
an emergency stop, and transactional per-user installation. It has no telemetry or network service.
The polished native interface uses themed Windows controls, a dedicated status card, visible
Start/Stop/Save actions, and a scrollable high-DPI settings body. The source and release artifacts
are licensed `GPL-3.0-only`. No tag or release has been published.

The current merged baseline is `origin/main` at
`14daadf64fd1c895b2721ccfb7006e48a29635ad`. PRs
[#30](https://github.com/Chris0Jeky/IdleHarbor/pull/30) and
[#33](https://github.com/Chris0Jeky/IdleHarbor/pull/33) completed the compact-viewport accessibility,
scroll-range convergence, and inherited-scrollbar fixes. PR
[#35](https://github.com/Chris0Jeky/IdleHarbor/pull/35) closed issue
[#34](https://github.com/Chris0Jeky/IdleHarbor/issues/34) with the descendant-repaint fix and native
192-DPI smoke. PR [#36](https://github.com/Chris0Jeky/IdleHarbor/pull/36) then landed the default,
ownership-safe Start Menu launcher. Earlier merged work established the runtime, settings recovery,
installer rollback and ownership boundaries, release workflow, multi-architecture CI, CodeQL, and
packaging isolation. PR [#41](https://github.com/Chris0Jeky/IdleHarbor/pull/41) landed the polished
native interface and whole-window resize repaint proof.

## Repaint follow-up

Issue [#34](https://github.com/Chris0Jeky/IdleHarbor/issues/34) records a 192-DPI Running-state
settings-body artifact: disabled controls can leave stale fragments after viewport movement, while
resizing temporarily clears the display. The cause seam is the manually moved child controls inside
a nested `WS_CLIPCHILDREN` viewport without a final explicit descendant repaint.

The fix repaints the settings viewport and all descendants after scrolling and state changes, then
repaints the parent plus descendants after layout convergence so newly exposed footer pixels cannot
retain clipped body controls. The native interactive smoke starts a safe motion-free session, proves
that body controls are disabled, drives real wheel input, and compares both the whole client and the
scrolled viewport with explicit repaint references at this desktop's 192 DPI. Natural and reference
captures are pixel-identical on the fixed x64 Release build. The original user-supplied screenshot
remains the pre-fix reproduction; the post-fix stopped, running, paused, scrolled, and tray states
are recorded in `docs/assets/capture-manifest.json` with exact PNG and executable hashes.

## Current release-preparation follow-up

The merged installer adds a per-user Start Menu shortcut as its default launch surface.
It opens the visible settings window with `--show`; automatic startup remains an independent,
explicit choice. The installer records ownership only for a shortcut it created, restores exact
prior bytes on rollback, and rejects unsafe or foreign shortcut leaves. `-StartMenu None` and the
uninstaller remove only an exact, marker-owned shortcut; changed or unowned shortcuts are
preserved. The lifecycle suite uses temporary injected shortcut paths, and the actual per-user Start
Menu path was exercised successfully during the local release-candidate installation.

PR [#39](https://github.com/Chris0Jeky/IdleHarbor/pull/39) removed the only direct upstream
motion-coordinate dependency, recorded the historical inspiration in `THIRD-PARTY-NOTICES.md`, and
applied `GPL-3.0-only` consistently to the repository, portable archive, package manifest, and SPDX
SBOM metadata.

The release-preparation branch restores Windows PowerShell 5.1 release checks, keeps first-owner and
forwarded CLI overrides explicitly saveable without silent INI persistence, and provides a
deterministic exact-build screenshot harness. Issues
[#43](https://github.com/Chris0Jeky/IdleHarbor/issues/43),
[#44](https://github.com/Chris0Jeky/IdleHarbor/issues/44), and
[#45](https://github.com/Chris0Jeky/IdleHarbor/issues/45) are expected to close when that branch
lands. Issue [#42](https://github.com/Chris0Jeky/IdleHarbor/issues/42) remains open for a deterministic
tray-failure transition test even though the accessible-text composition fix is implemented. Issue
[#46](https://github.com/Chris0Jeky/IdleHarbor/issues/46) records two non-blocking status refresh
follow-ups outside the `v0.1.0` release boundary.

## Portfolio and publication queue

PR [#31](https://github.com/Chris0Jeky/IdleHarbor/pull/31) contains pre-redesign portfolio text and
captures and is superseded rather than merged. The release-preparation branch replaces it with five
privacy-reviewed exact-build screenshots, a reproducible capture manifest, refreshed social-preview
artwork, and current publication metadata. Issue
[#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6) tracks the remaining publication boundary.

The installer has locally proved per-user Task Scheduler, Startup-folder, HKCU Run, and no-startup
modes; exact rollback after fresh and update failures; preservation of unexpected files and
pre-existing scheduler folders; linked-file rejection; uninstall; and bounded concurrent execution
under PowerShell 7 and Windows PowerShell 5.1. Startup remains explicit and visible.

## Human decisions

`HUMAN_TODO.md` is authoritative:

- q-1: resolved 2026-08-20 as `GPL-3.0-only`; no copyright holder was inferred.
- q-2: resolved 2026-08-20 as an explicitly unsigned `v0.1.0`, relying on checksums, SPDX SBOMs,
  and GitHub provenance attestations. Signing can be reconsidered for a later release.

## Resume order

1. Finish and merge exact-build screenshots, documentation, repository metadata, and social-preview
   artwork.
2. Run final native runtime, packaging, installer, performance, security, accessibility, and
   release-artifact QA; preserve a dated demonstration bundle.
3. Tag and publish unsigned `v0.1.0`, then derive and submit package-manager manifests from its
   immutable release URLs and hashes.

## Proving commands

On this machine, Visual Studio Build Tools 2019 is installed:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 16 2019" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\tests\Test-NativeViewportRepaint.ps1 -Executable .\build\x64\Release\IdleHarbor.exe
.\packaging\Test-ReleaseWorkflow.ps1
.\packaging\Test-Packaging.ps1
```

Use generator `Visual Studio 17 2022` where Visual Studio 2022 is installed. Run packaging and
release-workflow checks sequentially under PowerShell 7 and Windows PowerShell 5.1. CI additionally
builds ARM64 and Win32. Hosted ARM64 evidence proves build/test, not representative ARM64 runtime
behavior. A true cross-monitor mixed-DPI transition remains unverified because only one display is
attached. `powercfg /requests` also requires an elevated verification run on this machine.
