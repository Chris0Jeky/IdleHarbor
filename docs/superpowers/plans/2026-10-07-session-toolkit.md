# Session toolkit implementation plan

Goal: deliver issue #117's session-only timer, pause/resume, snooze and extension through the native tray and strict CLI without weakening safeguards or changing saved defaults.

Architecture: keep PolicyEngine as the sole expiry authority. Application commands delegate to it; status composition queries remaining time. The existing timer drives snooze expiry and countdown, with no extra worker or persisted state. Windows uptime includes sleep/hibernation, so elapsed-session limits continue to mean elapsed time.

Selected over alternatives: use current session controls rather than a new scheduler/service; retain the native tray rather than a new overlay; no history database or hosted backend. Active-hours UI, richer diagnostics and settings import/export remain possible later directions, not prerequisites or claimed deliveries.

- [x] Reconcile affected source and build paths against main e5a26836.
- [x] Add a buildable missing-feature test; unchanged core reports the absent session-controls contract.
- [x] Add bounded pause/snooze/extension/countdown and elapsed-time saturation; 65 core assertions plus original core tests pass GCC/Clang and sanitizers.
- [x] Wire native tray choices and strict CLI commands, preserving invalid/in-progress settings.
- [x] Test actual Application methods, real hidden native controls and menu structure; avoid live input and power changes in tests.
- [x] Report failed power cleanup and retain an explicit Stop retry.
- [x] Update README, user guide, changelog and current project state.
- [ ] Inspect exact-head Windows CI and code review, then merge only qualified work.

Review focus: expiry wins over manual pause, Resume rechecks safeguards, activity during snooze counts, invalid values do not mutate state, extensions cannot revive expired sessions, Stop stays immediate, and command handling must not discard a pending combo edit. Cross-process/interactive Explorer timing and ARM64 execution are separate from headless native-control tests.
