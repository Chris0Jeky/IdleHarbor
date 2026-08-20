# Contributing to IdleHarbor

Thank you for helping make a small, trustworthy Windows utility. The project is young, so behavior
and interfaces may continue to evolve. Keep changes focused, explain user-visible effects, and never
trade away an explicit stop path or truthful documentation for convenience.

## Before you start

Read [`AGENTS.md`](AGENTS.md), [`PROJECT_STATE.md`](PROJECT_STATE.md), and
[`docs/SAFETY.md`](docs/SAFETY.md). Search existing issues before opening a new one. If the change
touches input simulation, session state, persistence, startup, or packaging, include the relevant
failure and recovery behavior in the proposal.

## Development setup

Use a Windows developer PowerShell with CMake, Ninja, and a C++20-capable Visual Studio toolset:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Keep the core policy and motion logic testable without moving the real pointer. Runtime/UI checks
should be explicit and isolated from normal user input.

## Refreshing UI evidence

Build an x64 Release executable, close every existing IdleHarbor owner, set the interactive display
to the intended capture scale, and run:

```powershell
.\tools\Capture-IdleHarborScreenshots.ps1 `
  -Executable .\build\x64\Release\IdleHarbor.exe `
  -ExpectedDpi 192 `
  -Force
```

The tool creates stopped, scrolled, running, policy-paused, and notification-area captures under
`docs/assets`, plus `capture-manifest.json` with the exact source revision, executable identity,
Authenticode state, dimensions, and image hashes. Inspect every PNG before committing it; the
manifest is evidence of identity, not a substitute for visual review.

## Pull requests

- Describe the behavior changed and the smallest user scenario that proves it.
- Add or update tests for policy, validation, geometry, session signals, or packaging behavior.
- Update the README, user guide, changelog, and state document when their facts change.
- Include screenshots or a short recording for meaningful UI changes.
- Document Windows-version, architecture, privilege, and power-state assumptions.
- Do not add concealment, process hiding, monitoring bypasses, telemetry, elevation, or network
  access without a separately reviewed decision.

## Commit style

Use small, present-tense commits such as `Add interval validation tests` or `Document startup
policy`. Keep unrelated formatting and refactors out of a feature change.

## Issues and security

Use the issue forms for reproducible bugs and scoped feature proposals. Do not disclose security
vulnerabilities publicly; follow [`SECURITY.md`](SECURITY.md) instead.
