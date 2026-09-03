# Changelog

All notable user-visible changes are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow Semantic Versioning
where practical.

## [Unreleased]

### Distribution

- The project has a site at <https://chris0jeky.github.io/IdleHarbor/>, published from `docs/`. It
  describes the motion modes, the safeguards, and the download and verification steps, and is linked
  from the README and the user guide.
- The project site is now mirrored on Cloudflare Workers at
  <https://idleharbor.commit-atlas.workers.dev>, which adds a `/download` route that always resolves
  to the current release. <https://chris0jeky.github.io/IdleHarbor/> remains the canonical site and
  is unchanged; the mirror exists so the site can serve a root `robots.txt` and response headers,
  which a GitHub Pages subdirectory cannot.

## [0.2.0] - 2026-08-24

### Added

- Each settings field now carries a short explanation printed under it, so the window can be read
  straight through without hovering anything.
- Every settings control also explains itself in more detail on hover, including the labels, the
  Start, Stop, and Save actions, and the status card. Hovering a label gives the same description as the field beside
  it. During a running session the settings are disabled and Windows does not deliver hover to a
  disabled control, so only the labels, the status card, and Stop still describe themselves.

### Changed

- The maximum session duration moved from "Window & notifications" to "Safeguards". It is a session
  setting that a profile replaces, unlike everything else under the former heading.
- Renamed four labels for what they do rather than how: "Power request" is now "Keep awake",
  "Motion multiplier" is now "Motion size", "Pause after genuine input" is now "Pause after real
  input", and the low-battery threshold now names its unit. The out-of-range message for the motion
  size was still calling it a multiplier and now matches the label.

### Fixed

- Selecting a profile, motion mode, or power request now updates the field immediately instead of
  appearing to keep its previous value until focus moved elsewhere.
- Clicking a combo box no longer scrolls the settings body out from under the pointer, so a mode can
  be picked on the first attempt. The body holds still while any drop-down list is open.
- Scrolling the settings body moves its controls as one surface instead of repositioning them one at
  a time.
- The status card now reports unsaved changes before the Save action is relabelled or enabled, instead
  of briefly showing an available Save action beside a saved-looking status.
- Dismissing the profile list with Escape, or re-selecting the profile already in effect, no longer
  reloads that profile's defaults over settings you had edited.

### Distribution

- Published under `GPL-3.0-only` as unsigned x64 and ARM64 portable archives accompanied by
  SHA-256 checksums, per-architecture SPDX SBOMs, and GitHub artifact attestations, on the same
  terms as `0.1.0`.

## [0.1.0] - 2026-08-20

### Added

- GNU General Public License version 3 only (`GPL-3.0-only`), with the complete licence and a
  transparent implementation-provenance notice included in source and portable distributions.
- Ownership-safe per-user Start Menu launcher creation (enabled by default) with independent
  `-StartMenu Create|None` control, exact-link preflight, marker ownership, and transactional byte
  rollback; automatic startup remains independently disabled by default.
- Native visible Win32 window and notification-area controls with explicit Start, Stop, Show, and Exit paths.
- Motion modes: Off, Normal, Zen, Circle, and Linear.
- Named Balanced, Long Task, Presentation, Compatibility, Visible, Battery Saver, and Custom profiles.
- Validated local INI settings with portable and explicit-config paths.
- Genuine-input, lock/session, battery, fullscreen, active-hours, maximum-duration, and emergency-stop controls.
- Strict command-line parsing with visible help, version, status, session commands, and bounded options.
- Per-user installer, ownership marker, WhatIf preview, uninstall, and opt-in Task Scheduler/Startup-folder/HKCU Run choices.
- CI build matrix, pinned actions, CodeQL workflow, portable release packaging, checksums, SPDX SBOMs, and GitHub attestations.
- Per-monitor-DPI-aware resize and scrolling that keeps every setting and action keyboard reachable.
- Reproducible 192-DPI documentation captures for stopped, running, intelligently paused, scrolled,
  and notification-area states, with exact executable and image hashes in a machine-readable manifest.
- Fixed the live status and immediate Start/Stop actions outside the scrolling settings body; narrow
  work areas now reflow settings and wrap or stack actions, and focus reveal no longer undoes pointer
  or scrollbar scrolling when focus is unchanged.
- Polished the native interface with themed common controls, clearer section hierarchy, a dedicated
  status card, and a fixed Start/Stop/Save footer that remains visible while settings scroll.
- Confined the native scrollbar track to the settings viewport, aligned Tab and Shift+Tab with visual
  order (including the fixed footer's Start, Stop, Save sequence), transferred keyboard focus to the newly enabled
  Start/Stop action after session changes, re-revealed a still-focused setting after resize or DPI
  reflow, and adapted stacked controls to unusually narrow work areas.

### Changed

- Documentation now describes the implemented first stable release rather than the original foundation stub.
- Motion distance remains a multiplier, while Normal, Circle, and Linear now use independently
  designed IdleHarbor paths that preserve bounded safe-anchor restoration.

### Fixed

- Stop safely if the requested genuine-input observer cannot be refreshed, and bound its watchdog
  cadence to avoid unnecessary hook churn.
- Recover a lost notification-area icon or keep the settings window visible.
- Preserve unowned settings directories during `-PurgeData` and validate the least-privilege Task
  Scheduler principal on a non-mutating packaging test path.
- Refuse to overwrite foreign scheduled tasks, Startup-folder shortcuts, or HKCU Run values that
  happen to use the IdleHarbor name.
- Report partial input cleanup failures and keep forwarded status/minimize commands bounded and visible.
- Establish the current lock and disconnect state before starting, and fail closed when Windows
  cannot establish requested battery or session safeguards.
- Preserve and display every settings-recovery warning, keep the window visible and stopped, and
  block automatic starts until recovered values are reviewed and saved.
- Defer every forwarded command outside `WM_COPYDATA`, preflight startup ownership before installer
  mutations, and create the settings ownership marker before writing user data.
- Reject release tags that disagree with embedded versions, emit the required SPDX SHA-1 file and
  package-verification values, and keep documented INI examples directly copyable.
- Route wheel input over child controls into the settings viewport, retain precision-wheel partial
  deltas, follow Windows wheel preferences, and preserve native behavior for open combo lists.
- Roll back fresh-install files on failure, validate the application manifest version, require a
  tracked licence before publication, and document release-directory trust assets.
- Keep the v0.1.0 publication lane stable-only by rejecting prerelease and build-metadata tags
  consistently in both workflow and source-version validation.
- Pass the triggering release tag through step-local environment data so PowerShell packaging and
  GitHub release publication do not interpolate the tag directly into run scripts.
- Restore managed files, marker bytes, and owned startup state after failed updates; preserve
  pre-existing scheduler folders; and clean up installer-created empty task folders.
- Reject linked managed files before an update can write outside its ownership boundary, and make
  packaging/checksum verification pass under both Windows PowerShell 5.1 and PowerShell 7.
- Retain an exact-path transaction recovery backup when managed-file rollback is incomplete, clean
  it up after a complete rollback, and reject first-time same-directory installs without a valid
  ownership marker.
- Keep a redundant Stop request from moving focus when no session transition occurred.
- Size the settings body from the scroll viewport's effective client width, including wider native
  scrollbars, so stacked and fill-width controls remain within the visible viewport.
- Recompute and republish the settings scroll range when scrollbar appearance changes the effective
  viewport layout, keeping the bottom controls reachable after a breakpoint reflow.
- Re-evaluate a scrollbar-free viewport candidate after height-only resizes so an inherited vertical
  scrollbar does not keep a tall, column-fit window in the narrow stacked layout.
- Preserve the requested scroll position through re-entrant scrollbar/layout probing and clamp it
  only after the final stable viewport state is known.
- Serialize concurrent PowerShell packaging suites with a bounded, abandoned-owner-safe test lock
  while preserving transaction-residue assertions.
- Use in-process .NET hashing for SBOM and checksum generation so concurrent Windows PowerShell
  5.1 and PowerShell 7 packaging runs do not depend on command auto-loading.
- Repaint the settings viewport and all descendant controls after scrolling, layout convergence,
  and Running/Stopped enabled-state changes so stale control fragments do not survive until resize.
- Clear newly exposed parent and footer pixels after viewport resize so clipped settings never
  obscure the fixed Start/Stop/Save actions.
- Mark profile defaults and recovered settings as unsaved until explicitly saved, expose that state
  to accessibility APIs, and keep all three footer actions separated at narrow high-DPI sizes.
- Keep first-owner and forwarded command-line overrides consistently available for explicit saving
  without silently changing the persisted INI baseline.
- Preserve the unsaved-state prefix in notification-area tooltips and accessible status text while
  recovering from a missing tray icon.
- Resolve the release-version source root after PowerShell parameter binding so the default release
  checks run under both Windows PowerShell 5.1 and PowerShell 7.
- Keep the packaging test's deterministic scheduled-task shim isolated from the real per-user
  ScheduledTasks module and any installed IdleHarbor task.

### Distribution

- Published under `GPL-3.0-only` as unsigned x64 and ARM64 portable archives
  accompanied by SHA-256 checksums, per-architecture SPDX SBOMs, and GitHub artifact attestations.

[Unreleased]: https://github.com/Chris0Jeky/IdleHarbor/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v0.2.0
[0.1.0]: https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v0.1.0
