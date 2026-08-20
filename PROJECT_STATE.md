# Project state

Last updated: 2026-08-20

## Current milestone

IdleHarbor `0.1.0` is a native, dependency-free Windows release candidate with a testable policy
core, validated settings and CLI, bounded motion, genuine-input observation, power requests,
battery/fullscreen/session safeguards, visible tray controls, and an immediate stop path.

The distribution lane provides portable archives, transactional per-user install/update/uninstall,
explicit Task Scheduler/Startup-folder/HKCU Run choices, ownership boundaries, checksums, SPDX
SBOMs, pinned CI, CodeQL, and GitHub attestations. No release or tag has been published.

## Landed base

`origin/main` is `f9c07ef0a5e5b0b4c73bc591bc8b448bfabf3ec2`, the merge commit for pull request
[#8](https://github.com/Chris0Jeky/IdleHarbor/pull/8). Issue
[#3](https://github.com/Chris0Jeky/IdleHarbor/issues/3) is closed.

The settings-recovery change passed exact-head x64, ARM64, x86, CodeQL analysis, and CodeQL
scanning; its review thread was fixed and resolved. A real UI Automation smoke proved warning
visibility, blocking of automatic recovered starts, explicit Start, truthful Running/Stopped state,
Save recovery, healthy relaunch, and clean exit. The post-merge sweep found no late feedback.

## Installer and release trust

Branch `agent/v0.1.0-installer-trust` contains:

- transactional fresh-install and update rollback;
- restoration of managed files, the ownership marker, and owned startup state after caught failure;
- application-manifest version validation;
- a tracked-root-`LICENSE` publication guard without inventing licence approval; and
- an explicit contract keeping checksums and per-architecture SPDX files as release siblings.

Verified before merging current `main`:

- native x64 Release build and CTest passed 6/6;
- the PowerShell 7 packaging suite passed;
- the installer transaction smoke passed under PowerShell 7 and Windows PowerShell 5.1; and
- a real rollback matrix passed under both shells for RunKey, Startup Folder, and Task Scheduler,
  covering fresh failure cleanup, exact existing-startup restoration, file/marker rollback,
  unexpected-file preservation, successful update, and clean uninstall.

A fresh-context independent review of `dd65ee8` found no confirmed CRITICAL/HIGH defect. The live
matrix did expose an empty Task Scheduler folder left after uninstall/rollback; the exact empty test
folder was removed, and product cleanup still needs a bounded ownership-safe fix before the PR.

Issue [#9](https://github.com/Chris0Jeky/IdleHarbor/issues/9) remains in this release lane: normalize
ZIP separators and replace `System.IO.Path.GetRelativePath` so the complete packaging suite passes
under Windows PowerShell 5.1 as well as PowerShell 7. The workflow/validator must also consistently
reject unsupported prerelease/build tags for the stable-only v0.1.0 lane.

Intended merge issues are [#4](https://github.com/Chris0Jeky/IdleHarbor/issues/4),
[#5](https://github.com/Chris0Jeky/IdleHarbor/issues/5), and
[#9](https://github.com/Chris0Jeky/IdleHarbor/issues/9).

## Remaining project queue

- Branch `agent/v0.1.0-ui-viewport` has the high-DPI layout implementation but still needs current
  `main` integrated, child-target/high-resolution wheel routing, and real mixed-DPI UI evidence for
  [#2](https://github.com/Chris0Jeky/IdleHarbor/issues/2).
- Dependabot pull request [#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7) was rebased onto current
  `main`; exact-head CI and a fresh-base review are in progress.
- Portfolio visuals and the social preview remain under
  [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6).
- `powercfg /requests` remains NOT verified because this machine requires elevation and no elevation
  was authorized. Hosted ARM64 evidence is build/test evidence only, not ARM64 runtime proof.

## Human and publication gates

- q-1 in `HUMAN_TODO.md` is an unresolved hard gate. Do not add `LICENSE`, infer a copyright holder,
  tag, or publish without explicit approval.
- q-2 Authenticode signing is optional. If no signing identity is supplied, v0.1.0 must be clearly
  documented as unsigned with checksums and provenance attestations.

After q-1, land the exact approved licence and matching SBOM metadata through a focused reviewed
change. Only then create the annotated v0.1.0 tag, verify every published archive/checksum/SBOM/
attestation and clean install path, and generate WinGet/Scoop manifests from real URLs and hashes.

## Next safe slices

1. Finish Task Scheduler folder ownership cleanup, PowerShell 5.1 packaging portability, and stable
   tag-policy tests on this branch; rerun both shells plus native CMake/CTest.
2. Obtain one fresh-context review of the final logic, open the #4/#5/#9 PR, and ship only after
   exact-head hosted checks and the aging floor.
3. Finish and ship the high-DPI viewport, dependency PR, and portfolio evidence.
4. Resolve q-1/q-2, publish v0.1.0, and run the requirement-by-requirement completion audit.

## Proving commands

This machine uses Visual Studio Build Tools 2019:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 16 2019" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\packaging\Test-Packaging.ps1
```

CI additionally builds ARM64 and Win32.
