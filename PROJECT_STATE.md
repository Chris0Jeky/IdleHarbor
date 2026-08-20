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

`origin/main` is `b4bf3dd850bf4abe408fbece4d473c2117bf2bef`, including the merged settings-recovery
pull request [#8](https://github.com/Chris0Jeky/IdleHarbor/pull/8) and reviewed GitHub Actions
dependency pull request [#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7). Issue
[#3](https://github.com/Chris0Jeky/IdleHarbor/issues/3) is closed.

The settings-recovery change passed exact-head x64, ARM64, x86, CodeQL analysis, and CodeQL
scanning; its review thread was fixed and resolved. A real UI Automation smoke proved warning
visibility, blocking of automatic recovered starts, explicit Start, truthful Running/Stopped state,
Save recovery, healthy relaunch, and clean exit. The post-merge sweep found no late feedback.

## Installer and release trust

Branch `agent/v0.1.0-installer-trust` contains:

- transactional fresh-install and update rollback;
- restoration of managed files, the ownership marker, and owned startup state after caught failure;
- linked-file/reparse rejection before managed paths can cross the install-root boundary;
- tracked ownership and cleanup of installer-created empty Task Scheduler folders while preserving
  pre-existing or non-empty folders;
- application-manifest version validation;
- a tracked-root-`LICENSE` publication guard without inventing licence approval; and
- an explicit contract keeping checksums and per-architecture SPDX files as release siblings;
- a stable-only tag policy shared by the workflow and embedded-version validator; and
- packaging/checksum verification compatible with PowerShell 7 and Windows PowerShell 5.1.

Verified before merging current `main`:

- native x64 Release build and CTest passed 6/6;
- the complete packaging suite passed under PowerShell 7 and Windows PowerShell 5.1, including
  normalized archive/checksum paths, stable-tag rejection, and hard-link boundary refusal;
- the installer transaction smoke passed under PowerShell 7 and Windows PowerShell 5.1; and
- a real rollback matrix passed under both shells for RunKey, Startup Folder, and Task Scheduler,
  covering fresh failure cleanup, exact existing-startup restoration, file/marker rollback,
  unexpected-file preservation, successful update, clean uninstall, and preservation of a
  pre-existing scheduler folder.

A fresh-context independent review of `dd65ee8` found no confirmed CRITICAL/HIGH defect. Later live
testing found the empty scheduler-folder residue and linked-file boundary defect; both now have
direct regression coverage, but the final combined logic still needs one fresh-context review after
current `main` is integrated.

Intended merge issues are [#4](https://github.com/Chris0Jeky/IdleHarbor/issues/4),
[#5](https://github.com/Chris0Jeky/IdleHarbor/issues/5), and
[#9](https://github.com/Chris0Jeky/IdleHarbor/issues/9).

## Remaining project queue

- Branch `agent/v0.1.0-ui-viewport` has the high-DPI layout implementation but still needs current
  `main` integrated, child-target/high-resolution wheel routing, and real mixed-DPI UI evidence for
  [#2](https://github.com/Chris0Jeky/IdleHarbor/issues/2).
- Dependabot pull request [#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7) passed exact-head CI and
  a fresh-base review, then merged with a merge commit.
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

1. Merge current `origin/main`, rerun the scoped two-shell packaging/startup tests plus native
   CMake/CTest, and verify no startup residue remains.
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
