# Project state

Last updated: 2026-08-20

## Current milestone

IdleHarbor `0.1.0` is a native Windows release candidate with a platform-neutral policy core,
validated local settings, strict CLI, upstream-compatible motion patterns, Windows power requests,
genuine-input observation, battery/fullscreen/session safeguards, visible tray controls, an
emergency stop, and transactional per-user installation. It has no telemetry or network service.
No licence, tag, or release has been created or published.

The current merged baseline is `origin/main` at
`abcaf19e21aa535085dd34a052207c26d12acca4`. PRs
[#30](https://github.com/Chris0Jeky/IdleHarbor/pull/30) and
[#33](https://github.com/Chris0Jeky/IdleHarbor/pull/33) completed the compact-viewport accessibility,
scroll-range convergence, and inherited-scrollbar fixes. PR
[#35](https://github.com/Chris0Jeky/IdleHarbor/pull/35) closed issue
[#34](https://github.com/Chris0Jeky/IdleHarbor/issues/34) with the descendant-repaint fix and native
192-DPI smoke. Earlier merged work established the runtime, settings recovery, installer rollback
and ownership boundaries, release workflow, multi-architecture CI, CodeQL, and packaging isolation.

## Repaint follow-up

Issue [#34](https://github.com/Chris0Jeky/IdleHarbor/issues/34) records a 192-DPI Running-state
settings-body artifact: disabled controls can leave stale fragments after viewport movement, while
resizing temporarily clears the display. The cause seam is the manually moved child controls inside
a nested `WS_CLIPCHILDREN` viewport without a final explicit descendant repaint.

The merged fix repaints the settings viewport and all descendants after final layout convergence,
scroll-position changes, and Running/Stopped enabled-state changes. The native interactive smoke
starts a safe motion-free session, proves that body controls are disabled, drives real wheel input,
and compares natural frames with explicit descendant-repaint references at this desktop's 192 DPI.
The smoke passes on the fixed x64 Release build. The supplied screenshot remains the reliable
pre-fix reproduction: repeated automation against the old binary did not reproduce the intermittent
artifact deterministically, so a red-old/green-new image comparison is not claimed.

## Current installer follow-up

The current branch adds a per-user Start Menu shortcut as the installer's default launch surface.
It opens the visible settings window with `--show`; automatic startup remains an independent,
explicit choice. The installer records ownership only for a shortcut it created, restores exact
prior bytes on rollback, and rejects unsafe or foreign shortcut leaves. `-StartMenu None` and the
uninstaller remove only an exact, marker-owned shortcut; changed or unowned shortcuts are
preserved. The lifecycle suite uses temporary injected shortcut paths, so the actual per-user Start
Menu path will be exercised during the final machine installation after this slice merges.

## Portfolio and publication queue

PR [#31](https://github.com/Chris0Jeky/IdleHarbor/pull/31) contains exact-build portfolio text and
privacy-reviewed UI captures. Its hosted x64/ARM64/x86 and CodeQL checks are green, and independent
content review found no blocking defect. It remains intentionally unmerged until the repository
social-preview image is uploaded and visually checked on GitHub. After runtime changes land, its
exact-build evidence must be refreshed against final `main` before merge. Issue
[#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6) tracks that publication boundary.

The installer has locally proved per-user Task Scheduler, Startup-folder, HKCU Run, and no-startup
modes; exact rollback after fresh and update failures; preservation of unexpected files and
pre-existing scheduler folders; linked-file rejection; uninstall; and bounded concurrent execution
under PowerShell 7 and Windows PowerShell 5.1. Startup remains explicit and visible.

## Human decisions

`HUMAN_TODO.md` is authoritative:

- q-1: explicit open-source licence approval is unresolved. Do not add a `LICENSE`, infer a
  copyright holder, create a tag, publish a release, or submit package manifests until the user
  supplies that decision.
- q-2: Authenticode signing is optional. Without a signing identity, document `0.1.0` as unsigned
  and rely on checksums, SBOMs, and GitHub provenance attestations.

## Resume order

1. Prove, review, and merge the ownership-safe Start Menu installer follow-up, then sweep late
   review feedback once.
2. Incorporate final `main` into PR #31, refresh its exact-build evidence, and complete the GitHub
   social-preview upload/visual check.
3. Run final native runtime, packaging, installer, performance, security, accessibility, and
   release-artifact QA; preserve a dated demonstration bundle.
4. Resolve q-1 and q-2 with the user. Only then add the approved licence, tag/publish `v0.1.0`, and
   derive package-manager manifests from verified release URLs and hashes.

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
