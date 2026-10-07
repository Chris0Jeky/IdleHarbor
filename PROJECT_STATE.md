# Project state

Last updated: 2026-10-07

## Product and release boundary

IdleHarbor is a native C++20/Win32 Windows keep-awake utility with a platform-neutral policy
core, local settings, explicit Start/Stop/Exit controls and safety pauses. Keep the application
visible and user-controlled. Do not add application networking, telemetry, hidden behavior,
implicit startup, elevation or security-policy bypasses.

The package and website still target `v0.2.0`. This maintenance batch does not create a release,
change a published checksum, install anything on the development PC, submit a marketplace
package or deploy the site. GPL-3.0-only and intentionally unsigned distribution are unchanged.
The website's separate, owner-selected Pulseboard integration is retained; the checked-in SDK
contract passes `node observatory/check.mjs` and identifies SDK 3.3.1.

Earlier release, installed-PC, screenshot, distribution and native-desktop observations are
preserved byte-for-byte in [the historical state record](docs/history/PROJECT_STATE-before-2026-10-07.md).
They are dated evidence, not a fresh assertion about the current machine, upstream moderation,
public-site availability or an owner account. Recheck the relevant service before relying on an
old external status. `HUMAN_TODO.md` remains the authority for owner decisions.

## Maintenance delivery

The uploaded ZIP was reconciled to main `56419c2e702a9642cf2a01f49c4efba75c57a485`, including
its full Git tree, before edits. Publishing and exact-head CI used the connected GitHub tools.

- Merged #93: updated pinned CodeQL actions.
- Merged #94, #95 and #96: required release payload preflight, invalid InputMonitor notification
  target rejection, and release-version regression additions.
- Merged #100: completed all four remaining review findings from those PRs. One seven-file
  table now drives release preflight and copying, including `DISTRIBUTION.md`. Missing input
  preserves an existing archive. Standalone regression scripts are registered in CI in both
  Windows PowerShell 5.1 and PowerShell 7; no Pester installation is required.
- Merged #101: shared bounded, session-aware Chocolatey shutdown. Unknown process ownership
  or failed discovery is rejected; only a newly spawned, timed-out `--exit` child may be killed.
  Discovered application instances are never force-terminated. Issue #78 is resolved; #77 and
  #51 retain the broader installer and real-lifecycle follow-ups.
- #102 adds a dependency-free website release-consensus guard and corrects the inert hosting
  manifest's source paths. The six site references must agree with each other, not with CMake,
  so the release/repoint window remains valid. Hosting activation stays false and the canonical
  origin and all release destinations remain unchanged.

See [the maintenance handoff](docs/MAINTENANCE-2026-10-07.md) for exact-head red/green evidence,
all 22 issues reviewed, unverified boundaries and the next work slices. PR #102 carries the
final integrated validation record for this batch.

## Executable validation

| Surface | Checked path | Evidence boundary |
| --- | --- | --- |
| Native core and application | Windows CI builds x64, x86 and ARM64; x64/x86 run seven CTest executables | ARM64 is cross-build only; no representative ARM64 execution |
| Script regressions | `tests/Test-RegressionScripts.ps1` in PowerShell 5.1 and 7 | 11 version cases, 8 archive cases, 22 mocked Chocolatey shutdown cases, plus an InputMonitor source-contract check |
| Packaging | `packaging/Test-Packaging.ps1` in both PowerShell editions | Existing local/fixture checks, not real Chocolatey or WinGet orchestration |
| Website release and hosting | `node --test tests/site_release_tests.mjs` and `node tools/check-site-release.mjs` | 17 offline cases; no deployment or live-site verification |
| Website SDK | `node observatory/check.mjs` | Checked-in artifact, wiring, reporting hooks and CSP contract |

The maintenance container was Linux without PowerShell, the Windows SDK or an interactive
Windows desktop. Three portable C++ suites (core, CLI and window layout), the 17 Node tests,
the site release guard and the SDK check ran locally. Hosted Windows CI supplies the Windows
build and PowerShell results. The InputMonitor test checks source structure, not live hooks.
Shutdown tests use local command doubles and do not start or terminate a real application.

From a Windows checkout with Visual Studio 2022 Build Tools:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\tests\Test-RegressionScripts.ps1
.\packaging\Test-Packaging.ps1
node --test tests/site_release_tests.mjs
node tools/check-site-release.mjs
node observatory/check.mjs
```

Run packaging checks sequentially in each edition. Native viewport/help-tip scripts and the
capture tool require a real interactive desktop and were not run in this batch. Current-head
proof must not be replaced with a log from an earlier commit.

## Next work and owner gates

First address the related unbounded per-user installer wait in #77 while preserving exact-path
ownership and `-WhatIf`. Preview wording (#38) reports a preview summary and does not treat
WhatIf-retained managed files as unexpected leftovers. Then handle packaging test isolation (#48),
license completeness (#40), bounded Chocolatey version lag (#65), and WinGet manifest guards
(#68) through failing regression tests. #66 concerns published-archive verification rather
than offline metadata agreement.

Use an isolated Windows environment for #51's real Chocolatey lifecycle, session, architecture
and cleanup evidence. Do not infer that Chocolatey aborts every mutation merely because a mocked
entry point throws. Native focus/status/capture follow-ups retain their desktop proof requirements.

Owner actions remain outside this maintenance batch: Chocolatey account/API-key publication
(q-3), Google Search Console verification/submission (q-4), and social-preview upload (#6).
Do not put credentials into the repository, PR comments or logs. No hosting activation is
authorized by `.hosting/manifest.json`; it is a planning record, not a deployment instruction.
