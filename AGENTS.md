# IdleHarbor contributor instructions

Read `PROJECT_STATE.md` before changing the repository. The live Git tree, executable checks, and
hosted CI outrank prose.

## Build and test

From a Visual Studio developer PowerShell:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Release builds use `-DCMAKE_BUILD_TYPE=Release`. The application is Windows-only, C++20, Unicode,
and dependency-free outside Windows system libraries.

## Product boundaries

- Preserve a visible, user-controlled status and an immediate stop path.
- Never add concealment, misleading identity, process hiding, monitoring bypasses, telemetry,
  network access, elevation, or persistence without explicit user action.
- Treat injected input as compatibility behavior for legitimate idle prevention, not proof of
  presence or a security-control bypass.
- Keep core policy and motion generation testable without moving the real pointer.
- Update tests, documentation, `CHANGELOG.md`, and `PROJECT_STATE.md` when their facts change.

Use focused present-tense commits. Preserve unrelated work. In a worktree, first confirm the
repository root, branch/HEAD, status, and authority file before editing; one writer owns each
checkout.
