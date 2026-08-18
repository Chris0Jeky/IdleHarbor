# Security policy

IdleHarbor is pre-release and currently has no release artifact. Security fixes are still welcome,
especially for unsafe input handling, unintended privilege requirements, persistence, packaging,
and data exposure.

## Supported versions

| Version | Support |
| --- | --- |
| Development branch | Best effort while the feature is being developed |
| Released versions | Security fixes according to the release notes |

## Reporting a vulnerability

Please do not open a public issue for an undisclosed vulnerability. Use GitHub's private
vulnerability reporting channel for this repository if it is enabled. If it is not enabled, open a
minimal private maintainer contact through the repository's available GitHub contact path and ask
for a secure channel before sending sensitive details.

Include:

- affected commit, version, or artifact hash;
- Windows edition/build and architecture;
- a minimal reproducible sequence or proof of concept;
- expected versus observed behavior;
- any required privilege, policy, or session conditions.

Do not include credentials, private work data, or third-party personal information.

## Scope and response

Maintainers will acknowledge a report when they can access it, reproduce or classify the concern,
and coordinate a fix or mitigation. Timelines depend on the evidence and the pre-release status.
Please do not publish exploit details until a fix and release communication are available.

IdleHarbor does not promise to bypass monitoring or security software. A tool detecting or blocking
simulated input is expected behavior, not a vulnerability.
