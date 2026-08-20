# Security policy

IdleHarbor is pre-release. The development branch and any future tagged release are handled on a
best-effort basis until a support window is published.

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
fix or mitigation. Timelines depend on evidence and the pre-release status. Please do not publish
exploit details until a fix and release communication are available.
