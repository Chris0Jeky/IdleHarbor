# Project state

Last updated: 2026-08-18

## Current milestone

IdleHarbor v0.1.0 is in active development. The platform-neutral policy core, strict command-line
model, atomic settings store, bounded motion emitter, genuine-input observer, power request, and
battery/fullscreen adapters have landed with direct tests. The visible tray application has not
yet replaced the foundation window.

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

Integrate the landed core and Win32 adapters into the visible tray-controlled application.
