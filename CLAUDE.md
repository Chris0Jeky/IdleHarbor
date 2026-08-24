# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Authority order

`PROJECT_STATE.md` is the standing state document — read it before changing anything. It records the
current milestone, the released tag, distribution status, the open follow-up queue, and the "proving
commands" validated on this machine. `HUMAN_TODO.md` is authoritative for decisions only the owner
can make. `AGENTS.md` and `CONTRIBUTING.md` hold the contributor rules. The live Git tree, executable
checks, and hosted CI outrank all prose, including this file.

Update tests, `README.md`, `docs/`, `CHANGELOG.md`, and `PROJECT_STATE.md` whenever their facts change.

## Build and test

Windows-only, C++20, Unicode, no dependencies outside Windows system libraries. Build from a Visual
Studio developer PowerShell. This machine has Visual Studio 2019 Build Tools:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 16 2019" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
```

Use `Visual Studio 17 2022` (CI's generator) if VS 2022 is present, or the Ninja single-config form
documented in `AGENTS.md` (`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`) — note the
multi-config generator requires `--config`/`-C` on every build and ctest call.

Run one test suite by CTest name (`core`, `command_line`, `settings_store`, `window_layout`,
`motion_math`, `system_snapshot`, `foundation`):

```powershell
ctest --test-dir build/x64 -C Release -R core --output-on-failure
```

Tests are plain `main()` executables with a local `CHECK(condition)` macro that prints
`FAILED: <function>: <expr>` and returns a nonzero exit code — there is no test framework. Add a new
suite by adding a `tests/*_tests.cpp`, an `add_executable`/`add_test` pair in `CMakeLists.txt`, and
`idleharbor_configure_msvc(...)`. MSVC builds with `/W4 /WX /permissive-`, so warnings break the build.

### Runtime, packaging, and release checks

These are not part of CTest and must be run explicitly (sequentially, under both PowerShell 7 and
Windows PowerShell 5.1 for the packaging ones):

```powershell
.\tests\Test-NativeViewportRepaint.ps1 -Executable .\build\x64\Release\IdleHarbor.exe
.\tests\Test-ControlHelpTips.ps1 -Executable .\build\x64\Release\IdleHarbor.exe
.\packaging\Test-Packaging.ps1
.\packaging\Test-ReleaseWorkflow.ps1
.\packaging\Test-ReleaseVersion.ps1 -Tag v0.2.0
```

`tools\Capture-IdleHarborScreenshots.ps1` regenerates `docs/assets` plus `capture-manifest.json`; it
moves the real cursor and foregrounds the app, so do not run it unattended or interact with the
desktop while it runs. See `CONTRIBUTING.md` for its preconditions.

## Architecture

Three static libraries plus one WIN32 executable (`src/app/main.cpp`), all headers under
`include/idleharbor/`. The split exists so policy and geometry stay testable without a window or a
real pointer — keep it that way.

- **`idleharbor_core` (`src/core/core.cpp`, `include/idleharbor/core.hpp`)** — platform-neutral, no
  Win32. Owns `Settings` + `validate()`, the seven `ProfileKind` presets, bounded `MotionPlan`
  generation, `IntervalSampler`, and `PolicyEngine`. `PolicyEngine::evaluate(PolicyInput)` is the
  single decision point: it maps a snapshot of user activity, lock/disconnect, battery, fullscreen,
  minute-of-day, and elapsed time onto `{Run, Pause, Stop}` plus a `PolicyReason`. New safeguards
  belong here as a reason, not as an ad-hoc check in `main.cpp`.
- **`idleharbor_app_support` (`src/app/`)** — `command_line.cpp` (strict parser producing
  `CommandLineOptions` + errors, no console output), `settings_store.cpp` (INI load/validate/
  atomic temp-file-then-replace save; `ResolveSettingsPath` handles portable / `--config` / LocalAppData),
  `window_layout.cpp` (pure geometry: DPI scaling, clamping, scroll positions, wheel-delta remainder,
  wide/wrapped/stacked layout modes, tab-order sorting). All three are unit-tested.
- **`idleharbor_windows` (`src/platform/windows/`)** — `input_monitor` (low-level mouse/keyboard hooks
  that ignore IdleHarbor-marked injected input and post a message to the app thread; refreshed
  periodically so silent OS hook removal is detected), `motion_emitter` (the one `SendInput`
  boundary; moves to a safe anchor, emits the bounded path, returns exactly to the captured origin,
  and skips the pulse if genuine movement moved that origin), `power_request`
  (`SetThreadExecutionState`, cleared on pause/stop/shutdown/destruction), `system_snapshot`
  (battery/fullscreen/session). Only the pure-math parts have tests (`motion_math`, `system_snapshot`).
- **`src/app/main.cpp`** (~2.4k lines) — the `Application` class: window class, native control
  layout and scrolling viewport, tray icon and menu, one-second `WM_TIMER` that snapshots state and
  calls `PolicyEngine::evaluate`, single-instance `Local\IdleHarbor.Singleton.v1` mutex, emergency
  hotkey (Ctrl+Alt+Shift+F12), and shutdown teardown.

Injected input is tagged with `kIdleHarborInputMarker` (`"IDHBPLSE"`) so the hooks can distinguish it
from genuine input. A second invocation forwards its parsed command to the owner window through
size-limited, NUL-terminated `WM_COPYDATA` (`kCopyDataCommand`); storage paths stay an
owner-instance concern.

Motion and power are deliberately independent: a user can select input, power, both, or neither.
`docs/ARCHITECTURE.md` has the flow diagrams and the full UI/viewport layout contract.

## Product boundaries (hard constraints)

Do not add concealment, process hiding, misleading identity, monitoring bypasses, telemetry, network
access, elevation, or implicit persistence. Preserve a visible user-controlled status and an
immediate stop path. Treat injected input as compatibility behavior for legitimate idle prevention,
never as proof of presence or a way around a security control. `docs/SAFETY.md` is the reference.

## Conventions

- Namespaces `idleharbor::core`, `idleharbor::app`, `idleharbor::platform::windows`. Core uses
  `snake_case` free functions; app/platform use `PascalCase`. Prefer `[[nodiscard]]` and `noexcept`
  as the existing headers do.
- `.editorconfig`: CRLF, UTF-8, 4-space indent for C++/RC, 2-space for PS1/YAML/JSON/Markdown/CMake.
- The source version lives in four places that must stay in sync: `CMakeLists.txt` `VERSION`,
  `include/idleharbor/version.hpp`, `resources/IdleHarbor.rc` (both string and numeric fields), and
  `resources/app.manifest`. `Test-ReleaseVersion.ps1 -Tag vX.Y.Z` checks all four.
  `packaging/chocolatey/idleharbor.nuspec` is deliberately not one of them: it pins a *published*
  archive and its SHA-256, so it is repointed after the release workflow publishes, and
  `Test-ChocolateyPackage.ps1` only requires that it never runs ahead of the source version.
- Small present-tense commits (`Add interval validation tests`). Keep unrelated refactors out.
- Licence is `GPL-3.0-only`; releases are intentionally unsigned (see `HUMAN_TODO.md` q-2).

## CI

`.github/workflows/ci.yml` builds x64/ARM64/Win32 Release with VS 2022 and runs CTest for x64 and
Win32 (ARM64 builds only) plus `Test-Packaging.ps1`. `codeql.yml` runs a manual C/C++ build on PRs,
main, and weekly. `release.yml` triggers on stable `vX.Y.Z` tags and produces x64/ARM64 portable
ZIPs, SPDX 2.3 SBOMs, `SHA256SUMS.txt`, and GitHub artifact attestations. Actions are pinned by SHA.
