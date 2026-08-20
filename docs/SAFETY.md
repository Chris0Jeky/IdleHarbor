# Safety and acceptable use

IdleHarbor is a visible, user-controlled utility for legitimate work sessions where Windows idle
behavior is inconvenient. It is not a way to defeat security controls, conceal activity, or
misrepresent a person's presence.

## Boundaries

IdleHarbor does not provide stealth, process hiding, misleading identity, monitoring bypasses,
undetectability claims, elevation without consent, network telemetry, or remote control. Simulated
input can be detected, blocked, logged, or ignored by Windows and other software. It is never proof
that a person is working or present.

## User-visible controls

The application exposes Running, Paused, and Stopped states, pause reasons, a tray menu, a visible
window, and immediate Stop actions. The emergency hotkey is an additional escape path. Minimize to
the notification area is a visibility choice, not concealment.

Safeguards include genuine-input cooldown, lock/session pause, low-battery and battery policy,
fullscreen policy, active hours, and maximum duration. They are conservative compatibility features,
not security guarantees; test them in the actual Windows session where the utility will run.

If settings recovery warnings appear, treat the session as **Stopped** until the displayed fallback
values have been reviewed and saved. Automatic starts are blocked after recovery so a malformed or
mutually invalid file cannot silently select a different behavior; an explicit Start action remains
visible and user-controlled.

## Managed devices

Read the rules for the device, account, network, and work environment before installing or running
IdleHarbor. If a policy prohibits simulated input or third-party utilities, do not run it. The
project cannot determine or approve an employer's policy on a user's behalf.

Application approval and persistence approval may be separate decisions. Do not configure the
Task Scheduler, Startup-folder, or Run-key options on a managed laptop unless policy permits both
IdleHarbor and that startup mechanism. If endpoint controls block it, do not disable or independently
whitelist around them.

## Privacy and privilege

The current application has no network service or telemetry path. It stores validated local
preferences and does not need an elevated process or Windows service. It should not collect input
content, screenshots, window titles, or browsing history.

Low-level hooks and injected input are Windows compatibility mechanisms. They may be unavailable at
some integrity boundaries or under endpoint policy; IdleHarbor must not be treated as a guarantee
that a safeguard or pulse will work in every application.

## Distribution trust

Download only from the project’s
[`v0.1.0` release page](https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v0.1.0), then verify the
architecture, SHA-256 manifest, SPDX SBOM, and GitHub attestation before running it. `v0.1.0` is
intentionally unsigned, so Authenticode is expected to report `NotSigned`; checksums and attestations
establish artifact identity and provenance, not publisher identity. Do not disable endpoint
protection to run an unverified build.

## Vulnerabilities

Do not post undisclosed security issues publicly. Use the private advisory form described in
[`SECURITY.md`](../SECURITY.md).
