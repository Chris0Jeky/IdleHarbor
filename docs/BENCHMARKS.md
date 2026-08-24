# Benchmark evidence

IdleHarbor includes a reproducible local measurement script rather than telemetry. Results below
are a `v0.2.0` local baseline for one Windows machine, not a universal resource guarantee.

## v0.2.0 x64 local baseline

Measured on 2026-08-24 from the **published release** executable rather than a local build, so the
numbers describe the binary people actually download. The exact executable is identified by SHA-256
so the result remains auditable.

| Property | Value |
| --- | --- |
| Windows | NT 10.0.26100.0, x64 |
| Logical processors | 18 |
| Build | GitHub Actions `windows-2022`, MSVC / Visual Studio 2022, CMake Release, static MSVC runtime |
| Executable | 544,256 bytes (531.5 KiB) |
| Executable SHA-256 | `bbada75845a1832aa5907e81003ebfd52761cbc7cac1c234d61263be7baf5e5a` |
| Repeats | 3 |
| Window per phase | 60 seconds, sampled every 500 ms |

The stopped phase left the window hidden in the notification area. The active phase used the Long
Task profile with motion **Off**, a **System** power request, genuine-input pause disabled, battery
threshold disabled, and fullscreen pause disabled. It therefore measured the lowest-disruption
active path and emitted no pointer input.

| Metric | Stopped median (range) | Active median (range) |
| --- | ---: | ---: |
| CPU time per 60-second window | 0 ms (0-0) | 0 ms (0-15.625) |
| CPU, normalized across 18 logical processors | 0% (0-0) | 0% (0-0.0014) |
| Average working set | 13.710 MiB (13.709-13.713) | 15.147 MiB (15.131-15.147) |
| Average private bytes | 2.109 MiB (2.074-2.111) | 2.170 MiB (2.167-2.209) |
| Maximum handle count | 158 (158-158) | 172 (172-172) |
| Maximum thread count | 4 (4-4) | 4 (4-4) |

`0.2.0` adds a tooltip control and eight static explanations to the settings window, so it was
expected to cost slightly more than `0.1.0`. It measures slightly *less* on every memory and handle
figure -- stopped working set 13.710 MiB against `0.1.0`'s 14.381 MiB, and 158 handles against 163.
This is not a controlled comparison: `0.1.0` was measured from a local Visual Studio 2019 build and
`0.2.0` from the released Visual Studio 2022 build, and machine conditions differ between 2026-08-20
and 2026-08-24. Read it as "the settings-window work did not make IdleHarbor measurably heavier",
not as a like-for-like improvement.

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

Point `-Executable` at an extracted or installed release build instead to reproduce the table
above. IdleHarbor is single-instance, so any other copy -- including one the Task Scheduler
action started -- has to be closed first, or the measurement process forwards its command and
exits.

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
