# Safety and acceptable use

IdleHarbor is intended to make legitimate, user-observed work sessions less vulnerable to an
unwanted Windows idle transition. It is not a way to defeat security controls, conceal activity,
or misrepresent a person's presence.

## Non-negotiable boundaries

IdleHarbor will not provide:

- stealth or concealment modes;
- process hiding, identity spoofing, or misleading window names;
- monitoring bypasses or claims of being undetectable;
- elevation or a service installed without explicit consent;
- network telemetry or remote control as part of idle prevention.

Simulated input can be detected, blocked, logged, or ignored by Windows and other software. It is
never evidence that a person is working or present.

## User controls

The target release keeps the state visible and supplies an immediate stop path. Intelligent-stop
signals should be conservative: real user activity, lock/session changes, explicit end times, and
power policy can pause or stop the session. A user should be able to see why the state changed and
resume it intentionally.

## Managed devices

Read the rules for the device, account, network, and work environment before installing or running
IdleHarbor. If a policy prohibits simulated input or third-party utilities, do not run it. The
project cannot determine or approve an employer's policy on a user's behalf.

## Privacy and data

The current foundation has no network access or telemetry. The target design stores only validated
local preferences needed to reproduce a user's chosen configuration. It should not collect input
content, screenshots, window titles, or browsing history. Any future change to that boundary needs
separate documentation and review.

## Privilege and supply chain

The target application should run per-user without elevation. Release artifacts should be labelled
by architecture and accompanied by SHA-256 checksums, build provenance, and clear signing status.
Do not treat an unsigned binary as trusted merely because it came from a public repository; build
from source or verify the release evidence appropriate to your environment.

## Reporting a concern

Do not open a public issue for an undisclosed vulnerability. Follow [`SECURITY.md`](../SECURITY.md)
and include the smallest reproducible details that allow maintainers to validate the concern without
receiving private data or credentials.
