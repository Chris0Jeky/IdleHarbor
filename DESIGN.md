# Native window design

IdleHarbor is a Windows C++20/Win32 utility. Its window serves a short task:
choose how long to keep the system awake, start, check the status, and stop.
The owner identifies session duration as the most frequently used setting.

## Assessment and plan

The previous window displayed every setting and eight explanations in a long
scrolling form. Routine session choices competed with secondary motion,
safety and window preferences. Preserve the native Windows control language,
then simplify the hierarchy through progressive disclosure.

The main view contains Profile, Keep awake and Session duration. Duration offers
Until stopped, 15/30 minutes, 1/2/4 hours, and Custom duration. Custom reveals
the existing seconds field, retaining exact CLI/config values and validation.
Motion & timing, Safety pauses and Window & notifications begin collapsed.
Their buttons name Show/Hide and expose a native checked state. Opening a section
does not change or save its settings; several sections may stay open.

## Visual and interaction rules

- Use Segoe UI, native themed controls, system foreground/background colors and
  the existing application icon. Respect Windows high-contrast colors.
- Keep status above and Start/Stop/Save below the scrollable settings viewport.
  Status remains readable even when a safety section is collapsed.
- Show the canonical executable version in the window title. Do not imply a
  new public release merely because an unreleased source build uses the same version.
- Keep full control explanations in tooltips and detail-field hints within
  expanded sections. Label controls directly; never use an unlabeled settings icon.
- Exclude hidden controls from keyboard traversal. When collapsing a section
  containing focus, return focus to its disclosure button.
- Reuse existing DPI scaling, narrow stacking, scroll clamping and popup guards.
  Session settings remain locked during a running session; Stop remains enabled.

## Proof boundary

The compact UI target exercises actual native controls, duration selection,
custom validation, saved-value reload, disclosure state and DPI layouts.
Its optional capture mode renders synthetic stopped windows with the same
manifest and icon and checks actual keyboard focus, without starting a session.
It captures only those windows. Screen-reader speech, physical multi-monitor
transitions and live input emission need separate acceptance evidence.
