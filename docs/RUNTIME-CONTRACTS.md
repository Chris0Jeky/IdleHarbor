# Bounded runtime contracts

## Power request cleanup

`PowerRequest::Clear()` retains the last successfully applied mode if the Windows
cleanup call fails. A later Clear, Apply(None), or destructor can retry instead of
forgetting the outstanding request. `Apply(None)` reports false when cleanup fails;
the public signatures and valid System/Display flags are unchanged.

Microsoft documents a zero return from SetThreadExecutionState as failure:
https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadexecutionstate

The power_request CTest recompiles the real production source against a private,
guarded API boundary double. Its include path and macro apply only to that test
executable. The application and platform library retain real Windows headers and
linkage. Tests cover apply/clear failure, both active modes, failed transitions,
successful retries, idempotence and destructor retry without changing system power.

This fixes state tracking, not every visible error path: UI callers of the void
Clear method still need separate failure reporting and native interaction proof.
No real operating-system failure was induced by this test.

## Interval and status contracts

IntervalSampler now throws std::invalid_argument for nonpositive or reversed
bounds rather than silently accepting them. Positive equal bounds, fixed-mode
maximum selection and seeded random sampling remain unchanged. Application
settings validation still supplies the existing user-facing interval limits.

The core_contracts CTest covers twelve invalid-bound combinations, valid bounds,
seed repeatability and all seven safety-pause status reasons. These status cases
close a coverage gap; they do not change status formatting or prove GUI/tray text.

Both new CTests run in the regular x64/x86 Windows lanes. ARM64 remains cross-build
only. Issue #86 stays open for hook-cleanup handling, UI helper coverage and broader
native failure presentation. No telemetry, networking, persistence or input
simulation is introduced by this work.
