# Benchmark evidence

IdleHarbor includes a reproducible local measurement script rather than telemetry. Results below
are a release-candidate baseline for one Windows machine, not a universal resource guarantee.

The historical baseline remains below for comparison. The exact merged-main demo evidence is
recorded first and is identified by both source commit and executable hash.

## Exact merged-main x64 demo evidence

Measured on 2026-08-20 from merged-main source commit `4be21adfe69b299739b451a255e69d268757b668`.
The unsigned x64 executable is 516,608 bytes (504.5 KiB), version 0.1.0, with SHA-256
`d46b6b004a866ca0eed8eb02455ade1a769ac901e64c986e08f50f0720f54d6b`.

| Property | Value |
| --- | --- |
| Windows | Microsoft Windows NT 10.0.26100.0 |
| Architecture | AMD64 |
| Logical processors | 18 |
| Build | MSVC Release, exact merged-main source commit above |
| Executable | 516,608 bytes (504.5 KiB), unsigned |
| Executable SHA-256 | `d46b6b004a866ca0eed8eb02455ade1a769ac901e64c986e08f50f0720f54d6b` |
| Imported system libraries | `COMCTL32`, `SHELL32`, `WTSAPI32`, `ole32`, `USER32`, `KERNEL32`, `GDI32` |
| Runs | 3 |
| Window per phase | 60 seconds, sampled every 500 ms |

| Metric | Stopped median (range) | Active system-request median (range) |
| --- | ---: | ---: |
| CPU time per 60-second window | 0 ms (0-0) | 15.625 ms (0-46.875) |
| CPU, normalized across 18 logical processors | 0% (0-0) | 0.0014% (0-0.0043) |
| Average working set | 14.255 MiB (14.245-14.284) | 15.744 MiB (15.713-15.764) |
| Average private bytes | 1.998 MiB (1.988-2.017) | 2.170 MiB (2.142-2.177) |
| Maximum handle count | 164 (164-164) | 178 (178-178) |
| Threads | 4 (4-4) | 4 (4-5) |

The active phase used the power-request-only path with motion disabled, so this is a low-disruption
active baseline rather than a motion or hook-cost measurement. The raw JSON is retained with the
final demo evidence outside the repository and has SHA-256
`90b9ba01411cbd898f5a5ccf6f187492d5d2727251f4c3de11b18182e7563d3c`.

The exact-build portfolio captures are privacy-safe PNGs. Their dimensions and SHA-256 values are:

| Capture | Dimensions | SHA-256 |
| --- | ---: | --- |
| `idleharbor-window.png` | 1178x1389 | `b1df3612e78bd53cbd8de205805988c2575d79c35598b7c4af7197f4efc4ac04` |
| `idleharbor-viewport.png` | 1178x789 | `24fcc33a1eed4bbbd750c44132a198b9ae71717ca64f8e92ce90f8b2e962d236` |
| `idleharbor-running.png` | 1178x789 | `d177b90e222854999718f9405420b8f5af97b7d01e916cdb9860cbc537e6b0bc` |
| `idleharbor-paused.png` | 1178x789 | `88895486f9c1c23a4179170821ffc8a8a9e190ca3406820260361633c0790b62` |
| `idleharbor-tray-menu.png` | 244x415 | `2513a7c98ad4b9235dd6c89f18887b803f5265505104d3c4655819d701639d85` |

## Viewport proof boundary

The native PowerShell 7 and Windows PowerShell 5.1 harness preserved scroll position 24 through
a 570-to-580 logical-pixel height-only resize, then cleared the range, hid the scrollbar, and
returned to column geometry at a 670-logical-pixel viewport. The physical near-boundary width on
this desktop was 587 logical pixels because the application minimum constrained the window; exact
560-576 logical-pixel native behavior remains unverified. Deterministic model coverage covers that
boundary across supported DPI values.

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
