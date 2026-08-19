# Troubleshooting

IdleHarbor is pre-release. Record the commit or package version, Windows edition/build, architecture,
selected profile, relevant settings, and the smallest reproducible sequence. Remove credentials,
private work data, and managed-device details before sharing diagnostics.

## Build and test

Use a Windows developer PowerShell with Visual Studio 2022:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\packaging\Test-Packaging.ps1
```

The project is Windows-only. CI additionally builds ARM64 and Win32; release packaging currently
targets x64 and ARM64.

## The application does not appear

- Start without `--minimized` and confirm the process is not already running.
- If a previous instance owns the single-instance mutex, use `IdleHarbor.exe --show`.
- If Explorer has restarted, the tray icon may not be present; use `--show` or restart the owner.
- Check that the executable is not blocked by endpoint policy. Do not weaken security controls to
  force an unverified binary to run.

## It does not keep the intended state active

Read the visible pause reason first. Check genuine-input cooldown, lock/disconnect state, battery
threshold, fullscreen policy, active hours, and maximum duration. Motion and power are separate:

- `motion=off` with `power=system` or `power=display` uses a Windows power request without pointer movement;
- `motion=normal`, `circle`, or `linear` with `power=none` relies on visible input for applications
  with their own idle detection;
- `motion=zen` uses marked virtual input and is not guaranteed to work everywhere.

If a power request or input pulse fails, IdleHarbor should show a stopped error. The failure may be
caused by Windows integrity boundaries, endpoint policy, or an application that ignores injected
input.

## Genuine input and session safeguards

The input observer uses Windows low-level hooks and ignores IdleHarbor-marked input. A hook may be
unavailable in a restricted environment; the status identifies observer availability. Lock and
connect/disconnect handling depends on Windows session notifications. Test these transitions in a
non-critical session before relying on them.

## Settings and INI

Normal settings live under the user's local application-data directory. Portable mode stores
`IdleHarbor.ini` beside the executable; `--config PATH` takes precedence at owner-instance launch.
Settings are validated, unknown keys are ignored, and writes use a temporary file followed by an
atomic replacement. If an INI value is invalid, restore the profile defaults or remove the offending
key and relaunch.

For active hours, use minutes from midnight:

```ini
active_hours_enabled=true
active_hours_start_minute=540
active_hours_end_minute=1080
```

## Installation and startup

Run the installer with `-WhatIf` first. Startup is disabled by default; if enabled, the installer
creates only the selected per-user Task Scheduler, Startup-folder, or HKCU Run entry. A matching
uninstaller removes entries and files only when its ownership marker proves they belong to
IdleHarbor. Settings are preserved unless `-PurgeData` is explicitly supplied.

## Report a bug

Use the repository bug form with diagnostics and reproduction steps. Security concerns belong in the
private advisory flow in [`SECURITY.md`](../SECURITY.md), not a public issue.
