# Benchmark evidence

IdleHarbor includes a reproducible local measurement script rather than telemetry. Results below
are a release-candidate baseline for one Windows machine, not a universal resource guarantee.

The historical baseline remains below for comparison. The exact merged-main demo evidence is
recorded first and is identified by both source commit and executable hash.

## Exact merged-main x64 demo evidence

Measured on 2026-08-20 from merged-main source commit `b9bfa82a45dd8cd777c3232a4d71b68f8ef9a9c4`.
The unsigned x64 executable is 516,608 bytes (504.5 KiB), version 0.1.0, with SHA-256
`432026b0719671bd33c1b0ca0f7df809fec20e73fc05b38b6e513606584f6d54`.

| Property | Value |
| --- | --- |
| Windows | Microsoft Windows NT 10.0.26100.0 |
| Architecture | AMD64 |
| Logical processors | 18 |
| Build | MSVC Release, exact merged-main source commit above |
| Executable | 516,608 bytes (504.5 KiB), unsigned |
| Executable SHA-256 | `432026b0719671bd33c1b0ca0f7df809fec20e73fc05b38b6e513606584f6d54` |
| Imported system libraries | `COMCTL32`, `SHELL32`, `WTSAPI32`, `ole32`, `USER32`, `KERNEL32`, `GDI32` |
| Runs | 3 |
| Window per phase | 60 seconds, sampled every 500 ms |

| Metric | Stopped median (range) | Active system-request median (range) |
| --- | ---: | ---: |
| CPU time per 60-second window | 0 ms (0-0) | 0 ms (0-31.25) |
| CPU, normalized across 18 logical processors | 0% (0-0) | 0% (0-0.0029) |
| Average working set | 14.269 MiB (14.251-14.271) | 15.732 MiB (15.717-15.736) |
| Average private bytes | 2.012 MiB (1.992-2.014) | 2.154 MiB (2.150-2.181) |
| Maximum handle count | 164 (164-164) | 178 (178-178) |
| Threads | 4 (4-4) | 4 (4-5) |

The active phase used the power-request-only path with motion disabled, so this is a low-disruption
active baseline rather than a motion or hook-cost measurement. The raw JSON is retained with the
final demo evidence outside the repository and has SHA-256
`5f54946f64f02a124c7ef73a5f36ca50c1f8440de73c6c6a4c53d94b879e678f`.

The exact-build portfolio captures are privacy-safe PNGs. Their dimensions and SHA-256 values are:

| Capture | Dimensions | SHA-256 |
| --- | ---: | --- |
| `idleharbor-window.png` | 1178x1389 | `bd9e3ae7db44bc7aa4d90a2070d131c23a3e747a6b266c08d46fe2fd4bbb4d20` |
| `idleharbor-viewport.png` | 1178x789 | `46b3f7ee629f54aee5887f7a65711155b177e485e126aa544c6ee2fac194d141` |
| `idleharbor-running.png` | 1178x789 | `bdbc31fb2690610c608e249b18cd0b43d665b6dee8ec54c50a5e7a350f6a0157` |
| `idleharbor-paused.png` | 1178x789 | `51fdb85cd022305f01411339a5299eb4583fb4eb5e0a25ac65349801a08e4739` |
| `idleharbor-tray-menu.png` | 244x415 | `bf72e24f89fb21c7a8559c82c28ebd0b632da2babd6d4d18b91a73d02d3f9015` |

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
