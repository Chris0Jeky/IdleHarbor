# IdleHarbor

> A tiny native Windows utility for transparent, configurable idle prevention.

[![Status: pre-release](https://img.shields.io/badge/status-pre--release-orange.svg)](PROJECT_STATE.md)
[![Platform: Windows](https://img.shields.io/badge/platform-Windows-0078d4.svg)](docs/USER_GUIDE.md)

IdleHarbor is an independent C++20/Win32 application for keeping a legitimate, long-running
work session active without shipping a managed runtime. It is designed to be small, observable,
and easy to stop. It is not a concealment tool and does not promise to defeat monitoring or
security controls.

![IdleHarbor mark](docs/assets/idleharbor-mark.svg)

## Project status

This repository is in active pre-release development. There is no v0.1.0 release or download
package yet.

| Area | Current repository | v0.1.0 release target |
| --- | --- | --- |
| Native build | CMake foundation, C++20, Unicode Win32 executable | Keep the dependency-free x64/ARM64 build |
| Application | Foundation message box only | Tray application with visible running/paused/stopped state |
| Idle prevention | Not implemented | User-controlled Normal, Zen, Circle, and Linear modes |
| Safeguards | Not implemented | Real-input pause, lock/session handling, time limits, and emergency stop |
| Configuration | Version metadata only | Persisted settings with validation and reset-to-defaults |
| Distribution | Source only | Portable archives, checksums, and package-manager metadata when verified |
| Automation | None | Opt-in per-user Task Scheduler helper scripts |

The authoritative milestone and proving commands are in [`PROJECT_STATE.md`](PROJECT_STATE.md).
Planned behavior is deliberately labelled as a target throughout the documentation; it should
not be read as a claim that the current executable already provides it.

## Why IdleHarbor?

- **Native footprint:** use Windows system libraries rather than bundling a managed runtime.
- **Observable by design:** the window, notification-area state, logs, and stop controls are
  intended to make the program's behavior clear.
- **Conservative safeguards:** real user input, session changes, power policy, and time limits
  are first-class controls rather than afterthoughts.
- **Portable delivery:** the release target is a small, architecture-specific executable with
  reproducible build and checksum evidence.

## Build the current foundation

The current project is Windows-only and expects a Visual Studio developer PowerShell with CMake
and Ninja available:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The application currently displays a foundation message and exits. Keep-awake behavior is not
implemented in this checkout. Release downloads, installer commands, and scheduler commands will
be added only when their artifacts and scripts have been built and verified.

## Planned v0.1.0 experience

The following is the agreed release shape, not a list of currently landed features:

| Capability | Intended behavior |
| --- | --- |
| Normal | Small, visible diagonal movement when a session is active |
| Zen | Virtual input intended to refresh compatible Windows idle state without moving the pointer |
| Circle | Small circular movement pattern |
| Linear | Horizontal back-and-forth movement |
| Interval | Configurable interval with an optional bounded random interval |
| Distance | Validated movement multiplier with a safe default |
| Intelligent stop | Pause for real user input; stop or pause on lock/session change; optional end time and maximum duration |
| Power policy | User-selectable behavior for AC and battery, with conservative defaults |
| Visibility | Window/tray state, explicit start/stop, pause reason, and emergency hotkey |
| Launching | Normal launch plus an opt-in, per-user Task Scheduler helper with a matching uninstall path |
| CLI | Validated startup options and `--help`/`--version` output once the command-line model lands |

No feature will be implemented to hide the process, misrepresent a user's presence, bypass an
employer's controls, or defeat security software.

## Documentation

- [User guide](docs/USER_GUIDE.md) — planned controls, modes, safeguards, and launch paths.
- [Architecture](docs/ARCHITECTURE.md) — current foundation and intended native design.
- [Safety and acceptable use](docs/SAFETY.md) — boundaries, privacy, and trust assumptions.
- [Troubleshooting](docs/TROUBLESHOOTING.md) — build and runtime diagnosis.
- [Benchmark methodology](docs/BENCHMARKS.md) — how footprint and idle cost will be measured.
- [Contributing](CONTRIBUTING.md) — development workflow and quality bar.
- [Security policy](SECURITY.md) — responsible vulnerability reporting.
- [Changelog](CHANGELOG.md) — user-visible changes by release.

## Distribution and trust

The first release will not claim a download location until a verified artifact exists. A complete
release should include architecture labels, SHA-256 checksums, build provenance, and clear signing
status. See the [human decisions](HUMAN_TODO.md) for the licence and optional Authenticode signing
choices that cannot be inferred by an agent.

IdleHarbor is intended for legitimate personal workflows such as long installations, local
presentations, or dashboards where Windows idle behavior is an inconvenience. On a managed device,
read and follow the applicable policy before installing or running it. Simulated input may be
blocked, logged, or interpreted differently by other software; it is never proof that a person is
present.

## Roadmap

1. Land and test the policy engine, motion patterns, and command-line model.
2. Add the visible tray UI, persisted configuration, and intelligent-stop signals.
3. Add Windows smoke tests, performance measurements, and packaging scripts.
4. Publish a reviewed v0.1.0 only after artifacts, checksums, and release notes are verified.

## Licence

The licence decision is still open. See [`HUMAN_TODO.md`](HUMAN_TODO.md); do not assume that this
pre-release repository grants downstream rights until a licence is committed.
