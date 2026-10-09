# Native window design

Mode: Operate. The owner's visual direction is a premium, restrained, almost
Apple-like utility. The compact workflow stays fixed; the visual treatment changes.

## Assessment and direction

The plain compact form improved focus but oversized boxed disclosures competed
with session choices. Uniform type and stock borders left little visual hierarchy.
Use a quiet light surface, white rounded fields, a strong Session heading and
one blue primary action. Secondary settings become unboxed disclosure rows with
drawn chevrons. The existing Windows title bar, icon and executable version remain.
The owner's follow-up adds a default dark appearance and subtle native material.
Charcoal surfaces, cool white type and a pale blue primary action provide depth
without changing the compact workflow. Light remains available.

## Visual rules

- Surface: #f7f8fa; text: #1d2026; supporting text: #5e6571.
- Dark surface: #17191f; fields: #252932; text: #edf1f7; supporting text:
  #aab2c0; accent: #89baff with #0c203a primary-action text for contrast.
- Accent: #005bd3 for primary actions, checked controls and keyboard focus.
  Hover and pressed states deepen the accent. Disabled actions use muted neutrals.
- Segoe UI: 16pt semibold heading, 10pt controls, 9pt explanations.
  Routine rows use a 44 logical-pixel rhythm; disclosures reserve 44 pixels
  for a title and a quieter 9pt current-settings summary, on a 52-pixel rhythm.
- Rounded field/button corners, fine neutral borders, no gradients or decorative
  shadows. Visible blue focus borders; a white inset marks focus on a blue action.
- Native controls retain their roles, names, selection, caret, Undo and Tab
  behaviour. Only their painting changes. High contrast uses native/system colours;
  theme and settings changes refresh the palette and numeric field frames.
- Native dropdowns render matching item colours and retain selection semantics.
  High contrast uses system foreground/highlight colours. Appearance can change
  during a session; saving still waits until the session stops.
- Soft backdrop uses documented DWM Mica on supported Windows 11 versions,
  dynamically loaded from the system directory. It affects the frame/peripheral
  background; content and controls remain opaque. Older Windows, disabled material
  and high contrast use a solid fallback. No whole-window opacity or custom chrome.
  PrintWindow previews show the solid appearance because they cannot composite Mica.
- Start is primary when stopped; Stop becomes primary during a session. Status
  text and fixed actions remain available while the settings body scrolls.

## Interaction contract

Session duration comes first, followed by Profile and Keep awake; all remain visible. Duration offers
Until stopped, 15/30 minutes, 1/2/4 hours and Custom duration. Custom exposes exact
seconds, preserving non-preset CLI/config values. Motion & timing, Safety pauses
and Window & notifications begin collapsed and may expand independently.
Disclosure rows expose current motion/timing, the enabled safeguard count and
appearance preferences without expansion. Their native accessible names include
the summary, Show/Hide and native checked state. Randomized timing says Up to;
invalid numeric edits prompt review without modifying the input. Summary values
describe the editable configuration, not proof that a safeguard is currently pausing.
The preferred window height is 500 logical pixels so the compact view still fits. Collapsing
returns focus to the disclosure when its contents held focus. Disclosure never
changes settings. Session settings remain locked while running; Stop remains enabled.

## Proof boundary

The real-control fixture covers duration/persistence, disclosure, keyboard access,
numeric editing/Undo, DPI layouts, actual rendered field/action colours and repeated
GDI painting. Theme switching preserves unfinished edits, settings round-trip through
real files, and repeated palette switches release brushes. Field assertions accept
the defined hover colour without moving the user's cursor (issue #126).
Synthetic window captures inspect compact/custom/expanded, dark dropdown and native
system-colour fallback states. This does not prove screen-reader speech, actual
Windows high-contrast activation or physical multi-monitor transitions.
Installed-window DWM queries prove the dark title bar and Mica attribute are active
on the current machine; a pixel read inside the foreground app confirms the composed
header is not the black surface returned by an uncomposited PrintWindow capture.
