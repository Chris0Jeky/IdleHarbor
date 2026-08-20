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

### Changed

- Documentation now describes the landed integration branch rather than the original foundation stub.

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
- Restore managed files, marker bytes, and owned startup state after failed updates; preserve
  pre-existing scheduler folders; and clean up installer-created empty task folders.
- Reject linked managed files before an update can write outside its ownership boundary, and make
  packaging/checksum verification pass under both Windows PowerShell 5.1 and PowerShell 7.

### Release boundary

- No version has been published. Download links, signing status, and downstream licence rights remain intentionally unclaimed.

## [0.1.0] - Unreleased

This heading reserves the first release entry. It must not be dated or described as released until
the tagged artifacts, release checks, checksums, SBOMs, attestations, signing decision, and licence
decision are verified.
