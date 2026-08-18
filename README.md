# IdleHarbor

> A tiny native Windows utility for transparent, configurable idle prevention.

IdleHarbor is an independent C++/Win32 application for keeping legitimate long-running work,
presentations, monitoring dashboards, and installations awake without shipping a managed runtime.
It is currently under active development toward v0.1.0.

## Design goals

- One dependency-free native executable for each supported Windows architecture.
- Near-zero work while stopped and timer-driven operation while active.
- Clear window and notification-area status, with an immediate stop control.
- Real-user activity, lock, battery, and time-limit safeguards.
- Portable, per-user, and start-at-sign-in installation choices.
- Reproducible builds, tests, checksums, an SBOM, and release provenance.

## Safety boundary

IdleHarbor is not an employee-monitoring bypass and will not provide stealth, misleading identity,
process hiding, or security-control evasion. Simulated input is detectable, may be blocked by
Windows integrity boundaries or organisational policy, and must not be treated as proof that a
person is present. Check the rules that apply to any managed device before installing it.

## Current status

The platform-neutral motion, interval, validation, and safety-policy core is now implemented and
covered by dependency-free tests. Win32 input, power-state integration, tray UI, and installers
remain in progress. See [`PROJECT_STATE.md`](PROJECT_STATE.md) for the live milestone and proving
commands.

The core keeps motion and power requests independent. Profiles currently provide conservative
starting points: `balanced` (Normal/System), `long-task` (quiet Zen/System with a four-hour limit),
`presentation` (no pointer motion/display request), `compatibility` (Normal input only), `visible`
(Circle input only), `battery-saver` (quiet Zen input with battery pause), and `custom` (balanced
values for explicit overrides). `Off` motion with `None` power is rejected as non-runnable.

## Build the foundation

Use a Visual Studio developer PowerShell with CMake and Ninja available:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The detailed feature guide, installation paths, screenshots, benchmarks, and downloads will land
with the implementation and verified release rather than being claimed in advance.
