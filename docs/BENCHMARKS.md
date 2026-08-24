# Benchmark evidence

IdleHarbor includes a reproducible measurement script rather than telemetry. Results below are a
`v0.2.0` baseline for one Windows machine, not a universal resource guarantee.

## v0.2.0 x64 baseline

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
expected to cost more than `0.1.0`. The figures move in both directions against that baseline:

| Metric | `0.1.0` | `0.2.0` | Direction |
| --- | ---: | ---: | --- |
| Stopped working set | 14.381 MiB | 13.710 MiB | down 0.671 MiB |
| Active working set | 15.850 MiB | 15.147 MiB | down 0.703 MiB |
| Stopped private bytes | 1.986 MiB | 2.109 MiB | **up 0.123 MiB** |
| Active private bytes | 2.115 MiB | 2.170 MiB | **up 0.055 MiB** |
| Stopped handles | 163 | 158 | down 5 |
| Active handles | 177 | 172 | down 5 |

Every row above compares medians. Thread count is left out because on that basis it did not move:
both releases have a median of 4 in each phase, and only the *peak* differs (`0.1.0` reached 5 once
during an active phase, `0.2.0` never did).

Working set and handle count fell; private bytes rose, by about 0.12 MiB stopped. Those two
directions are not noise inside the method -- the stopped private-bytes ranges do not even overlap
(`1.974-1.987` against `2.074-2.111`) -- so this script can resolve a shift that size. CPU was
effectively zero in both releases and distinguishes nothing.

What it cannot resolve is *why*. `0.1.0` was measured from a local Visual Studio 2019 build and
`0.2.0` from the released Visual Studio 2022 build, four days apart, on a machine whose conditions
were not held constant, so neither direction is attributable to the settings-window work without a
controlled comparison that has not been run.

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
above. Close every other IdleHarbor first -- including one the Task Scheduler action started. The
script checks for this and refuses with `Close existing IdleHarbor instances before running a
controlled measurement.` rather than measuring the wrong process.

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
