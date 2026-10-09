# Native window design

Mode: Operate. The owner's visual direction is a premium, restrained, almost
Apple-like utility. The compact workflow stays fixed; the visual treatment changes.

## Assessment and direction

The plain compact form improved focus but oversized boxed disclosures competed
with session choices. Uniform type and stock borders left little visual hierarchy.
Use a quiet light surface, white rounded fields, a strong Session heading and
one blue primary action. Secondary settings become unboxed disclosure rows with
drawn chevrons. The existing Windows title bar, icon and executable version remain.

## Visual rules

- Surface: #f7f8fa; text: #1d2026; supporting text: #5e6571.
- Accent: #005bd3 for primary actions, checked controls and keyboard focus.
  Hover and pressed states deepen the accent. Disabled actions use muted neutrals.
- Segoe UI: 16pt semibold heading, 10pt controls, 9pt explanations.
  Routine rows use a 44 logical-pixel rhythm; disclosures use 40.
- Rounded field/button corners, fine neutral borders, no gradients or decorative
  shadows. Visible blue focus borders; a white inset marks focus on a blue action.
- Native controls retain their roles, names, selection, caret, Undo and Tab
  behaviour. Only their painting changes. High contrast uses native/system colours;
  theme and settings changes refresh the palette and numeric field frames.
- Start is primary when stopped; Stop becomes primary during a session. Status
  text and fixed actions remain available while the settings body scrolls.

## Interaction contract

Profile, Keep awake and Session duration are always visible. Duration offers
Until stopped, 15/30 minutes, 1/2/4 hours and Custom duration. Custom exposes exact
seconds, preserving non-preset CLI/config values. Motion & timing, Safety pauses
and Window & notifications begin collapsed and may expand independently.
Accessible disclosure names include Show/Hide and native checked state. Collapsing
returns focus to the disclosure when its contents held focus. Disclosure never
changes settings. Session settings remain locked while running; Stop remains enabled.

## Proof boundary

The real-control fixture covers duration/persistence, disclosure, keyboard access,
numeric editing/Undo, DPI layouts, actual rendered field/action colours and repeated
GDI painting. Synthetic window captures inspect compact/custom/expanded and native
system-colour fallback states. This does not prove screen-reader speech, actual
Windows high-contrast activation or physical multi-monitor transitions.
