# Benchmark evidence

IdleHarbor includes a reproducible local measurement script rather than telemetry. Results below
are a release-candidate baseline for one Windows machine, not a universal resource guarantee.

## v0.1.0 x64 local baseline

Measured on 2026-08-19 from native source commit `fc9a0e3` (the following commit changed packaging
only). The exact executable is identified by SHA-256 so the result remains auditable.

| Property | Value |
| --- | --- |
| Windows | NT 10.0.26100.0, x64 |
| Logical processors | 18 |
| Build | MSVC 19.29 / Visual Studio Build Tools 2019, CMake Release, static MSVC runtime |
| Executable | 493,056 bytes (481.5 KiB) |
| Executable SHA-256 | `2db24a680c7fa9b3ed4133ab1b79ea1865e90dc56619d01b22a598b5488c4ef0` |
| Repeats | 3 |
| Window per phase | 60 seconds, sampled every 500 ms |

The stopped phase left the window hidden in the notification area. The active phase used the Long
Task profile with motion **Off**, a **System** power request, genuine-input pause disabled, battery
threshold disabled, and fullscreen pause disabled. It therefore measured the lowest-disruption
active path and emitted no pointer input.

| Metric | Stopped median (range) | Active median (range) |
| --- | ---: | ---: |
| CPU time per 60-second window | 0 ms (0-0) | 15.625 ms (15.625-31.250) |
| CPU, normalized across 18 logical processors | 0% (0-0) | 0.0014% (0.0014-0.0029) |
| Average working set | 13.261 MiB (13.243-13.280) | 13.250 MiB (13.238-13.250) |
| Average private bytes | 1.838 MiB (1.818-1.870) | 1.781 MiB (1.766-1.781) |
| Maximum handle count | 158 (158-158) | 158 (158-158) |

The binary imports only Windows system libraries (`SHELL32`, `WTSAPI32`, `ole32`, `USER32`,
`KERNEL32`, and `GDI32`); it has no .NET, Qt, Electron, or separately installed Visual C++ runtime
dependency.

## Reproduce

Build Release, close other IdleHarbor instances, then run:

```powershell
.\tools\Measure-IdleHarbor.ps1 `
  -Executable .\build\x64\Release\IdleHarbor.exe `
  -DurationSeconds 60 `
  -Runs 3 `
  -OutputPath .\build\idleharbor-benchmark.json
```

The script records the executable hash and version, Windows version, architecture, logical processor
count, per-run CPU time, normalized CPU percentage, working/private memory, handles, and threads. It
uses a temporary explicit settings file and exits only the process it started.

## Interpretation and limitations

- Windows CPU accounting is quantized; 15.625 ms is one observed accounting interval, so values at
  this scale should be read as effectively idle rather than as a precise continuous rate.
- This sample covers stopped and power-request-only operation. Visible/Zen motion, genuine-input
  hooks, pause states, mixed-DPI movement, and ARM64 release artifacts need separate measurements.
- Endpoint protection, shell extensions, monitor topology, and other machine conditions can change
  memory and timing. Repeat the command on the target machine instead of treating this baseline as a
  promise.
- A 60-second window is useful release smoke evidence, not a substitute for long-duration soak or
  Windows Performance Recorder analysis.

For deeper release work, retain raw output outside the repository, record the exact tag and artifact
hash, measure stopped/paused/active paths for longer windows, and report median plus range rather
than a single best run.
