# Project state

Last updated: 2026-10-09

## Dark appearance and native material (2026-10-09)

The owner requests dark mode and richer depth after the premium styling pass.
Dark is now the initial appearance. Window & notifications adds saved Dark appearance
and Soft window backdrop checkboxes. The native Mica frame is optional, dynamically
loaded from the system directory, and falls back to a solid surface on unsupported
Windows or in high contrast. Content/controls stay opaque. Native dropdown items
match the palette; theme changes preserve numeric edits and may run during a session.
Existing settings files without these keys receive the dark/soft defaults; no schema
or session-policy change. Issue #126's cursor-dependent paint assertion is addressed.

MSVC 19.29 x64 Release and all seventeen CTests pass. The capture run passes 474
checks, including real dark dropdown routing, live theme changes, unfinished-edit
preservation and repeated brush replacement. Settings tests cover both appearance
values, old-file defaults and malformed-value recovery. The live help test passes
33 tooltips and six fitting hints; the live 192-DPI wheel/repaint test passes three
churn cycles. Two bounded visual rounds cover light/dark and expanded/dropdown states;
PrintWindow previews explicitly render the solid fallback, not the DWM composition.

Installed executable SHA-256:
`47f20f94c8fe70b2fd33d28ba8ec01cf086f6fbd175715f2d3e37816dba31e27`.
The build is installed and visible with the v0.2.0 title, stopped. DWM queries return
success with backdrop=2 (Mica) and dark-caption=1. A pixel sampled only inside the
foreground app's header is 0x2f2e2f, confirming actual composition rather than a
black uncomposited capture. Saved settings bytes and the startup task are unchanged.
Binary/settings/task backups and local synthetic previews are retained outside Git
at `Documents/Codex/IdleHarbor-dark-material-2026-10-09`.

Actual OS high-contrast activation, screen-reader speech, physical monitor transitions,
Windows 10 fallback execution and ARM64 execution remain unverified. This remains an
unreleased development build labelled v0.2.0; published archives/screenshots and
`HUMAN_TODO.md` owner account gates are unchanged.

## Premium native styling (2026-10-09)

The owner retains the compact workflow and requests a premium, almost Apple-like
visual treatment. The source now uses a soft neutral surface, clearer type hierarchy,
rounded fields/actions and quiet chevron disclosures. Native control roles, selection,
numeric caret/Undo, keyboard access and high-contrast fallback remain. See `PRODUCT.md`
and `DESIGN.md` for the owner priorities and visual/interaction contract.

MSVC 19.29 x64 Release and all seventeen CTest targets pass. The synthetic capture
initial run passes 439 checks; final focused tests pass 433 checks, including actual painted action/field colours, disabled state,
numeric editing/Undo and bounded GDI-resource churn. Two visual inspection rounds
cover compact, custom, expanded and native system-colour fallback renders. The live
help test passes 31 tooltips and six fitting hints; the live 192-DPI wheel/repaint
test passes three churn cycles. Actual Windows high-contrast activation, screen-reader
speech, physical monitor transitions and ARM64 execution remain unverified.

The tested executable is installed and reopened stopped. Installed SHA-256:
`1f33267a91d72101b0c3190179349b9ec2c32dcbe129b810854b576ed4674895`.
It matches the source build; saved settings bytes and the existing startup task are
unchanged. The prior executable, settings, startup XML and synthetic previews are
retained outside Git at `Documents/Codex/IdleHarbor-premium-ui-2026-10-09`.
This remains an unreleased development build labelled v0.2.0. Published archives,
website screenshots and `HUMAN_TODO.md` owner gates remain unchanged.

## Compact native window (2026-10-09)

The source window now prioritizes Profile, Keep awake and Session duration.
Duration has common presets and an exact Custom seconds field; Motion & timing,
Safety pauses and Window & notifications start collapsed. All settings retain
their existing validation and persistence. The title displays the canonical version,
and status plus Start/Stop/Save remain outside the scrolling viewport.
See `DESIGN.md` for the assessment, plan and interaction contract.

MSVC 19.29 x64 Release and all seventeen CTest targets pass locally. The additional
compact UI target checks real native controls and actual saved settings, with DPI
layouts at 96/120/144/168/192. Synthetic stopped-window renders use the application
manifest/icon and prove duration access, Tab order and focus recovery on collapse.
No real session is started by that target. Screen-reader speech, physical monitor
transitions and ARM64 execution remain unverified. Published release archives and
website screenshots still describe v0.2.0; `HUMAN_TODO.md` retains owner account gates.

The owner authorized ending the active session and replacing the desktop executable.
The earlier compact x64 build SHA-256 was
`ca75477afeec8dbc26407eed24e3085494ccc632a630445736cc694a54eccbeb`;
it matches the tested source build. Saved settings bytes and the existing startup
task are unchanged, and the app is reopened stopped with `--show`. A prior binary
and settings backup is retained outside Git under the dated local Codex evidence folder.
The live help check passes with 31 tooltips and six fitting expanded-field hints;
the live 192-DPI repaint/wheel check passes with three churn cycles. The capture
portability regression passes 30 checks without recapturing published screenshots.

## Product and release boundary

IdleHarbor remains a native C++20/Win32 Windows keep-awake utility with local
settings, explicit Start/Stop/Exit controls and safety pauses. Preserve visible
user control and immediate stop. Do not add application networking, telemetry,
elevation, hidden behavior or implicit persistence. The website-only, owner-selected
Pulseboard integration remains separate from the application.

The checked-in application, Chocolatey package and website still name v0.2.0.
No new release, archive-pin change or marketplace submission is published by this
maintenance. On 2026-10-08 the owner's desktop installation was upgraded from the
August release binary to the locally verified x64 source build at 01ee3e6 (the same
application source as merged #118/#120). Its installed SHA-256 matches the checked
build, saved settings are unchanged, and the existing limited logon task retains
`--start --minimized`. Real native status plus a forwarded Stop/Start cycle succeed
in the same installed process. This is a development build still labeled v0.2.0,
not a newly published release. GPL-3.0-only and
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
- #116: compose tray availability separately from the session reason; keep native
  status, tooltips and --status consistent through dirty-state failure and recovery.
  Sixteen hidden-native-control scenarios and direct numeric/enum helper checks
  form a 345-assertion regression target.

The concurrent #110 overlaps #109. Its extra timestamp-stability coverage is
retained in the preview suite without reapplying conflicting production changes.
The consolidation PR carries the final integrated validation and supersession
record. See [the continuation handoff](docs/MAINTENANCE-CONTINUED-2026-10-07.md).

## Session toolkit (#117 / #118)

The unreleased source adds tray quick timers, manual Pause/Resume, bounded snooze, extension and
remaining-time status/tooltip with strict CLI equivalents. Session controls do not save preferences;
expiry continues during holds and Resume rechecks safeguards. Failed power release stops automatic
activity, keeps Stop available for retry and blocks a new session until cleanup succeeds. Tray actions
capture a session generation so nested-loop expiry/replacement cannot modify a replacement session.

On 2026-10-08, the complete MSVC 19.29 x64 Release build and all sixteen CTest targets passed locally,
including 65 core session assertions, 113 CLI checks, 101 actual-Application/hidden-control checks and
73 menu-session interleaving checks. Native tests use real hidden controls and menus with test-local
power/input/tray/dialog boundaries; they do not move the pointer or acquire real power requests.
Explorer interaction, cross-process forwarding timing, visible UI/screenshots and ARM64 execution
remain unverified. Hosted exact-head CI and independent review remain merge gates for PR #118.
Existing release archives, website screenshots and installed-user binaries remain v0.2.0.

## Executable checks and their limits

| Surface | Executed checks | Boundary |
| --- | --- | --- |
| Native sources | Twelve registered CTest executables; Windows x64/x86 execution and x64/x86/ARM64 build gates | ARM64 is cross-build only; power and input-hook failures use private API doubles, not induced live OS failures |
| Application status | Actual Application methods, real hidden control text, sixteen status scenarios and numeric/enum helpers | Tray/dialog/show-window calls are intercepted; no Explorer interaction, screen-reader session, visible window or screenshot proof |
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

The 2026-10-08 startup recovery repair retries a failed Windows session-notification
registration when Start re-establishes the session state. Automatic startup can
precede the Windows services required for registration; a single failure no longer
requires restarting the process. Hidden native controls and API doubles exercise
unreadable-state recovery, failed-registration recovery, notification precedence,
unrelated warning preservation and Stop after recovery. The previous Start-time
snapshot retry is protected by a mutation that failed two assertions; the new
registration retry has four pre-fix failures. Physical sleep/wake and reboot remain
separate desktop acceptance checks. The locally installed August v0.2.0 executable
predated these repairs and has now been replaced as described above. The integrated
MSVC 19.29 x64 Release build passes all sixteen CTests; both PowerShell editions
pass the registered regression and packaging fixture suites.

#77 is closed: its reported unbounded shutdown copies are fixed by #101/#104. The broader
orchestration question remains with #51: prove actual Chocolatey install/upgrade/
uninstall behavior, hook failure handling, session/desktop boundaries, architecture
rejection and shim cleanup in an isolated Windows environment. Per-user process
lookup still suppresses inaccessible paths and enumeration errors; do not call
that universally fail-closed discovery.

#115 addresses #86's input-hook handle loss with bounded retention and installing-
thread lifecycle tests. See [cleanup contracts](docs/INPUT-HOOK-CLEANUP.md); pending
handles are retained recovery state, not successful cleanup. #116 adds direct
main.cpp numeric/enum helper coverage and addresses #42/#46 status consistency.
See [status contracts](docs/STATUS-SURFACES.md) for the native-control/API-double
boundary. #118 supplies visible, retryable cleanup-failure presentation, allowing
#86's original tracker claims to close. #56 is also closed: #118 commits pending
combo selections before forwarded overrides and protects ordinary/early synchronous
closeup ordering with hidden-native regression tests. #55's focus-origin edge case
remains open and needs sent-focus/modal-loop interaction proof.

#37 concerns ownership across known-folder relocation. #49/#50/#59 concern
capture quoting, corner privacy and harness cleanup. #70's serialization issue is
addressed by #113, without claiming the other native capture gates are complete.
The #49 portability slice quotes owner configuration paths, reuses the loaded capture helper
and disables the capture-only emergency hotkey. Thirty executable checks pass in each of Windows
PowerShell 5.1 and PowerShell 7, including an actual native argv probe and repeated initialization.
See [capture validation](docs/CAPTURE-MANIFEST-VALIDATION.md). A real spaced-path capture and two
`-Force` captures in one Windows PowerShell 5.1 process remain pending; screenshots are unchanged.
Preserve their native/desktop evidence requirements rather than substituting a
source check or a passing headless build.

Owner-only work remains unchanged: Chocolatey account/API-key publication,
Google Search Console operations and the social-preview Settings upload (#6).
No credentials belong in the repository or logs. No new hosting activation,
marketplace publication or native screenshot recapture is authorized by this record.
