# Changelog

All notable user-visible changes are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow Semantic Versioning
where practical.

## [Unreleased]

### Added

- Native visible Win32 window and notification-area controls with explicit Start, Stop, Show, and Exit paths.
- Motion modes: Off, Normal, Zen, Circle, and Linear.
- Named Balanced, Long Task, Presentation, Compatibility, Visible, Battery Saver, and Custom profiles.
- Validated local INI settings with portable and explicit-config paths.
- Genuine-input, lock/session, battery, fullscreen, active-hours, maximum-duration, and emergency-stop controls.
- Strict command-line parsing with visible help, version, status, session commands, and bounded options.
- Per-user installer, ownership marker, WhatIf preview, uninstall, and opt-in Task Scheduler/Startup-folder/HKCU Run choices.
- CI build matrix, pinned actions, CodeQL workflow, portable release packaging, checksums, SPDX SBOMs, and GitHub attestations.
- Per-monitor-DPI-aware resize and scrolling that keeps every setting and action keyboard reachable.
- Fixed the live status and immediate Start/Stop actions outside the scrolling settings body; narrow
  work areas now reflow settings and wrap or stack actions, and focus reveal no longer undoes pointer
  or scrollbar scrolling when focus is unchanged.
- Confined the native scrollbar track to the settings viewport, aligned Tab and Shift+Tab with visual
  order (including Save before fixed Start/Stop), transferred keyboard focus to the newly enabled
  Start/Stop action after session changes, re-revealed a still-focused setting after resize or DPI
  reflow, and adapted stacked controls to unusually narrow work areas.
- Added privacy-safe exact-build Running, Paused, full-window, compact-viewport, and tray captures;
  refreshed the social preview footer for stronger contrast and recorded the final x64 resource
  baseline.
- Refreshed the portfolio evidence against merged main after the inherited-scrollbar resize fix;
  the social-preview upload remains an explicit pre-merge publication gate.

### Changed

- Documentation now describes the landed integration branch rather than the original foundation stub.
- Motion distance now matches the upstream Mouse Jiggler multiplier semantics and canonical Normal,
  Circle, and Linear patterns while preserving safe-anchor restoration.

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

### Release boundary

- No version has been published. Download links, signing status, and downstream licence rights remain intentionally unclaimed.

## [0.1.0] - Unreleased

This heading reserves the first release entry. It must not be dated or described as released until
the tagged artifacts, release checks, checksums, SBOMs, attestations, signing decision, and licence
decision are verified.
