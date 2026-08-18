# User guide

IdleHarbor is pre-release. This guide documents the intended v0.1.0 experience; the current
foundation executable only displays a status message. Do not use the commands below as if a
release package already exists.

## First run (planned)

1. Launch the architecture-matched executable.
2. Confirm the visible status is **Stopped**.
3. Choose a mode and conservative interval.
4. Review intelligent-stop and power settings.
5. Select **Start** only for the work session that needs it.
6. Use **Stop** or the emergency hotkey as soon as the session no longer needs idle prevention.

The application should make the active state and pause reason visible in both its window and
notification-area menu. Minimize-to-tray should reduce clutter, not hide the program's identity or
status.

## Modes (planned)

| Mode | Pointer behavior | Best fit |
| --- | --- | --- |
| Normal | Small, visible diagonal movement | Compatibility with applications that require visible movement |
| Zen | Virtual input with no intended visible movement | Windows idle state where visible movement is undesirable |
| Circle | Small circular path | A distinct, bounded visible pattern |
| Linear | Horizontal back-and-forth path | A simple visible pattern |

Applications implement idle detection differently. Zen is not guaranteed to work everywhere, and
visible movement is not proof that another application will accept the input.

## Intelligent stopping (planned)

The release target uses conservative, user-controlled safeguards:

- pause when real mouse or keyboard activity is detected;
- pause or stop on lock, unlock, session switch, or remote-session changes where Windows exposes a
  reliable signal;
- optionally stop at a chosen end time or after a maximum duration;
- optionally pause on battery or when the display/power policy changes;
- show the pause reason and resume condition;
- provide an immediate stop hotkey and a visible menu action.

Safeguards should be tested in the user's actual Windows session before relying on them. They do
not replace workplace policy or application-specific idle settings.

## Launching and startup (planned)

Normal launch is the default. A per-user Task Scheduler helper may be provided for users who
explicitly want start-at-sign-in behavior. The helper must be:

- opt-in and explain exactly what it creates;
- non-elevated by default;
- paired with an uninstall script;
- safe to run repeatedly (idempotent);
- verifiable with a dry-run and a status command.

There is no planned hidden startup mechanism.

## Command line and configuration (planned)

The command-line model is not yet landed. Once implemented, `--help` and `--version` will be
available, invalid values will fail with a useful message, and startup flags will be distinct from
persisted settings. Documentation will list the exact switches from the shipped binary rather than
guessing them in advance.

## Stop immediately

Until the emergency hotkey lands, close the foundation application normally. In the target UI,
**Stop** must always be available from the main window and tray menu; the hotkey is an additional
escape path, not the only one.

For boundary, privacy, and managed-device guidance, read [SAFETY.md](SAFETY.md). For failures,
read [TROUBLESHOOTING.md](TROUBLESHOOTING.md).
