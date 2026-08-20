# Project state

Last updated: 2026-08-20

## Stop-safe checkpoint

The 2026-08-20 session ended intentionally at a recoverable checkpoint. All intended source and
documentation work is committed, the two temporary delegated worktrees were removed with plain
`git worktree remove`, and every active branch is saved on `origin`. This checkout intentionally
remains on `agent/v0.1.0-ui-viewport` so this handoff and the unfinished viewport slice are visible
on resume. Ignored build products remain locally and are disposable.

`origin/main` is `986cb011ea645f73e0d806f623c15e454bd740c1`, the merge commit for pull request
[#1](https://github.com/Chris0Jeky/IdleHarbor/pull/1). The PR merged with x64, ARM64, x86,
CodeQL analysis, and CodeQL scanning green; all 11 review threads were resolved and the post-merge
review sweep found no untriaged late feedback. Main branch protection still requires the four named
build/analysis checks and conversation resolution, and disallows force pushes and deletion.

No release or tag has been published. Human licence approval remains a hard release gate; see
`HUMAN_TODO.md`.

## Saved follow-up branches

### `agent/v0.1.0-settings-safety`

Remote head: `95a619a639b2ece23c575dd993d91a2e236623f6`

- `6f3dda5` blocks automatic start and stopped-session toggle after settings recovery until the
  warnings have been shown and the settings are successfully saved.
- `95a619a` adds malformed, out-of-range, and cross-field warning coverage and updates the user,
  safety, troubleshooting, changelog, and project-state documentation.
- Verified locally: x64 Debug build and CTest 6/6; x64 Release build and CTest 6/6;
  `git diff --check origin/main..HEAD`.
- NOT verified: hosted CI, x86, or a real GUI/tray smoke of the recovery message and forwarded
  startup-command behavior.
- Intended issue: [#3](https://github.com/Chris0Jeky/IdleHarbor/issues/3).

### `agent/v0.1.0-installer-trust`

Remote head: `a8d84c5facda76fb265df402fba59f2aeacd4515`

- `e8340cf` makes a fresh per-user installation transactional and adds injected-failure rollback
  coverage.
- `a8d84c5` validates the application manifest version, adds a tracked-root-`LICENSE` publication
  guard without inventing licence approval, and documents which trust assets live outside the
  portable ZIP.
- Verified locally: `packaging/Test-Packaging.ps1`, YAML lint for all workflows,
  `Test-ReleaseVersion.ps1 -Tag v0.1.0`, and `git diff --check origin/main..HEAD`.
- Expected gate: the standalone publication guard rejects the current repository because no tracked
  root `LICENSE` exists.
- NOT verified: current native CMake/CTest, hosted CI/review, runtime install QA, signing, or an
  actual publication.
- Residual risk: rollback restores fresh-install files and directories, but does not snapshot a
  startup registration if the real registration operation partially mutates before failing.
- Intended issues: [#4](https://github.com/Chris0Jeky/IdleHarbor/issues/4) and
  [#5](https://github.com/Chris0Jeky/IdleHarbor/issues/5).

### `agent/v0.1.0-ui-viewport`

Viewport implementation commit: `be6b42d77e73cac563e7bce76abe4a14fedbd4a4`

- Adds canonical DPI-scaled control geometry, work-area clamping, resize layout, vertical scrolling,
  focus reveal, and pure layout/scroll helpers with tests.
- Verified locally: the existing x64 Release build completed and CTest passed 7/7, including the
  new `window_layout` test; the staged source passed `git diff --check` before commit.
- NOT verified: hosted CI/review; a real desktop smoke for resize, scrollbar thumb, child-targeted
  mouse wheel, high-resolution wheel deltas, focus auto-scroll, and mixed-DPI monitor movement.
- Review before PR: the current wheel path handles top-level `WM_MOUSEWHEEL` and does not retain
  partial wheel deltas. Confirm child-target routing and combo-popup behavior before calling #2
  complete.
- Intended issue: [#2](https://github.com/Chris0Jeky/IdleHarbor/issues/2).

The settings-safety and viewport branches both change `src/app/main.cpp`. Integrate them
sequentially and preserve both commit histories; merge the newly landed `main` into the remaining
branch and resolve that overlap explicitly rather than force-rewriting a published branch.

## Evidence already gathered on the landed base

- Real Windows startup-collision smokes proved that foreign Startup-folder, HKCU Run, and scheduled
  task entries are rejected before install-root mutation and remain preserved.
- A forwarded invalid start returned promptly, displayed its owner-side error, accepted a follow-up
  show command, and exited cleanly.
- Power-only runtime control reached Running, accepted Stop, reported `Stopped: manually stopped`,
  and exited cleanly.
- UI Automation found 21 focusable controls; forward and reverse keyboard traversal worked and
  Space activated Save. Enter intentionally does not start the application by default.
- `powercfg /requests` was NOT verified because this machine requires an elevated shell and no
  elevation was authorized.

## GitHub queue

- Milestone issues [#2](https://github.com/Chris0Jeky/IdleHarbor/issues/2) through
  [#6](https://github.com/Chris0Jeky/IdleHarbor/issues/6) remain open.
- Dependabot pull request [#7](https://github.com/Chris0Jeky/IdleHarbor/pull/7) appeared after the
  product PR merged and was not reviewed in this session.
- No follow-up product PR is open yet; the three branches above are checkpoints, not merge-ready
  claims.

## Next safe slices

1. Review `agent/v0.1.0-settings-safety`, perform its real GUI recovery/startup smoke, open a
   ready-for-review PR that closes #3, and merge only after exact-head required CI, one review, and
   the three-minute aging floor.
2. Review `agent/v0.1.0-installer-trust`, run current native tests plus real install failure/rollback
   smoke, then open the #4/#5 PR and use the same gate. The missing licence is an expected release
   blocker, not a reason to weaken the guard.
3. Merge the new main into `agent/v0.1.0-ui-viewport`, resolve the settings-window overlap, finish
   child-target/high-resolution wheel routing, run the real resize/focus/mixed-DPI smoke, update
   user docs/changelog and genuine screenshots, then open the #2 PR.
4. Complete the portfolio visual evidence and manually upload the committed 1280x640 social preview
   for #6. Automated browser attachment was denied; no repository setting was changed.
5. Ask for an explicit answer to q-1. If MIT is approved, add the exact approved licence/copyright
   in a focused reviewed change and update SBOM licence fields. Do not infer approval. q-2 may remain
   explicitly unsigned for v0.1.0.
6. Only after all exact-head release gates pass: create and push the annotated `v0.1.0` tag, verify
   the published archives/checksums/SBOM/attestations and clean-machine install paths, then add
   WinGet/Scoop manifests from real asset URLs and hashes.

## Proving commands

From a Windows developer PowerShell:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\packaging\Test-Packaging.ps1
```

CI additionally configures ARM64 and Win32. Hosted ARM64 evidence proves build/test only, not
runtime behavior on representative ARM64 hardware.
