# Project state

Last updated: 2026-08-19

## Current milestone

IdleHarbor `0.1.0` is a release candidate with a landed native Win32 runtime, platform-neutral
policy core, validated INI settings store, strict command-line model, bounded motion emitter,
genuine-input observer, power request, battery/fullscreen/session safeguards, visible
tray-controlled application, and emergency stop path.

The distribution lane has also landed portable archive generation, per-user installation and
uninstallation scripts, ownership checks, opt-in Task Scheduler/Startup-folder/HKCU Run startup
choices, checksums, SPDX SBOM generation, pinned CI, CodeQL, and GitHub attestation workflows.

No release has been published. Local release QA and measured performance evidence are recorded;
hosted exact-head CI, human licence approval, and optional Authenticode signing remain open.

## Authority and publication

- Declared posture: T1 sandbox, public synthetic publication, no sensitive data.
- Public remote: `Chris0Jeky/IdleHarbor`.
- Release: none.
- Human decisions: see `HUMAN_TODO.md`; do not infer or close those decisions.

## Proving commands

From a Windows developer PowerShell:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\packaging\Test-Packaging.ps1
```

CI additionally configures ARM64 and Win32. The release workflow packages x64 and ARM64 on SemVer
tags and generates checksums, SPDX SBOMs, and GitHub attestations.

## Next slice

Complete Windows release QA: exercise the visible UI at supported DPI scales, verify input/session/
power cleanup on real Windows sessions, run packaging and artifact checks, record benchmark evidence,
and resolve the human-owned licence/signing decisions before publishing a release.
