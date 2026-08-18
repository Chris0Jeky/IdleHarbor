# Project state

Last updated: 2026-08-18

## Current milestone

IdleHarbor v0.1.0 is in active development. The current slice establishes a compilable native
Win32 project and contributor guardrails; keep-awake behavior has not landed yet.

## Authority and publication

- Declared posture: T1 sandbox, public synthetic publication, no sensitive data.
- Intended remote: `Chris0Jeky/IdleHarbor`.
- Release: none.
- Human decisions: see `HUMAN_TODO.md`.

## Proving commands

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Next slice

Implement the dependency-free policy engine, movement patterns, command-line model, and unit tests.
