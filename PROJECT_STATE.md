# Project state

Last updated: 2026-08-18

## Current milestone

IdleHarbor v0.1.0 is in active development. The platform-neutral policy core, strict command-line
model, atomic settings store, bounded motion emitter, genuine-input observer, power request,
battery/fullscreen adapters, and visible tray-controlled application have landed with direct tests.
Packaging, installer/startup helpers, and release QA remain.

## Authority and publication

- Declared posture: T1 sandbox, public synthetic publication, no sensitive data.
- Public remote: `Chris0Jeky/IdleHarbor`.
- Release: none.
- Human decisions: see `HUMAN_TODO.md`.

## Proving commands

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Next slice

Complete packaging/startup distribution and release QA against the visible application.
