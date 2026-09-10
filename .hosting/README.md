# IdleHarbor hosting reference

Reference-only preparation, 2026-09-10. The inert `manifest.json` is for agent intake, not application configuration. No runtime, release, network behavior, hostname, billing or deployment trigger changes here. Existing workflows may publish after a future main merge; review them first.

IdleHarbor remains a native Windows utility. Retain the existing static project site and GitHub Releases rather than adding an application server. A custom domain should change the website's identity, not the installer's identity, local INI paths or released checksums.

IH1 prepares canonical metadata, deep links and asset paths for an owned hostname. IH2 proves that architecture-labelled downloads, checksum files, provenance links and existing installer/settings behavior survive a later site or display-name change. IH3 keeps the site honest about native execution, version support and signing state; no browser version, cloud account or telemetry is implied.

Follow `AGENTS.md` and `PROJECT_STATE.md`. This document is a reference, not a new agent workflow. Syntax: `python -m json.tool .hosting/manifest.json`. Website follow-ons need actual link/asset checks; native changes use the existing CMake/CTest and Windows proving path. JSON validation does not prove Windows behavior, signing or a hosted deployment.

Preserve the prior static build and immutable release links as rollback. Keep credentials, private account receipts and unregistered name candidates out of public assets. No paid backend is needed for this plan.
