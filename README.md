# IdleHarbor

> Transparent, configurable idle prevention for Windows, built as a small native application.

[![CI](https://github.com/Chris0Jeky/IdleHarbor/actions/workflows/ci.yml/badge.svg)](https://github.com/Chris0Jeky/IdleHarbor/actions/workflows/ci.yml)
[![CodeQL](https://github.com/Chris0Jeky/IdleHarbor/actions/workflows/codeql.yml/badge.svg)](https://github.com/Chris0Jeky/IdleHarbor/actions/workflows/codeql.yml)

![IdleHarbor social preview](docs/assets/idleharbor-social.svg)

## Genuine UI

<p align="center">
  <img src="docs/assets/idleharbor-window.png"
       alt="IdleHarbor settings window at 200 percent Windows display scaling"
       width="594">
</p>

This is a real Release-QA capture at 200% Windows display scaling, not a mock-up.

<p align="center">
  <img src="docs/assets/idleharbor-viewport.png"
       alt="IdleHarbor resized at 200 percent scaling with its native vertical scrollbar"
       width="600">
</p>

The compact capture shows the same native window resized to a short work area. Wheel, scrollbar,
and keyboard focus paths keep every control reachable.

IdleHarbor is an independent C++20/Win32 utility for legitimate long-running work sessions,
presentations, installations, and local dashboards. It combines optional motion input with Windows
power requests, visible controls, conservative safeguards, and an immediate stop path. It does not
provide concealment, monitoring bypasses, or claims of undetectability.

## Status

The project is a pre-release `0.1.0` release candidate. The visible runtime, policy core, settings
store, CLI, portable packaging scripts, per-user installer, startup choices, CI, CodeQL, SBOM, and
attestation workflow are present on the integration branch. No GitHub release artifact has been
published yet.

| Area | Landed now | Release boundary |
| --- | --- | --- |
| Runtime | Native Win32 window and notification-area controls | Windows release artifacts must pass release QA |
| Modes | Off, Normal, Zen, Circle, Linear | Behavior remains subject to application and Windows compatibility |
| Safeguards | Genuine-input pause, lock/disconnect, battery, fullscreen, active hours, max duration | Users must verify behavior in their own session |
| Configuration | Validated local INI settings and profiles | No cloud sync or telemetry |
| Distribution | Portable archive and per-user installation scripts | No download link until a tagged release exists |
| Trust evidence | CI, CodeQL, SHA-256, SPDX SBOM, GitHub attestations are wired into workflows | Signing and licence decisions remain human-owned |

Read [`PROJECT_STATE.md`](PROJECT_STATE.md) for the current milestone and proving commands.

## Why IdleHarbor?

- **Native footprint:** C++20 and Windows system libraries, without bundling an application runtime.
- **Visible by design:** the window, tray state, status reason, settings, and stop controls remain
  available to the user.
- **Viewport-safe:** per-monitor DPI scaling, resize, native scrolling, and focus reveal keep every
  control reachable on compact or highly scaled displays.
- **Two complementary mechanisms:** motion modes address applications that observe input; power
  requests address Windows idle transitions. They can be configured independently.
- **Conservative automation:** startup is opt-in, per-user, least-privilege, and paired with an
  ownership-aware uninstall path.
- **Evidence-led delivery:** builds, tests, CodeQL, checksums, SBOMs, and attestations are part of
  the release workflow rather than marketing claims.

An earlier release-candidate x64 build measured 493,056 bytes (481.5 KiB); current release artifacts
will be measured again before publication. The reproducible three-run local resource baseline and
its limitations are recorded in [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md).

## Build and test from source

Use a Windows developer PowerShell with Visual Studio 2019 or newer and CMake. This machine's
validated Visual Studio 2019 Build Tools command is:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 16 2019" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
```

With Visual Studio 2022, use generator `Visual Studio 17 2022` instead.

The project is Windows-only. CI also builds ARM64 and Win32; release packaging currently produces
x64 and ARM64 archives. The packaging parser and ownership tests can be run independently:

```powershell
.\packaging\Test-Packaging.ps1
```

## Runtime at a glance

1. Launch IdleHarbor and confirm the visible state is **Stopped**.
2. Choose a profile or configure motion, power, and safeguard settings.
3. Press **Start** for the specific session that needs idle prevention.
4. Watch the status reason; genuine input, lock/session changes, battery policy, fullscreen policy,
   active hours, and maximum duration can pause or stop the session.
5. Press **Stop**, use the tray menu, or use the emergency hotkey when finished.

IdleHarbor stores local settings only. It has no network service or telemetry path.

## Modes and profiles

Motion modes are selectable independently of power requests:

| Motion | Behavior |
| --- | --- |
| Off | No pointer/input pulse; useful with a power request |
| Normal | Small visible diagonal path, scaled by the distance multiplier |
| Zen | Virtual mouse input intended not to move the visible pointer |
| Circle | Bounded circular path, scaled by the distance multiplier |
| Linear | Horizontal back-and-forth path, scaled by the distance multiplier |

The distance setting is a multiplier from **1** to **120**, not a raw pixel radius. Visible motion
uses the canonical Mouse Jiggler pattern deltas translated into cumulative safe-anchor points; the
reference is [`JigglePatterns.cs`](https://github.com/arkane-systems/mousejiggler/blob/master/MouseJiggler/JigglePatterns.cs).

Profiles provide named starting points and can be refined before saving:

| Profile | Starting point |
| --- | --- |
| Balanced | Zen input, system power request, 60-second interval, user/lock/disconnect/low-battery safeguards |
| Long task | No motion, system power request, 120-second interval, four-hour maximum duration |
| Presentation | No motion, display-and-system power request, no pause on genuine input |
| Compatibility | Visible Normal input, no power request, 60-second interval |
| Visible | Circle input, no power request |
| Battery saver | Zen input, no power request, randomized 30–120-second interval, 30% low-battery threshold |
| Custom | Balanced starting values for explicit user overrides |

Power modes are `none`, `system`, and `display`. A power request is not input simulation: it asks
Windows to keep the system or display available while the session is active.

## Command line

The GUI executable accepts one command and validated options. `--help` and `--version` open visible
information dialogs; `--status` opens a visible status dialog rather than writing to a console.

```text
Commands: --start (-j, --jiggle), --stop, --toggle, --status,
          --show (--settings, -g), --exit

Profiles: --profile balanced|long-task|presentation|compatibility|visible|battery-saver|custom
Motion:   --motion off|zen|diagonal|linear|circle
Power:    --power none|system|display
Timing:   --interval DURATION, --random, --no-random, --pause-on-input DURATION,
          --stop-after DURATION
Safety:   --distance MULTIPLIER (1..120), --battery-threshold 0..100,
          --pause-on-fullscreen, --no-pause-on-fullscreen
Window:   --minimized, --close-to-tray, --no-close-to-tray
Storage:  --portable, --config PATH
```

Durations accept seconds by default or `s`, `m`, and `h` suffixes. Storage-path options apply when
the owning instance is launched; an already-running instance remains bound to its existing settings
path. The complete help text in the shipped executable is authoritative.

## Advanced INI settings

The settings store is a local, validated INI-style file. Portable mode places it beside the
executable; normal mode uses the user's local application-data directory; `--config PATH` selects an
explicit file. Unknown keys are ignored and invalid values fall back conservatively.

Active-hours controls are intentionally advanced and use minutes from midnight:

```ini
active_hours_enabled=true
active_hours_start_minute=540
active_hours_end_minute=1080
```

An end earlier than the start represents an overnight window. The complete key names are documented
in [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md).

## Distribution

There is no published release yet, so this repository intentionally contains no guessed download
URL. The release workflow is prepared to produce architecture-labelled portable archives such as
`IdleHarbor-<version>-windows-x64-portable.zip` and the corresponding ARM64 archive, plus:

- `SHA256SUMS.txt` checksum manifest;
- SPDX 2.3 SBOM JSON for each executable;
- GitHub artifact attestations;
- package manifest with source revision and architecture.

The optional per-user installer and startup helpers are documented in
[`packaging/README.md`](packaging/README.md). Signing is not claimed until a human-owned
Authenticode decision is made; the licence is also still pending in [`HUMAN_TODO.md`](HUMAN_TODO.md).

## Safety boundary

IdleHarbor is not an employee-monitoring bypass and must not be used to misrepresent presence or
evade a device policy. Simulated input may be detected, blocked, logged, or ignored. Check the rules
for a managed device before installing or running it. See [`docs/SAFETY.md`](docs/SAFETY.md).

## Documentation

- [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md) — controls, profiles, CLI, INI, and startup workflows.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — landed runtime boundaries and release flow.
- [`docs/SAFETY.md`](docs/SAFETY.md) — acceptable use, privacy, and trust assumptions.
- [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md) — build, runtime, install, and verification diagnosis.
- [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md) — reproducible footprint and idle-cost methodology.
- [`packaging/README.md`](packaging/README.md) — portable archives, installation, and startup choices.
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — development and review expectations.
- [`SECURITY.md`](SECURITY.md) — private vulnerability reporting.
- [`CHANGELOG.md`](CHANGELOG.md) — pre-release user-visible changes.

## Licence

No licence is committed yet. See [`HUMAN_TODO.md`](HUMAN_TODO.md); do not infer downstream rights
from this pre-release repository.
