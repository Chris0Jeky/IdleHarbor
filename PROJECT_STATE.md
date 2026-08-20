# Project state

Last updated: 2026-08-20

## Current milestone

IdleHarbor `0.1.0` is a native Windows release candidate with a platform-neutral policy core,
validated INI settings, strict CLI, bounded motion, genuine-input observation, power requests,
battery/fullscreen/session safeguards, visible tray controls, and an emergency stop path.

The distribution lane includes portable archives, per-user install/uninstall scripts, explicit
Task Scheduler/Startup-folder/HKCU Run choices, ownership checks, checksums, SPDX SBOMs, pinned CI,
CodeQL, and GitHub attestations. No release or tag has been published.

## Landed base

Pull request [#1](https://github.com/Chris0Jeky/IdleHarbor/pull/1) merged into `main` at
`986cb011ea645f73e0d806f623c15e454bd740c1`. Its exact head passed x64, ARM64, x86, CodeQL
analysis, and CodeQL scanning. All 11 review threads were resolved, and the post-merge review sweep
found no untriaged late feedback.

Real Windows smokes on that base covered foreign startup-entry collision rejection, deferred
forwarded commands, visible Running/Stopped lifecycle control, keyboard traversal, and clean exit.
`powercfg /requests` remains unverified because it requires an elevated shell on this machine.

## Active integration queue

### Settings recovery — pull request #8

Branch `agent/v0.1.0-settings-safety` preserves and displays every recovery warning, keeps
automatic start/toggle stopped until a successful Save, and retains explicit user control.

Local x64 Release build and CTest pass 6/6. A real Windows UI Automation smoke proved:

- all recovery warnings were visible;
- initial and forwarded automatic starts/toggle stayed stopped and restored the visible window;
- explicit Start reached Running, and a redundant blocked start preserved that truthful status;
- Save cleared the gate, a safe power-only start ran, Stop/Exit completed, and a healthy relaunch
  showed `Stopped: ready`.

The first independent review found and fixed a healthy-launch empty-status regression in `16e016b`.
Exact-head connector review found and fixed the active-session status mismatch in `a7ed48e`.
Issue [#3](https://github.com/Chris0Jeky/IdleHarbor/issues/3) is linked for closure on merge.

### Installer and release trust

Remote branch `agent/v0.1.0-installer-trust` at
`a8d84c5facda76fb265df402fba59f2aeacd4515` contains transactional fresh-install rollback,
manifest-version validation, a tracked-root-`LICENSE` publication guard, and the external
trust-asset contract for checksums/SBOMs.

Native x64 Release build and CTest pass 6/6. The PowerShell 7 packaging suite passes. Real installs
under both PowerShell 7 and Windows PowerShell 5 proved injected rollback, exact-boundary sentinel
preservation, executable hash equality, ownership-marker validation, installed executable
launch/exit, and clean uninstall. The broader packaging suite's ZIP entry-name assertions fail
under Windows PowerShell 5 but pass under the release workflow's PowerShell 7; this compatibility
gap is not a release artifact failure and is tracked in
[#9](https://github.com/Chris0Jeky/IdleHarbor/issues/9).

### High-DPI viewport

Remote branch `agent/v0.1.0-ui-viewport` includes implementation commit `be6b42d` plus the prior
stop-safe handoff. It adds canonical DPI geometry, work-area clamping, resizing, scrolling, focus
reveal, and pure tests. Local CTest passed 7/7. Child-targeted/high-resolution wheel routing and
real resize/focus/mixed-DPI desktop QA remain before issue
[#2](https://github.com/Chris0Jeky/IdleHarbor/issues/2) is merge-ready.

The viewport and settings branches both change `src/app/main.cpp`. Preserve both histories and
merge the newly landed `main` into the viewport branch before resolving that overlap.

## GitHub and publication gates

- Dependabot pull request [#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7) is green but has not
  yet been reviewed.
- Milestone issues [#2](https://github.com/Chris0Jeky/IdleHarbor/issues/2) through
  [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6) remain the v0.1.0 closeout queue.
- q-1 in `HUMAN_TODO.md` remains a hard release gate. Do not add a licence, tag, or release
  without explicit approval. q-2 signing is optional and may be documented as unsigned.
- The 1280x640 social-preview asset is committed, but browser automation could not attach it in
  repository settings. No setting was changed.

## Next safe slices

1. Finish exact-head CI/review aging for PR #8, merge with a merge commit, and perform one late-review
   sweep.
2. Reconcile the installer branch with the new main, finish its independent review, open the #4/#5
   PR, and ship only after exact-head gates.
3. Merge the new main into the viewport branch, finish wheel routing and real DPI/resize QA, update
   docs/screenshots, and ship #2.
4. Complete #6 portfolio visuals and social preview, then resolve q-1/q-2 and run the final
   tag/release/install/distribution audit.

## Proving commands

```powershell
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\packaging\Test-Packaging.ps1
```

This machine has Visual Studio Build Tools 2019 rather than Visual Studio 2022, so local proving
uses generator `Visual Studio 16 2019`. CI additionally builds ARM64 and Win32. Hosted ARM64
evidence proves build/test, not representative ARM64 runtime behavior.
