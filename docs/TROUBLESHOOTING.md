# Troubleshooting

This project is pre-release. The first section applies to the current foundation; later sections
describe checks for the v0.1.0 target once those components land.

## Build problems

### CMake cannot find a generator

Use a Visual Studio developer PowerShell and confirm that both `cmake` and `ninja` are on `PATH`:

```powershell
cmake --version
ninja --version
```

Then configure from the repository root:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

### CMake rejects the platform

IdleHarbor is Windows-only. The project intentionally stops configuration on non-Windows hosts.
Use a Windows build environment or cross-compile only after a documented toolchain is added.

### Tests fail to configure or build

Start from a fresh build directory only after preserving any diagnostics you need. Re-run the
commands in [`PROJECT_STATE.md`](../PROJECT_STATE.md), then attach the complete CMake and test
output to a bug report. Do not include credentials, private paths, or workplace data.

## Current application behavior

The foundation executable shows a message box and exits. That is expected at this milestone. If
you are looking for a tray icon, movement mode, scheduler helper, or idle prevention, those are
release targets and are not yet available in the current checkout.

## Target runtime checks

When v0.1.0 lands, collect:

1. Windows edition/build and architecture.
2. IdleHarbor version and package type.
3. Selected mode, interval, power policy, and pause reason.
4. Whether the app is stopped, paused, or running.
5. The smallest reproducible sequence and relevant event-log message.

First try the visible **Stop**, reset settings to defaults, and relaunch. Do not disable endpoint
protection or change workplace policy to work around a compatibility issue.

## Reporting a bug

Use the repository bug-report form and redact private or managed-environment details. Security
issues belong in [`SECURITY.md`](../SECURITY.md), not a public issue.
