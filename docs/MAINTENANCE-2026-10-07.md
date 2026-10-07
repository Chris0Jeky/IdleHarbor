# Maintenance handoff: 2026-10-07

## Source authority and scope

The input ZIP identifies main `56419c2e702a9642cf2a01f49c4efba75c57a485`; its complete Git tree
matched `87de58bb009936cce15f5e1bd1b31fede29acef6`. All publication, branch reconciliation,
review-thread updates and merge decisions used the connected GitHub tools. No generic-shell
GitHub clone or raw download was used. Local work ran in isolated worktrees of that snapshot.

All 22 initially open issues and all four initially open PRs (#93-#96) were reviewed. Changes
were bounded to release/package correctness, regression execution, input-observer admission,
website metadata and accurate handoffs. Native behavior was not broadened.

## Delivered changes and evidence

| PR | Result | Exact implementation head and hosted evidence |
| --- | --- | --- |
| [#93](https://github.com/Chris0Jeky/IdleHarbor/pull/93) | Merged the two pinned CodeQL action updates | `51a9e08c`; CI 37263608216, CodeQL 37263608259 |
| [#94](https://github.com/Chris0Jeky/IdleHarbor/pull/94) | Merged required-payload preflight; remaining findings completed by #100 | `54f85b80`; CI 36492935590, CodeQL 36492935624 |
| [#95](https://github.com/Chris0Jeky/IdleHarbor/pull/95) | Merged null/zero notification-target guard; CI registration completed by #100 | `a5948a98`; CI 36492954487, CodeQL 36492954420 |
| [#96](https://github.com/Chris0Jeky/IdleHarbor/pull/96) | Merged version fixtures; standalone edition-compatible coverage completed by #100 | `a28eeb7c`; CI 36492974431, CodeQL 36492974694 |
| [#100](https://github.com/Chris0Jeky/IdleHarbor/pull/100) | Merged all seven required payloads and registered dual-edition regression execution | `6cc397aa`; CI 37547353859, CodeQL 37547353856 |
| [#101](https://github.com/Chris0Jeky/IdleHarbor/pull/101) | Merged shared bounded Chocolatey shutdown and 22 behavioral cases | `fabda483`; CI 37548464524, CodeQL 37548464513 |
| [#102](https://github.com/Chris0Jeky/IdleHarbor/pull/102) | Website consensus and hosting source guard, 17 tests, and this consolidated handoff | Implementation `19805c06`; CI 37548794765, CodeQL 37548794675. See the PR for final documentation/integration head qualification. |

All four remaining review threads from #94-#96 were resolved only after #100 delivered the
fixes. Their prior green workflows had not executed the newly added standalone/Pester files;
the final runner removes that false sense of coverage. The InputMonitor guard was reviewed
against its owner-claim/release and valid-caller paths, not represented as native hook execution.

### Test-first observations

The package suite at `7a906546cc874f814ae41d40740722a213c1d9c3` passed the input contract and
11 version cases, then failed specifically because missing `packaging/README.md` was silently
accepted. Both edition logs in [run 37547007433](https://github.com/Chris0Jeky/IdleHarbor/actions/runs/37547007433)
were inspected. The fixed `6cc397aa` passes all eight archive cases and both packaging lanes.
An earlier dynamic-shell workflow configuration failure is not counted as regression evidence.

The shutdown suite at `58db55e1df6c086f17f084c7e3e9262f6f61656f` exposes unbounded uninstall,
missing ownership/session/error rejection and unsafe empty/single-result `.Count` handling.
The PowerShell 5.1 log in [run 37548145664](https://github.com/Chris0Jeky/IdleHarbor/actions/runs/37548145664)
was inspected. The fixed `fabda483` passes all 22 cases in both editions. The initial test-double
scope bug was corrected before taking that red evidence and is not a production defect claim.

The website's first 16 tests failed against a no-op validator and passed after implementation.
The seventeenth test failed against the prior two-source hosting manifest and passed after
repair. All 17 then passed locally, along with the real-site-file validator and SDK contract.

### What the green checks do not establish

CI builds x64, x86 and ARM64. Only x64 and x86 execute the seven CTest binaries. ARM64 remains
cross-build evidence, not ARM64 execution. The script lanes run 11 version cases, 8 archive
cases, 22 mocked shutdown cases and the source-only InputMonitor contract in each PowerShell
edition, plus the existing full packaging checks.

There was no interactive Windows desktop, real Chocolatey/WinGet lifecycle, public artifact
re-download, owner-account operation, release publication, local installed-app upgrade or
Cloudflare deployment in this batch. A child wait and polling budget do not bound the duration
of an operating-system CIM query. Real Chocolatey abort behavior still needs isolated proof.

## Disposition of the 22 initially open issues

Live GitHub issue state is authoritative. This table records the reviewed scope and next gate;
reviewing an issue is not a claim that its fix or its platform-specific proof was completed.

| Issue | Disposition / next acceptance gate |
| --- | --- |
| #6 | Prepared social-preview upload remains a GitHub Settings/UI action; no upload claimed. |
| #37 | Preserve ownership across Start Menu known-folder relocation; needs transactional Windows proof. |
| #38 | Make installer/uninstaller WhatIf output unambiguously preview-only and assert no mutations in both editions. |
| #40 | Add normalized full-text GPLv3 completeness proof, not just recognizable license markers. |
| #42 | Add deterministic tray-failure/dirty-state tests with truthful visible status. |
| #46 | Preserve tray recovery and status-refresh semantics; validate native failure/recovery paths. |
| #48 | Packaging suite restores a pre-existing ScheduledTasks Get-ScheduledTask after the test double is removed. |
| #49 | Harden capture path quoting, repeated interop loading and hotkey setup on a real desktop. |
| #50 | Normalize rounded screenshot-corner backgrounds without altering the captured application. |
| #51 | Advanced by #101's fail-closed script behavior; real Chocolatey lifecycle, architecture, session and cleanup remain unverified. |
| #55 | Validate focus changes that bypass the normal message loop with native UI automation. |
| #56 | Preserve pending combo selection across forwarded commands and prove the interaction on Windows. |
| #59 | Factor native harness setup/teardown without losing original exceptions or bounded cleanup. |
| #65 | Bound allowed Chocolatey version lag using release history instead of accepting every older version. |
| #66 | Add a read-only scheduled/manual lane that downloads the pinned archive and checks its real digest. |
| #68 | Add automated validation of tracked WinGet identity, version, architecture, executable path and checksums. |
| #70 | Produce deterministic capture-manifest serialization in PowerShell 5.1 and 7. |
| #76 | Addressed by #102: six website version references agree independently of CMake. |
| #77 | Chocolatey path fixed by #101; per-user installer still has an unbounded exit-command wait, and orchestration proof remains. |
| #78 | Resolved by #101: registered executable wrapper tests cover no-running, foreign-session, hung-child and failure cases. |
| #82 | Addressed by #102: inert hosting manifest names actual routing/release authorities and retains no-activation semantics. |
| #86 | Keep later runtime hardening bounded: visible status, power-clear failure, observer cleanup and invalid interval checks need focused proof. |

## Recommended next slices

1. Fix #77's per-user `Stop-OwnedApplicationIfRunning` wait with regression-first tests while
   preserving exact executable ownership, WhatIf and transactional update behavior. Do not
   force-kill a discovered application instance.
2. Tackle #38/#40 as small independent packaging-contract changes, then #65/#68/#66 as
   distribution validation. Real downloads and upstream publication are different acceptance gates.
3. Run #51 in an isolated Windows environment before claiming Chocolatey lifecycle readiness;
   keep owner credentials outside the repository and logs. Continue native focus/status/capture
   work only with the corresponding desktop evidence.

The prior state document is retained at
[docs/history/PROJECT_STATE-before-2026-10-07.md](history/PROJECT_STATE-before-2026-10-07.md)
with the exact original blob `7440541f36b4981ba361b7a950037bef2f58fbe8`. It retains historical
release hashes, screenshot identities, installed-PC observations and earlier reasoning without
presenting them as fresh verification. `PROJECT_STATE.md` now separates current implementation
from historical external state and names the executable commands and remaining gates.
