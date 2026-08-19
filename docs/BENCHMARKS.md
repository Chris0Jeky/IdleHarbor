# Benchmark methodology

No performance number is claimed in this repository yet. The integration branch contains the
runtime and release tooling, but a benchmark result belongs to a pinned binary, controlled Windows
environment, and retained raw evidence.

## Measure

- executable and portable archive size, compressed and extracted;
- resident set size while stopped, paused, and active;
- CPU time and wakeups over fixed windows;
- timer behavior while stopped, paused, and active;
- clean release of hooks, timer, tray icon, power request, and mutex;
- x64 versus ARM64 results when both release artifacts exist.

## Controlled procedure

Record commit/tag, Windows edition/build, architecture, compiler/configuration, power mode, monitor
layout, endpoint-protection state, selected profile, and all session settings. Use the same machine
for comparisons where possible, repeat each run at least three times, and report median plus range.

1. Build a clean Release configuration from a pinned commit.
2. Record executable/archive sizes and SHA-256 hashes.
3. Measure five minutes stopped and five minutes paused.
4. Measure fifteen minutes active for each motion/power combination that is supported by the test plan.
5. Repeat start/stop, lock/unlock, disconnect/connect, and close paths to check cleanup.
6. Retain raw captures and tool versions with the release evidence.

Windows Performance Recorder/Analyzer, Process Explorer, or PowerShell process counters are
acceptable if the exact tool and command are recorded. Do not add telemetry to measure a local app.

## Goals, not results

The engineering goals are a small native artifact, no busy loop, bounded timer-driven work, and
near-zero work while stopped. These are goals, not measured claims. Do not put a number in the README
until the procedure has been run against the actual release binary.

## Result template

```text
Commit/tag:
Windows/build:
Architecture:
Compiler/configuration:
Profile and settings:
Tooling and versions:
Artifact size (zip / extracted):
Stopped CPU / working set / wakeups:
Paused CPU / working set / wakeups:
Active CPU / working set / wakeups:
Cleanup checks:
Runs and spread:
Known confounders:
```
