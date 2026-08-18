# Benchmark methodology

No IdleHarbor performance numbers are claimed yet. The current repository contains a foundation
message-box executable, not the timer or input path that the benchmark must measure. This document
defines a reproducible method for reporting results once behavior lands.

## Questions to answer

- What is the size of each release artifact, compressed and extracted?
- What is the resident set size while stopped, paused, and active?
- What is CPU time over a fixed active-session window?
- How much does the timer wake the process while stopped and paused?
- Does the application close cleanly and release its timer, tray, and input resources?

## Controlled setup

Record Windows version/build, architecture, compiler/toolset, build configuration, commit SHA,
power mode, monitor count, and whether endpoint protection is enabled. Close unrelated applications
where practical, but do not claim that a single workstation is representative of every device.

Use the same settings for each comparison: mode, interval, randomization, distance, and session
duration. Repeat each run at least three times and report median plus range. Keep raw measurements
with the release evidence rather than copying a single best run into the README.

## Suggested procedure

1. Build a clean Release configuration from a pinned commit.
2. Record executable and archive sizes, plus SHA-256 hashes.
3. Measure a stopped baseline for five minutes.
4. Measure a paused session for five minutes.
5. Measure an active session for fifteen minutes using each supported mode.
6. Record process CPU time, peak working set, wakeups, and unexpected errors.
7. Repeat after lock/unlock and stop/close to check cleanup.

The exact tooling may use Windows Performance Recorder/Analyzer, Process Explorer, or built-in
PowerShell process counters. Name the tool and command in the result so another contributor can
repeat it. Avoid adding a telemetry dependency merely to measure a local utility.

## Acceptance goals (not results)

The design goal is near-zero work while stopped, bounded timer-driven work while active, no busy
loop, and a small native artifact. These are engineering goals, not measured facts. Publish the
numbers only after the procedure above has been run against the actual release binary.

## Reporting template

```text
Commit:
Windows/build:
Architecture:
Compiler/configuration:
Mode/settings:
Tooling:
Artifact size (zip / extracted):
Stopped CPU / working set / wakeups:
Paused CPU / working set / wakeups:
Active CPU / working set / wakeups:
Runs and spread:
Known confounders:
```
