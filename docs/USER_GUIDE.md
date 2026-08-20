# User guide

IdleHarbor is a visible, user-controlled Windows utility. When the
[`v0.1.0` release page](https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v0.1.0) is available,
download its architecture-matched archive and check the published checksum and provenance. If the
page is unavailable, no verified release artifact exists yet; build from source instead.

## First run

1. Launch the architecture-matched executable.
2. Confirm the main window or tray state says **Stopped**.
3. Select a profile, or choose the motion and power modes independently.
4. Review genuine-input, lock/session, battery, fullscreen, active-hours, and maximum-duration
   safeguards.
5. Press **Save** if the settings should persist, then **Start** for the intended work session.
6. Use the visible **Stop** button, tray menu, or emergency hotkey when finished.

The status text and tray tooltip identify Running, Paused, and Stopped states. Pause reasons are
shown so a user can understand why a pulse is not being emitted.

## Window, scaling, and keyboard navigation

The settings window is per-monitor-DPI aware, resizable, and kept inside the current monitor's work
area. The live Running/Paused/Stopped status and Start/Stop/Save actions remain in fixed safety
regions while the settings body uses a native vertical scrollbar whose track is confined to that
body. On narrow or highly scaled work areas, settings reflow into a single column and the action row
wraps or stacks so every control stays visible and keyboard reachable, including unusually tiny work
areas.

- Turn the mouse wheel over the window or any closed control to move the settings viewport.
- An open profile, motion, or power list keeps native wheel handling so its choices can be browsed.
- Precision touchpads and high-resolution wheels retain partial input until a complete wheel step is
  reached; IdleHarbor also follows the Windows wheel-lines/page setting.
- Use **Tab** and **Shift+Tab** to traverse controls in visual top-to-bottom order. The fixed action
  footer follows its visible **Start**, **Stop**, **Save** order. The body viewport automatically
  reveals the focused setting when focus changes or a resize/DPI reflow clips the still-focused
  setting; scrolling with the pointer or scrollbar does not snap back. Starting or stopping from the
  keyboard transfers focus to the newly enabled opposite action, keeping the immediate control path
  intact.

Moving the window between monitors reapplies the destination DPI and constrains the suggested size
to that monitor's usable work area.

## Motion and power are separate controls

Motion determines whether and how IdleHarbor emits input. Power determines whether Windows receives
an execution-state request. They are independent:

| Need | Suggested setting |
| --- | --- |
| Keep Windows available without moving the pointer | Motion **Off**, power **System** or **Display** |
| Support an application with its own idle detector | Motion **Normal**, power **None** |
| Prefer a quiet compatibility attempt | Motion **Zen**, power **None** |
| Make movement obvious | Motion **Circle** or **Linear**, power as required |

An application may implement idle detection differently. Zen is not guaranteed to work everywhere,
and visible movement is not evidence of human presence.

For visible modes, the `--distance` setting is a multiplier from **1** to **120**, not a raw pixel
radius. IdleHarbor uses independently designed bounded motion paths and translates them into
cumulative safe-anchor points that finish at the captured pointer position.

## Profiles

Profiles are starting points, not hidden modes. Selecting one loads editable values; press **Save**
to persist the resulting settings.

| Profile | Defaults |
| --- | --- |
| Balanced | Zen, system power, 60-second interval, user/lock/disconnect/low-battery safeguards |
| Long task | Motion Off, system power, 120 seconds, four-hour maximum duration |
| Presentation | Motion Off, display-and-system power, genuine-input pause disabled |
| Compatibility | Normal, no power request, 60 seconds |
| Visible | Circle, no power request |
| Battery saver | Zen, no power request, randomized 30–120 seconds, 30% low-battery threshold |
| Custom | Balanced starting values for explicit edits |

## Intelligent stopping and safeguards

The policy engine can pause or stop for:

- genuine mouse or keyboard input, followed by a configurable quiet cooldown;
- workstation lock/unlock and local or remote session connect/disconnect;
- low battery or any battery power when that option is enabled;
- a foreground window covering its monitor in fullscreen mode;
- an active-hours window in the advanced settings;
- a configured maximum session duration;
- an explicit Stop action or emergency hotkey.

These signals are compatibility safeguards, not security guarantees. Test the behavior in the actual
Windows session where the utility will be used.

## Command line

The executable accepts one command and validated options. The GUI subsystem shows help, version, and
status in visible dialogs rather than a console stream.

### Commands

| Option | Effect |
| --- | --- |
| `--start`, `--jiggle`, `-j` | Start a session |
| `--stop` | Stop the current session |
| `--toggle` | Toggle running/stopped |
| `--status` | Show the current state in a dialog |
| `--show`, `--settings`, `-g` | Show the settings window |
| `--exit` | Stop and exit the running instance |
| `--help`, `-h`, `-?` | Show validated usage |
| `--version` | Show product/version information |

### Options

`--profile` accepts `balanced`, `long-task`, `presentation`, `compatibility`, `visible`,
`battery-saver`, or `custom`. `--motion`/`--mode` accepts `off`, `zen`, `diagonal`, `linear`, or
`circle`; `--power` accepts `none`, `system`, or `display`.

`--interval`, `--distance` (the motion multiplier, 1–120), `--random`, `--no-random`, `--pause-on-input`, `--stop-after`,
`--battery-threshold`, `--pause-on-fullscreen`, and `--no-pause-on-fullscreen` control session
behavior. `--minimized` starts hidden in the notification area. `--close-to-tray` and
`--no-close-to-tray` control close behavior. `--portable` and `--config PATH` choose storage at
owner-instance launch.

Profile and settings options are runtime overrides. They update the owner window and are marked as
unsaved, but they do not silently rewrite the persisted INI file. Use the visible **Save** action if
the resulting values should become the next launch defaults. This is true for options on the first
owner invocation and for options forwarded by a later invocation.

Run `IdleHarbor.exe --help` for the exact current syntax and ranges.

## Advanced INI settings

Settings are validated and written atomically. Portable mode stores `IdleHarbor.ini` beside the
executable. Normal mode stores `settings.ini` below the user's local application-data directory.
An explicit `--config PATH` takes precedence over both.

If an existing settings file contains malformed, out-of-range, unsupported, or mutually invalid
values, IdleHarbor keeps safe profile defaults and shows every recovery warning. The settings
window stays visible and **Stopped**, and automatic `--start` or stopped `--toggle` commands are
blocked until the values have been reviewed. Correct the fields and press **Save** before relying
on automatic startup; an explicit Start action remains user-controlled.

The active-hours keys use minutes from midnight and support an overnight window when the end is
earlier than the start:

```ini
active_hours_enabled=true
active_hours_start_minute=540
active_hours_end_minute=1080
```

Other persisted keys include `profile`, `motion`, `power`, `interval_seconds`,
`random_minimum_seconds`, `distance` (the motion multiplier, 1–120), `randomize`, `pause_on_user_activity`,
`user_activity_cooldown_seconds`, `pause_when_locked`, `pause_when_disconnected`, `pause_on_battery`,
`pause_on_low_battery`, `low_battery_threshold`, `pause_when_fullscreen`, `max_duration_seconds`,
`start_minimized`, `close_to_tray`, `show_notifications`, and `emergency_hotkey`.

## Installation and startup

After downloading and checksum-verifying the architecture-matched release ZIP, extract it. The
recommended installation keeps automatic startup off while creating the default per-user Start
Menu launcher:

```powershell
.\install.ps1 -Startup None
```

Installation creates a per-user Start Menu launcher by default at `Start Menu\Programs\IdleHarbor.lnk`.
It opens the installed application with `--show`; use `-StartMenu None` to omit or remove the
installer-owned launcher. This setting is independent of automatic startup: startup is opt-in.
Supported automatic-startup choices are `TaskScheduler`, `StartupFolder`, `RunKey`, and `None`:

```powershell
.\install.ps1 -Startup TaskScheduler
```

The Task Scheduler option is the recommended least-privilege choice. It creates a per-user
interactive task that starts `--start --minimized`. `-WhatIf` previews changes; `uninstall.ps1`
removes only entries and files proven to belong to IdleHarbor. Failed install/update operations
restore managed files, startup state, and exact prior Start Menu shortcut bytes, while unexpected
files and pre-existing scheduler folders are preserved. See [`packaging/README.md`](../packaging/README.md).

Running the executable directly from the extracted archive is also supported and creates no startup
entry. On a managed laptop, do not enable Task Scheduler, Startup-folder, or Run-key startup unless
device policy permits both the application and the chosen persistence mechanism. Do not disable or
independently whitelist around endpoint controls.

## Visibility and stopping

Minimize-to-tray reduces window clutter; it is not concealment. The tray menu provides Show, Start
or Stop, and Exit. The emergency hotkey is an additional stop path, not a replacement for the
visible controls. A tray icon may be unavailable while Windows Explorer is restarting; use
`--show` if needed.

## Limitations

- IdleHarbor is Windows-only and has no managed application runtime dependency.
- Simulated input may be blocked or handled differently by applications and integrity boundaries.
- Low-level input hooks and session notifications depend on Windows policy and availability.
- Display scaling and monitor layouts vary; verify the viewport after changing a monitor's scale.
- `v0.1.0` is unsigned; validate the published checksum and attestation and expect Windows to report
  no Authenticode publisher identity.
