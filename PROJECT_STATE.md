# Project state

Last updated: 2026-10-07

## Product and release boundary

IdleHarbor remains a native C++20/Win32 Windows keep-awake utility with local
settings, explicit Start/Stop/Exit controls and safety pauses. Preserve visible
user control and immediate stop. Do not add application networking, telemetry,
elevation, hidden behavior or implicit persistence. The website-only, owner-selected
Pulseboard integration remains separate from the application.

The checked-in application, Chocolatey package and website still name v0.2.0.
This maintenance work does not publish a new release, change archive pins, update
an installed-user application or submit a marketplace package. GPL-3.0-only and
unsigned distribution are unchanged. Hosting activation remains false.

Historical release hashes, screenshots and installed-PC observations are retained
in [the historical state record](docs/history/PROJECT_STATE-before-2026-10-07.md).
They are not current-machine, desktop, account or public-site verification.
`HUMAN_TODO.md` remains the authority for owner-only decisions.

## Delivered maintenance

The first batch (#93-#102) qualified existing PRs, completed release payload
preflight, registered both PowerShell editions, shared bounded Chocolatey shutdown,
and added website release/hosting contracts. Its dated evidence remains in
[the first handoff](docs/MAINTENANCE-2026-10-07.md).

The continuation delivers:

- #103: valid single-dash configuration filenames and 109 parser-boundary cases.
- #104: bounded per-user install/uninstall shutdown and 24 isolated cases.
- #105: complete normalized GPLv3 integrity, with nine tracked/untracked fixtures.
- #106: read-only automation that downloads and hashes the actual Chocolatey archive.
- #107: at most one dated-release lag for Chocolatey, with 14 regression cases.
- #108: WinGet metadata contracts, 28 offline cases, and actual-byte verification
  of all four tracked x64/ARM64 archives for v0.1.0/v0.2.0.
- #109: explicit preview summaries and genuine-unowned-file warnings, protected
  by six fixture operations and source/installed-byte preservation checks.
- #112: retryable failed power cleanup, rejected invalid interval bounds and
  executable coverage for every safety-pause status reason.
- #113: deterministic capture-manifest JSON with pure formatter cases and
  read-only hash/dimension verification of all five committed PNGs.
- #111: packaging fixture isolation in an edition-matched child PowerShell process,
  preserving caller-owned commands and variables on success and child failure.
- #115: retain failed input-hook handles across Stop, partial refresh and destruction;
  thirteen lifecycle scenarios cover bounded cleanup and callback quiescence.

The concurrent #110 overlaps #109. Its extra timestamp-stability coverage is
retained in the preview suite without reapplying conflicting production changes.
The consolidation PR carries the final integrated validation and supersession
record. See [the continuation handoff](docs/MAINTENANCE-CONTINUED-2026-10-07.md).

## Executable checks and their limits

| Surface | Executed checks | Boundary |
| --- | --- | --- |
| Native sources | Eleven registered CTest executables; Windows x64/x86 execution and x64/x86/ARM64 build gates | ARM64 is cross-build only; power and input-hook failures use private API doubles, not induced live OS failures |
| Script regressions | Both PowerShell editions: 11 version, 8 archive, 22 Chocolatey shutdown, 24 per-user shutdown, 9 license and 14 release-lag cases; preview/isolation suites; InputMonitor source contract | No live input hooks or complete Chocolatey lifecycle proof |
| Packaging fixtures | Public Test-Packaging entry point starts the internal fixture suite in a same-edition child process | Process isolation protects caller namespaces, not a filesystem or Windows-API sandbox |
| Capture metadata | Fixed-format and ten edge checks in both PowerShell editions; five existing PNG hashes/dimensions | No fresh capture, pointer movement or native visual inspection |
| Website | 17 Node cases, six release-reference agreement checks, SDK 3.3.1 contract | Offline repository checks, not current deployment or live-site inspection |
| WinGet | 28 Node cases on Windows/Linux, four tracked architecture-specific archive pins | Focused plain-scalar manifest contract, not a general YAML parser or full WinGet schema validator |
| Published archives | Automated real ZIP downloads and SHA-256 comparisons for Chocolatey and WinGet | Bytes are not extracted/executed; no package-manager installation or moderation proof |

Use [the validation guide](packaging/VALIDATION.md) for exact commands and workflow
names. Registered regressions fail on terminating assertions; they do not depend
on Pester discovery. The public packaging entry point is unchanged, but callers
must not invoke its internal fixture implementation directly when isolation matters.

Local work used a reconciled ZIP in Linux worktrees. GCC/Clang exercised portable
and test-double C++ suites; Node exercised offline metadata/hash tests. Hosted
Windows CI supplied MSVC, both PowerShell editions and real release-download proof.
GitHub commits, exact-head CI and live issue state outrank these summaries.

## Remaining work and owner gates

#77's reported unbounded shutdown copies are fixed by #101/#104. Its broader
orchestration question remains with #51: prove actual Chocolatey install/upgrade/
uninstall behavior, hook failure handling, session/desktop boundaries, architecture
rejection and shim cleanup in an isolated Windows environment. Per-user process
lookup still suppresses inaccessible paths and enumeration errors; do not call
that universally fail-closed discovery.

#115 addresses #86's input-hook handle loss with bounded retention and installing-
thread lifecycle tests. See [cleanup contracts](docs/INPUT-HOOK-CLEANUP.md); pending
handles are retained recovery state, not successful cleanup. #86 retains main.cpp
helper coverage and visible cleanup-failure presentation. #112 repairs the low-level
power object's state and bool result, not every UI caller of void Clear. #42/#46
still need accessible/visible tray and status consistency; #55/#56 need focus/combo
interaction proof.

#37 concerns ownership across known-folder relocation. #49/#50/#59 concern
capture quoting, corner privacy and harness cleanup. #70's serialization issue is
addressed by #113, without claiming the other native capture gates are complete.
Preserve their native/desktop evidence requirements rather than substituting a
source check or a passing headless build.

Owner-only work remains unchanged: Chocolatey account/API-key publication,
Google Search Console operations and the social-preview Settings upload (#6).
No credentials belong in the repository or logs. No new hosting activation,
marketplace publication or native screenshot recapture is authorized by this record.
