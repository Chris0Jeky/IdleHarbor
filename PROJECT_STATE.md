# Project state

Last updated: 2026-08-20

## Current milestone

IdleHarbor `0.1.0` is a release candidate with a landed native Win32 runtime, platform-neutral
policy core, validated INI settings store, strict command-line model, bounded motion emitter,
genuine-input observer, power request, battery/fullscreen/session safeguards, visible
tray-controlled application, and emergency stop path.

The distribution lane has also landed portable archive generation, per-user installation and
uninstallation scripts, ownership checks, opt-in Task Scheduler/Startup-folder/HKCU Run startup
choices, checksums, SPDX SBOM generation, pinned CI, CodeQL, and GitHub attestation workflows.

No release has been published. Local release QA and measured performance evidence are recorded.
Pull request [#1](https://github.com/Chris0Jeky/IdleHarbor/pull/1) remains open on
`agent/v0.1.0-native-release`; human licence approval and optional Authenticode signing remain open.

## 2026-08-20 session checkpoint

The single bounded review-fix round is integrated as seven focused commits after `21ea6e4`. It
addresses all eight concrete connector findings: current battery/session state now fails closed,
forwarded commands are deferred outside synchronous `WM_COPYDATA`, installer startup ownership is
checked before mutation, the data marker precedes settings data, release tags must match every
embedded version, SPDX output includes the required SHA-1/package verification code, and the README
INI example is directly copyable.

Fresh local evidence at this checkpoint:

- x64 Release built successfully and all 6 CTest tests passed.
- x86 Release built successfully and all 6 CTest tests passed.
- `packaging/Test-Packaging.ps1` passed, including matching/mismatched release tags and SPDX checks.
- `git diff --check` passed before this state update.

The review-fix checkpoint is pushed to `origin/agent/v0.1.0-native-release`. The previous PR head
`21ea6e4` was green in hosted x64, ARM64, x86, CodeQL analysis, and CodeQL scanning; hosted CI has
not yet proved the new head. Review threads have not yet been replied to or resolved; preserve them
until the corresponding commit and verification evidence are posted.

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

## Next safe slice

1. Wait for exact-head x64, ARM64, x86, CodeQL analysis, and CodeQL scanning results on
   `agent/v0.1.0-native-release`.
2. Exercise the deferred forwarded-command path and installer collision preflight on a real Windows
   desktop, then reply to and resolve each of the eight review threads with commit/check evidence.
3. Recheck the PR diff, zero open code-scanning alerts, mergeability, and the three-minute head-age
   floor. Merge PR #1 with a merge commit only when every required check is green.
4. At the next workflow checkpoint, inspect PR #1 once for late review feedback and triage it once.
5. Do not create `v0.1.0` or publish a release until q-1 in `HUMAN_TODO.md` is explicitly answered.
   If MIT is approved, add the licence and update SBOM licence fields in a focused reviewed change;
   q-2 may remain explicitly unsigned for v0.1.0.
6. Before claiming release-quality runtime coverage, still exercise lock/unlock, remote-session
   disconnect/reconnect, Explorer restart, mixed-DPI monitor movement, and power-request cleanup on
   representative Windows hardware. Hosted ARM64 currently proves build/test only, not runtime.
7. Upload `docs/assets/idleharbor-social.png` under GitHub Settings > General > Social preview. The
   signed-in browser reached the upload control, but its file chooser denied automated attachment;
   the finished 1280x640 asset is committed and no repository setting was changed.
8. After a real release exists, add WinGet/Scoop distribution manifests only from the published
   asset URLs and verified checksums; do not invent pre-release download locations or hashes.
