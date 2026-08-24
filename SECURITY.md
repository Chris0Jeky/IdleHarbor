# Security policy

Security reports are accepted for the latest tagged release and the current `main` branch. Support
is best-effort; no fixed response-time or maintenance window is promised for any release.

## Reporting a vulnerability

Please do not open a public issue for an undisclosed vulnerability. Submit it through GitHub's
private advisory form:

[Report a private vulnerability](https://github.com/Chris0Jeky/IdleHarbor/security/advisories/new)

Include:

- affected commit, version, or artifact hash;
- Windows edition/build and architecture;
- a minimal reproducible sequence or proof of concept;
- expected versus observed behavior;
- required privilege, policy, or session conditions.

Do not include credentials, private work data, or third-party personal information. If the advisory
form is unavailable, contact the maintainer privately through GitHub and request a secure channel
before sending sensitive details.

## Scope

Reports involving unsafe input handling, unintended privilege requirements, settings persistence,
installer ownership, startup entries, release artifacts, or data exposure are in scope. A security
tool detecting or blocking simulated input is expected behavior, not a vulnerability. IdleHarbor
does not promise to bypass monitoring or security software.

## Response

Maintainers will acknowledge accessible reports, reproduce or classify the concern, and coordinate a
fix or mitigation. Timelines depend on the evidence and impact. Please do not publish
exploit details until a fix and release communication are available.
