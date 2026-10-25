# Project state

Last updated: 2026-10-25

## Sent and nested-loop focus classification (#55)

Input classification now lasts through the current queued dispatch. The main loop
resets it before waiting again, allowing sent/programmatic focus to reveal its
target after pointer input. Child subclasses scope their own input classification,
so a nested message pump preserves pointer-focus stability and reveals keyboard
targets, including a clipped control that already holds focus. Returning from
nested input clears its cause instead of reviving an older one. Non-input messages
do not retry pointer-suppressed reveals; the open-combo reveal guard is unchanged.

On base `658bc7d`, the Visual Studio 17 2022 x64 Release build and all seventeen
CTests pass. The expanded `compact_ui` fixture passes 596 checks. It sends a
cross-thread focus request while GetMessage waits, then uses a window-procedure
nested pump to dispatch posted keyboard and pointer messages without wWinMain.
Both styled and native-color paths use real Win32 controls, focus and geometry;
there is no injected desktop input. The same tests compile against the original
main.cpp extracted by read-only `git show`, then fail ten assertions. Restoring
the generated test include byte-for-byte and rebuilding returns them to green.
The runner and logs remain in ignored `build/Test-FocusOriginsBaseline.ps1` and
`build/focus-{baseline,fixed}-{build,test}.log`.

Windows 10, x86/ARM64 execution, actual screen-reader/UI Automation clients and
real MessageBox/TrackPopupMenu interaction are not verified by the synthetic nested
pump. Hosted CI and publication belong to the coordinator; no Git writes are
performed. `HUMAN_TODO.md` q-3 and q-4 remain open and unchanged.

## Simultaneous policy safeguards (#145)

Core tests now cover all 36 pairs among maximum duration, manual pause and the
seven automatic safeguard reasons. Each condition is also checked alone, plus
an all-clear Run control. Expectations use an explicit order independent of
`reason_priority`, checking action, state, reason and cooldown as well as the
engine's stored state/reason. Only the selected safeguards are enabled, so the
on-battery input required by low-battery protection cannot mask another pair.
Separate cases check exact-boundary expiry with every automatic safeguard active
during both indefinite manual pause and snooze, and that expiry remains stopped.
The current order is documented in `docs/ARCHITECTURE.md`; production behavior
is unchanged.

On base `bbaa0bd`, the Visual Studio 17 2022 x64 Release build and all seventeen
CTests pass before and after the additions. The focused command is
`ctest --test-dir build/x64 -C Release -R '^(core|core_contracts|session_controls)$' --output-on-failure`.
A temporary mutation moves lock handling before manual pause in `evaluate`.
The original core tests still pass; the expanded core tests fail on the
manual-pause/lock pair and the all-safeguards pre-expiry assertion. Both source
files are restored byte-for-byte to their intended versions in a `finally` block,
then the complete build and CTest suite pass again. The mutation runner, original
test copy and logs remain under ignored `build/` (`Test-PolicyPriorityMutation.ps1`,
`core_tests.baseline.cpp`, `mutation-*-build.log`, `mutation-*-test.log`).

Windows x86/ARM64, live desktop behavior and hosted CI are not verified for this
test-only change. No Git writes or PR publication are performed; the coordinator
owns those steps. `HUMAN_TODO.md` q-3 and q-4 remain open and unchanged.

## Read-only Start Menu cleanup and retry evidence (#147)

Update and uninstall now clear ReadOnly through the deletion handle only after
matching the shortcut's file identity and bytes. Other attributes remain intact;
if native deletion fails, the helper attempts to restore the original attributes.
Cleanup errors now terminate: update rolls back its new launcher, executable and
marker, while uninstall stops before deleting installed files or the marker.
The retained marker allows cleanup to be retried after a sharing/access failure.

On base `ae474ca`, all seventeen x64 Release CTests pass after configuring with
Visual Studio 17 2022 and building with `--config Release --parallel 2`.
Windows PowerShell 5.1 passes all 267 assertions in the expanded
`tests/Test-StartMenuRelocation.ps1`: read-only and read-only/hidden shortcuts,
real sharing failures, exact marker/executable rollback, retries, replacement
with identical bytes during the actual native reopen gap, and attribute recovery
after a forced native disposition failure. Install/uninstall native helpers are
also checked for parity. Against the original scripts extracted using read-only
`git show HEAD:packaging/<name>.ps1`, the same tests fail 16 assertions.
The isolated baseline copy and logs remain under ignored `build/` directories.
All fourteen suites in `tests/Test-RegressionScripts.ps1` and the public
`packaging/Test-Packaging.ps1` gate pass in Windows PowerShell 5.1 with temp/profile
fixtures confined to this worktree.

No installed app, actual Start Menu, registry or scheduled task is changed.
PowerShell 7, Windows 10/ARM64 and hosted CI are not verified for this repair.
No commit, push or PR publication is performed under this task's no-write Git
constraint; owner-account actions in `HUMAN_TODO.md` remain open.

## Rounded documentation capture corners (#50)

The capture tool now clears alpha and RGB outside rounded corners before each PNG
save, for both full windows and notification-area popups. The pure System.Drawing
helper uses capture radii of 8 DIPs for windows and 4 DIPs for popups, scaled using
the captured window's own DPI. Pixels crossed by the curve are cleared too, so
partially covered desktop pixels are not retained as hidden RGB.

The offline Windows PowerShell 5.1 regression passes 14 cases / 72,614 checks,
covering six DPIs, small-image bounds and an in-memory PNG round-trip. It verifies
3,180 cleared exterior pixels and 14,960 byte-identical interior pixels. The same
assertions fail against a helper-disabled mutation. All 13 registered suites in
`tests/Test-RegressionScripts.ps1` pass, ending with "All registered script
regressions passed." The separate offline capture-manifest/evidence suites and
the 30-check capture portability suite also pass in Windows PowerShell 5.1.
Test temp/profile paths are confined to this worktree.

The radius follows `DWMWA_WINDOW_CORNER_PREFERENCE` for the window or popup menu
(`Get-CaptureCornerRadiusDip`): a failed read (Windows 10) or `DWMWCP_DONOTROUND` leaves the
corners unmasked (Codex P2 on PR #148), `DWMWCP_ROUNDSMALL` masks 4 DIPs (#149),
`DWMWCP_ROUND` 8 DIPs, and `DWMWCP_DEFAULT` the surface default above (8 for windows, 4 for
popups). The pure decision is tested offline; the live DWM read is not exercised.

No live desktop capture or visual inspection is performed for this change.
Committed PNG assets and `docs/assets/capture-manifest.json` remain unchanged;
the next real capture applies the helper. Existing owner-account actions in
`HUMAN_TODO.md` remain open.

## Start Menu known-folder relocation (issue #37)

Per-user markers now retain the install-time Programs directory and SHA-256 of
the launcher bytes written. Update/uninstall accept the former path only when it
is exactly `IdleHarbor.lnk` in that recorded boundary, bytes and launcher properties
match, the leaf has one hard link and no path component is a reparse point.
Changed, foreign or unsafe entries are reported and preserved. Legacy markers
remain owned only at the current Programs path. Migration cleanup is the last
transaction step, after the current launcher and marker commit; earlier failures
leave the old launcher intact and restore the current path. Rollback refuses to
overwrite byte changes made after the installer wrote its launcher.

PR #146's confirmation race is repaired in both marker-removal functions: full
ownership validation now repeats after approval under a read handle that denies
writes and leaf replacement. Deletion reopens with DELETE access, compares the
volume/file ID, single-link/regular-file properties and bytes, and marks that
same handle for deletion. Sharing/access failures preserve the leaf with a warning.
Ancestor directories are not locked; identity comparison rejects redirection to
a different file during the reopen gap, and handle deletion cannot follow a later
path redirect. Crafted-marker behavior is unchanged.

Six fixture races replace a shortcut or swap its Programs ancestor for a junction
at the true return from ShouldProcess during update, update-without-launcher and
uninstall. Before the repair they fail 18 preservation assertions. Separate checks
exercise write/rename denial and rejection of a different file ID with identical bytes.
Windows PowerShell 5.1.26100.9711 passes all 226 relocation assertions, all 13
registered regression suites, and both packaging entry points. Disabling only the
new post-confirmation revalidation (retaining native identity-bound deletion)
reproduces all 18 failures across the six races. The public packaging check's
first concurrent run detects another suite's temporary transaction directory;
rerunning with its own worktree-local temp root passes. No production scripts
are modified for this mutation check, and no commit or push is performed.

The initial Windows PowerShell 5.1 relocation regression failed both intact
update/uninstall removal assertions while current-path creation passed. The
expanded lifecycle regression and packaging fixtures exercise real temporary
shortcut bytes, hashes, hard links and junctions through injected known folders.
All registered script regressions and the public packaging suite pass on Windows
PowerShell 5.1.
No installed app, actual Start Menu, Run entry or scheduled task is changed.
PowerShell 7, hosted CI and real profile redirection are not verified locally.
No commit, push or issue closure is performed; `HUMAN_TODO.md` owner gates remain unchanged.

## Chocolatey native architecture guard (PR #144 / issue #51)

The x64-only package now reads the machine-wide `PROCESSOR_ARCHITECTURE` value from
`HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment` before downloading.
Only native `AMD64` is accepted. Per-process architecture values cannot override it;
missing, unknown or unreadable registry values fail closed, even with W6432=AMD64.
This corrects the prior guard's acceptance of Prism's AMD64 process without W6432
on native ARM64. The pure decision and installer tests inject native/process values
and registry read failures without changing the registry or downloading an archive.

Windows PowerShell 5.1 passes `packaging/Test-ChocolateyPackage.ps1` (17 native/process
cases through both the pure function and installer), `tests/Test-RegressionScripts.ps1`
and `packaging/Test-Packaging.ps1`. Test temp/profile paths are confined to this worktree.
The Prism rejection assertion fails against commit `28c1d42`'s extracted guard because
it accepts that case. A real registry read on this host also passes through the installer
with only the ZIP helper doubled; no archive is downloaded or installed.

Real ARM64/Prism behaviour and registry contents under emulation remain unobserved
on hardware. PowerShell 7 and hosted CI are not verified locally for this change.
No Chocolatey install, account operation or publication is part of this repair;
issue #51 and the owner gates in `HUMAN_TODO.md` remain open.

## Smooth native navigation (2026-10-09)

The owner requests smoother scrolling, buttons and navigation, including a new
scrollbar. Precision wheel deltas now move proportionally; detents settle over
160 ms with interruption, accumulation, reversal and reduced-motion handling.
Scrolling translates the arranged body in one deferred-position batch and schedules
repainting without redoing form layout or repositioning fixed actions. A native
SCROLLBAR child keeps dragging/input/accessibility semantics with a slim themed
thumb and native high-contrast fallback. Page keys navigate settings from buttons
without taking edit/combo keys. Buffered paints and post-native-state invalidation
give action buttons immediate press/release feedback.

The baseline native fixture fails three new regressions: press invalidation,
sub-detent movement, and fixed-footer position calls (ten per ten scroll updates).
After the change those footer calls are zero. All seventeen x64 Release CTests
pass; final reversal changes pass the two scoped suites (574 compact checks).
The batched preview run passes 587 checks before the final reversal adjustment.
Repeated buffered paints preserve the GDI object count. Live help passes 33 tips
and six fitting hints. Live native thumb dragging, wheel churn and natural/reference
repainting pass at 192 DPI with hash
`1586532c8db38864202a901d510f48c204a1dbaea8bf8d30b7fa98e7a520c76c`.
The repaint fixture now discovers the named native scrollbar by enumerating its
class rather than a nullable title lookup, and reads its SB_CTL range.

The tested build is installed and reopened stopped as v0.2.0; settings bytes and
startup XML are unchanged. SHA-256:
`3d371a87353c6309928971d5ba462fc25e204f1bbee64eb4d1bd45faebb8d803`.
One batched visual inspection covers light/dark expanded and narrow settings;
one physical own-client confirmation shows the installed dark controls at 192 DPI.
Backups, test logs and captures survive outside Git in
`Documents/Codex/IdleHarbor-navigation-2026-10-09`.

Physical touchpad feel, frame timing/CPU usage, screen-reader speech, actual OS
high contrast, physical multi-monitor transitions and Windows 10/ARM64 execution
are not verified. Published packages and HUMAN_TODO owner-account gates remain
unchanged; this is an unreleased development build.

## Native UX recovery and bounded QA (2026-10-09)

The owner requests continued UX/UI improvement and Muse QA. Three isolated,
read-only Muse Spark high lenses (bug hunt, test gaps, performance) complete with
valid reports and integrity checks; all temporary lens worktrees are removed.
Their claims are verified locally rather than treated as execution evidence.

Start/Save rejection now preserves entries, expands the relevant section and
selects its numeric field for correction. ReadControls commits only a fully valid
candidate. Collapsed motion summaries flag invalid interval/size in every mode,
including Off (issue #129). Tab enters at Session duration; preset-matching custom
duration restores focus to its selector before the field disappears.

The native fixture reproduces the previous misleading summaries (12 failures),
edit/focus recovery defects (18 failures) and both keyboard defects (two failures).
It measures 100 hover invalidations for 100 within-control moves before the fix,
and one after it, retaining leave/reentry and independent state redraws. Native
status tests add emergency-stop, close/tray and pending-power-cleanup coverage.
Mutating candidate isolation and all three safety seams fails five assertions;
restoring the fixture passes. All seventeen x64 Release CTests pass; the direct
compact fixture passes 554 checks, status passes 372 assertions in 16 scenarios,
and the batched native preview run passes 566 checks before the final bound guard.
PR review catches a custom-duration group-4 bounds read in the new recovery path;
expansion is now limited to the actual three disclosure groups. Both Start and Save
custom-duration rejection retain those groups' state, with scoped tests rerun.

One visual inspection covers light validation feedback, dark compact and narrow
custom-duration states. Live help passes 33 tips/six fitting hints; the live
192-DPI repaint test passes three churn cycles. The tested executable is installed
and reopened as v0.2.0, with settings bytes and startup XML unchanged. SHA-256:
`6175b371a8a097addcea6d955527a63fd5a2559de110d8adcf0fff182b811e94`.
Backups, Muse receipts, previews and native evidence are retained outside Git at
`Documents/Codex/IdleHarbor-qa-2026-10-09`.

The larger scroll/layout batching suggestion remains an unmeasured hypothesis;
no speculative batching or cache is added. Actual OS high contrast, screen-reader
speech, physical monitor transitions and Windows 10/ARM64 execution remain
unverified. This is an unreleased development build; published packages and
HUMAN_TODO owner account gates are unchanged.

## Repair disappearing native controls (2026-10-09)

The owner reports that only the material/window remains until Snipping Tool is
activated. The previous integration extends DWM glass across the whole GDI client
and erases its root background black. PrintWindow tests deliberately disabled the
material flag, so earlier captures and DWM attribute queries did not prove normal
composed control visibility. This repair supersedes those material-proof claims.

Frame-extension margins are now always zero; the client always paints its opaque
theme surface. Optional material is limited to the native title bar, and the label,
summary and help describe that boundary. The capture-only production branch and
test bypass are removed. No policy, schema or saved preference change.

A new native regression records and forwards real DWM calls and rejects nonzero
margins. Both palette/background tests plus the margin test fail on the previous
source (three failures) and pass after repair. All seventeen x64 Release CTests
pass; the material-enabled native capture run passes 492 checks. The live help
check passes 33 tips/six fitting hints; the 192-DPI repaint test passes three cycles.

The tested executable is installed and reopened stopped as v0.2.0. SHA-256:
`47b4a52742c6397496ec28b0efd5fffe7390d40cfd808a68f8178b3a0d17ce34`.
The temporary solid-mode mitigation is removed and original settings bytes are
restored. A physical-pixel, foreground-client capture at 192 DPI (not PrintWindow
or Snipping Tool) shows visible controls and the opaque #17191f root surface.
DPI-aware coordinates are required: the first virtualized-coordinate probe was
invalid and is superseded by the corrected own-client capture. Backups and evidence
survive outside Git in `Documents/Codex/IdleHarbor-backdrop-repair-2026-10-09`.

Screen-reader speech, actual OS high contrast, physical monitor transitions and
Windows 10/ARM64 execution remain unverified. Owner confirmation of the repaired
visibility is requested, not inferred. Published packages and HUMAN_TODO gates
remain unchanged; this is an unreleased development build.

## Duration-first settings refinement (2026-10-09)

The owner asks to keep improving the premium UI. Session duration now leads the
routine choices, with exact custom seconds immediately below it when selected.
Expandable rows have a quieter second line describing motion/timing, all six
possible safety pauses and appearance preferences. Summaries live in the native
accessible names, preserve incomplete edits and distinguish randomized intervals.
Native high-contrast buttons retain multiline text. The preferred height is 500
logical pixels so the preset view still fits without scrolling.

All seventeen x64 Release CTests pass; the final direct native fixture passes
476 checks, including a real stacked-layout window, summary updates and native
control/persistence checks. Removing battery-power pauses from the count fails
the regression, and the restored build passes. Two bounded visual rounds inspect
light/dark, custom/expanded and native system-colour fallback previews. The live
help check passes 33 tips and six fitting hints; the live 192-DPI repaint test
passes three churn cycles. Actual OS high contrast, screen-reader speech, physical
monitor transitions and Windows 10/ARM64 execution remain unverified.

The tested executable is installed and reopened with the v0.2.0 title; its SHA-256
is `c9b1d07571f93766faf302f687da399eca1cbc2ce1df8c2b4bc3409c855d4154`.
Saved settings bytes and startup XML remain unchanged. Backups, the tested binary
and local evidence survive outside Git at
`Documents/Codex/IdleHarbor-refined-settings-2026-10-09`.
This remains an unreleased development build; published packages and the owner
account gates in `HUMAN_TODO.md` are unchanged.

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
closeup ordering with hidden-native regression tests. #55's sent-focus/nested-pump
repair and its native acceptance limits are described above. A genuine-input
notification carries the session generation that posted it. The window ignores a
message from an earlier session, so Stop then Start does not pause the replacement
unless that session receives its own genuine input. The hidden fixture does not
install low-level hooks.

#37's known-folder relocation repair is described above. #49/#50/#59 concern
capture quoting, corner privacy and harness cleanup. The two native scripts now
remove their temp directory through `tests/Remove-IdleHarborScopedTemp.ps1`.
The helper refuses paths outside the user temp directory or with the wrong leaf,
and it returns that refusal instead of throwing. Each script throws the refusal
only after its `try` completes, so a failed test is no longer replaced by the
path check or by a locked `settings.ini` removal error. Launch, `--exit`, and
the help script's hung-process kill stay in each script.
`tools/Capture-IdleHarborScreenshots.ps1` still has its own copy. The regression
does not launch IdleHarbor. #70's serialization issue is
addressed by #113, without claiming the other native capture gates are complete.
The #49 portability slice quotes owner configuration paths, reuses the loaded capture helper
and disables the capture-only emergency hotkey. Thirty executable checks pass in each of Windows
PowerShell 5.1 and PowerShell 7, including an actual native argv probe and repeated initialization.
See [capture validation](docs/CAPTURE-MANIFEST-VALIDATION.md). A real spaced-path capture and two
`-Force` captures in one Windows PowerShell 5.1 process remain pending; screenshots are unchanged.
Preserve their native/desktop evidence requirements rather than substituting a
source check or a passing headless build.

A repeated settings key keeps the later value and adds one recovery warning. A field that is
not an integer warns as non-numeric rather than out of range. Both warnings use the existing
recovery path, so automatic start stays stopped until the file is reviewed and saved. A
settings path that exists or cannot be inspected is a recovery warning, not a first run. A
path that is not there still returns defaults with no warning. Access-denied and sharing
failures use that same open-failure path and are not separately provoked here.

Owner-only work remains unchanged: Chocolatey account/API-key publication,
Google Search Console operations and the social-preview Settings upload (#6).
No credentials belong in the repository or logs. No new hosting activation,
marketplace publication or native screenshot recapture is authorized by this record.
