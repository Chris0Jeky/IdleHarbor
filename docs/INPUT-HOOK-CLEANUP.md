# Input-hook cleanup and ownership

InputMonitor disables notifications before Stop attempts cleanup. Failed unhooks
retain their handles and exclusive ownership; a stopped monitor never reports
cleanup residue as usable input capabilities. Start retries cleanup before doing
any installation and refuses to allocate around unresolved handles.

Refresh owns two current handles and at most two replacement/retirement handles.
A failed replacement or retirement stops observation, retains every unresolved
handle and cannot grow the backlog on repeated calls. On destruction, up to four
unresolved handles move to a process-local quarantine and the callback owner is
cleared. A later Start must drain that quarantine before installing new hooks.
Lifecycle calls and destruction run on the installing thread.

Release is known only after a successful UnhookWindowsHookEx or an explicit
ERROR_INVALID_HOOK_HANDLE result. The latter permits recovery after Windows has
already removed a timed-out low-level hook. No busy retry loop is introduced.
Persistent OS failures can still leave handles quarantined until another Start
attempt or process teardown; this is retained recovery state, not successful release.

Primary Windows API contracts:
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unhookwindowshookex
- https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc

The input_monitor_lifecycle CTest compiles the real production source against a
guarded, target-private Windows API double. Thirteen scenarios cover stop/restart,
competing owners, refresh/partial failure, bounded four-handle quarantine,
destructor safety, known-invalid handles, injected-input filtering, notification
coalescing and PostMessage retry. No live hook or input is installed by the test.
The real application and platform library continue using Windows headers/linkage.

Local unchanged-source evidence: 46 failed assertions across 13 scenarios. Fixed
source passes GCC and Clang C++20 with warnings as errors and Clang address/undefined
behavior sanitizers. Hosted Windows CI is a separate gate. The broader #86 tracker
retains main.cpp helper coverage and visible cleanup-error presentation.
