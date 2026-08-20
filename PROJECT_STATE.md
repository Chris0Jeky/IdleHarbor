# Project state

Last updated: 2026-08-20 12:05 UTC

## Stop-safe checkpoint

The session ended intentionally at a recoverable checkpoint. Every intended repository change is
committed and pushed. The completed settings-recovery slice is merged; the installer and viewport
slices are saved on separate remote branches and are not represented as merge-ready. No release,
tag, licence, signing decision, startup configuration, or external deployment was created.

The primary checkout intentionally remains on `agent/v0.1.0-ui-viewport` so the unfinished viewport
work and this handoff are immediately visible on resume. The only remaining delegated worktree is
the clean installer checkout. The completed settings worktree was removed with plain
`git worktree remove` after its status showed only disposable ignored build output.

## Landed on `main`

`origin/main` is `f9c07ef0a5e5b0b4c73bc591bc8b448bfabf3ec2`, the merge commit for pull request
[#8](https://github.com/Chris0Jeky/IdleHarbor/pull/8). Issue
[#3](https://github.com/Chris0Jeky/IdleHarbor/issues/3) is closed.

The landed settings-recovery behavior:

- shows every recovery warning and leaves the window visible;
- blocks initial or forwarded automatic start and stopped-session toggle until a successful Save;
- keeps explicit in-window Start under the user's control;
- preserves `Stopped: ready` on a healthy launch; and
- preserves Running or Paused status when a redundant forwarded start is blocked.

Exact-head x64, ARM64, x86, CodeQL analysis, and CodeQL scanning checks passed. A fresh-context
review passed after the logic fixes, the sole connector thread was fixed and resolved, and the
three-minute final-head aging floor elapsed before merge. A real Windows UI Automation smoke covered
the warning dialog, all four warnings, blocked launch commands, explicit Start, redundant forwarded
Start, Stop, Save, post-save start, healthy relaunch, and clean exit. The immediate post-merge sweep
found no untriaged late feedback.

## Saved installer and release-trust slice

Branch: `agent/v0.1.0-installer-trust`  
Remote head: `dd65ee83af1113100936420dc860c8c2169f26cf`  
Worktree: `..\IdleHarbor-worktrees\installer-trust-ship`

Commits:

- `e8340cf` — fresh-install rollback and failure injection;
- `a8d84c5` — embedded-version and tracked-root-`LICENSE` publication guards; and
- `dd65ee8` — snapshots/restores managed installation files, the ownership marker, and owned
  startup state when an update or startup mutation throws.

Saved verification at `dd65ee8`:

- both changed PowerShell files parse successfully under PowerShell 7.6.4;
- `git diff --check` passes; and
- `packaging/Test-Packaging.ps1` passes, including fresh-root rollback, preservation of a
  pre-existing empty root, exact update file/marker restoration, preservation of an unknown user
  file, successful update, and same-source/destination marker rollback.

Earlier real installer smoke passed under PowerShell 7 and Windows PowerShell 5.1 for injected fresh
failure, sentinel preservation, actual install, hash/marker validation, launch/exit, and clean
uninstall. The complete packaging suite currently passes under PowerShell 7; its Windows PowerShell
5.1 ZIP-entry comparison incompatibility is tracked by
[#9](https://github.com/Chris0Jeky/IdleHarbor/issues/9).

This branch is **not merge-ready yet**. The two prior HIGH review findings motivated `dd65ee8`, but
that fix commit still needs a fresh-context review and real startup/update rollback smoke. It also
needs `origin/main` merged into the branch, current native CMake/CTest, hosted exact-head CI, PR
review, and the normal aging gate. Rollback covers caught PowerShell failures, not machine loss or
process termination mid-transaction; stronger crash consistency would require a durable journal.
No PR is open for this branch. Intended issues are
[#4](https://github.com/Chris0Jeky/IdleHarbor/issues/4) and
[#5](https://github.com/Chris0Jeky/IdleHarbor/issues/5).

## Saved high-DPI viewport slice

Branch: `agent/v0.1.0-ui-viewport`  
Implementation commit: `be6b42d77e73cac563e7bce76abe4a14fedbd4a4`

The slice adds DPI-scaled control geometry, work-area clamping, resize layout, vertical scrolling,
focus reveal, and pure layout/scroll helpers with tests. The saved x64 Release build completed and
CTest passed 7/7, including `window_layout`; the committed source passed `git diff --check`.

This branch is **not merge-ready yet**. It predates PR #8 and both slices edit `src/app/main.cpp`.
Merge `origin/main` into this published branch and resolve the overlap without rewriting history.
Then finish child-target wheel routing, retain partial high-resolution wheel deltas, avoid swallowing
combo-popup input, and run genuine resize, scrollbar-thumb, focus-reveal, mixed-DPI, and tray/UI
smokes. Update docs, changelog, and screenshots only after the actual behavior is proven. Intended
issue: [#2](https://github.com/Chris0Jeky/IdleHarbor/issues/2).

## GitHub and portfolio queue

- Dependabot pull request [#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7) is open with green
  checks but still needs its one real review before merge.
- Portfolio visuals and the repository social preview remain under
  [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6). The existing 1280x640 asset needs a
  contrast pass, and genuine Running, Paused, and tray screenshots still need to be captured.
- The browser file chooser previously prevented automated social-preview upload; treat the GitHub
  setting as a human/manual fallback if a safe automated path is still unavailable.
- Hosted ARM64 evidence is build/test evidence only, not representative ARM64 runtime proof.
- `powercfg /requests` remains NOT verified because this machine requires elevation and no elevation
  was authorized.

## Human gates and release boundary

`HUMAN_TODO.md` remains authoritative:

- q-1: explicit open-source licence approval is unresolved. Do not add a `LICENSE`, infer the
  copyright holder, tag, or publish a release until the user answers it.
- q-2: Authenticode signing is optional. If no signing identity is supplied, document v0.1.0 as
  unsigned and rely on checksums and provenance attestations.

After q-1 is answered, land the exact approved licence and SBOM metadata through a focused reviewed
change. Only after every exact-head release gate passes should `v0.1.0` be tagged. Verify the actual
published archives, checksums, SBOM, attestations, and clean install paths before generating WinGet
or Scoop manifests from real URLs and hashes.

## Resume order

1. Fetch `origin` and read `AGENTS.md`, `HUMAN_TODO.md`, and this file.
2. Finish and review the installer rollback fix at `dd65ee8`; merge current `origin/main`, rerun its
   scoped tests, and open the #4/#5 PR only when the branch is genuinely ready.
3. Merge the newly landed `main` into `agent/v0.1.0-ui-viewport`, finish the wheel/input behavior,
   and complete #2 with real visual/runtime evidence.
4. Review Dependabot #7 and complete the portfolio evidence in #6.
5. Ask the user for q-1 once implementation is otherwise release-ready; handle q-2 explicitly.
6. Run a final end-to-end completion audit, then publish and verify v0.1.0. The broader project goal
   remains active; this checkpoint is a pause, not a completion claim.

## Useful local evidence

Scratch smoke scripts are saved outside the repository under the session's `work` directory:

- `test-settings-recovery.ps1`
- `test-installer-transaction.ps1`
- `test-forwarded-command.ps1`
- `test-foreign-startup.ps1`
- `test-power-request.ps1`
- `capture-idleharbor.ps1`

On this machine use the installed Visual Studio 2019 generator:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 16 2019" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\packaging\Test-Packaging.ps1
```

CI additionally configures ARM64 and Win32.
