# Status-surface recovery implementation plan

**Goal:** Keep the underlying session reason and dirty state consistent across native status text, owner drawing, tooltips and the status command during tray failure/recovery.
**Issues:** #46, focused #42 regression coverage, and #86 main.cpp helper coverage.
**Constraints:** No session-policy, persistence, input, layout or foreground behavior change. No real desktop automation in the regression fixture.

- [x] Read the actual Application status, tray recovery and command paths.
- [x] Add a test-private friend copy and Shell/dialog/show-window boundaries while retaining real hidden native status controls.
- [x] Execute the test against unchanged production: Windows CI 37613604584 fails 184 of 345 assertions across sixteen scenarios; the original eleven CTests pass.
- [x] Separate transient tray availability from the underlying status reason and share status composition across surfaces.
- [x] Synchronize native status after recovery without recursively invoking tray recovery.
- [x] Preserve dirty state, pause/stopped reasons and the immediate Stop path.
- [ ] Re-run the complete Windows matrix and inspect final-head test execution.
- [ ] Review the final diff and current review threads, then merge only the qualified head.

The local production replacement is byte-identified as Git blob 055134cea5e7c3ea37b4b8cda3f288991517dd5f. Local Linux cannot execute this native fixture; Windows CI supplies both red and green evidence. Tests of already-correct numeric/enum helpers are coverage improvements, not claimed defect fixes.
