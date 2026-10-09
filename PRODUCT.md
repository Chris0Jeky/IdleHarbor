# IdleHarbor

## Platform and purpose

Windows desktop; native C++20/Win32, using Windows system libraries.
IdleHarbor provides user-controlled idle prevention with visible session status,
explicit Start/Stop, timed sessions and safety pauses. The application stores
settings locally and has no networking or telemetry.

## Owner priorities

Keep Start, Stop and session duration accessible. Hide less frequent settings
behind clearly named expandable sections. Display the executable version.
The owner requests a premium, minimal, almost Apple-like visual feel while
retaining the simpler workflow delivered by the compact window.
The owner subsequently requests richer surface depth, dark mode and subtle
opacity. Dark is the initial appearance; a saved light-mode switch and a saved
soft title-bar backdrop switch live under Window & notifications. The control area
is always opaque after the full-window glass rendering repair.

## Constraints

Retain the existing icon, settings semantics, exact custom durations and immediate
Stop path. Preserve native keyboard behaviour, accessible control names and
Windows high-contrast rendering. An unreleased local build does not constitute
a new public release.
