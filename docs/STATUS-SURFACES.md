# Status composition and tray recovery

The application keeps the session/status reason separate from notification-icon
availability. Tray loss adds a transient warning to the displayed state instead
of replacing a paused or stopped reason with a generic Running/Stopped label.
Dirty settings remain an independent prefix.

The status card's native text, owner-drawn text, tray tooltip and --status dialog
share the same composition. Recovery removes only the icon-availability warning;
it does not discard a safeguard reason, settings-recovery warning or dirty state.
A newly added icon is seeded with the recovered composition rather than the old
unavailable tooltip. Failed recovery keeps the window-visible fallback. Native
status synchronization does not call tray recovery, avoiding recursive retries.
Session start/stop policy, persistence, motion, focus and window-layout behavior
are not changed by this repair.

## Executable regression

The application_status CTest invokes actual Application methods from a generated
build-directory copy of main.cpp. The only source transformation adds one private
friend declaration for test access; the production target compiles main.cpp
directly and contains no test instrumentation. The test translation unit intercepts
Shell_NotifyIconW, MessageBoxW and ShowWindow, but creates hidden native STATIC
controls and reads their real GetWindowText output.

Sixteen dirty/clean scenarios cover stopped, running and paused states, initial
status, icon loss, failed and successful TaskbarCreated/tooltip/direct recovery,
status-command agreement and immediate Stop. Forty-one additional assertions call
the real unsigned-number and profile/motion/power text helpers. The initial test
head d13e1fbf failed 184 of 345 assertions in Windows CI 37613604584; the eleven
existing CTests and helper assertions already passed. This is behavioral red
proof, not an unbuildable test fixture.

The test does not create a real tray icon or dialog, show the hidden windows, move
the pointer, install input hooks, foreground an application or capture a screenshot.
It proves native status text plus injected Shell failure/recovery behavior, not
interactive Explorer timing, pixel rendering, screen-reader interaction or native
power-cleanup failure presentation. Those distinctions remain relevant to #86
and the separate focus/capture work.
