# Input-hook cleanup implementation plan

**Goal:** Preserve failed hook cleanup without claiming observer availability or accumulating hooks.
**Architecture:** Keep current and retired handles inside InputMonitor. Stop detaches notification state before releasing handles. Destruction transfers unresolved handles to bounded process quarantine; a new exclusive owner drains it before installing.
**Spec:** Issue #86, AGENTS.md and docs/INPUT-HOOK-CLEANUP.md.
**Constraints:** No live input, new dependency, runtime injection hook, API signature change or release action. Lifecycle methods remain installing-thread operations.

- [x] Reconcile relevant source/header and CMake blobs against live main.
- [x] Add production-source tests with a target-private Windows API double.
- [x] Observe unchanged source fail for lost handles and unsafe lifecycle transitions.
- [x] Retain Stop, partial-replacement and retirement cleanup state; quarantine destructor residue.
- [x] Prove no callback reaches a destroyed owner, no repeated-failure allocation growth, and recovery after Windows-side removal.
- [x] Run GCC/Clang warning-clean and address/undefined sanitizer checks: thirteen scenarios pass; unchanged source fails 46 assertions.
- [x] Register the CTest and update cleanup contracts, changelog and project state.
- [ ] Confirm final-head Windows CI/CodeQL, inspect actual test execution, refresh reviews and merge only that head.

## Review focus

Keep the backlog at two current plus two retired handles. Stop must quiesce callbacks before attempting cleanup. Failed Start must preserve another owner's slot. Known-invalid hook handles must not strand watchdog recovery. Destruction must never leave the global callback target pointing at dead storage. Private API doubles do not prove live Windows failure timing or cross-thread use.
