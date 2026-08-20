# IdleHarbor distribution

IdleHarbor is distributed as a native Windows executable with no application runtime or network
dependency. The portable archive is the canonical package; the PowerShell installer is an optional
per-user convenience layer around that archive. No release archive has been published yet.

## Release artifacts

The tag-triggered release workflow is prepared to produce:

- `IdleHarbor-<version>-windows-x64-portable.zip`;
- `IdleHarbor-<version>-windows-arm64-portable.zip`;
- `SHA256SUMS.txt` for the final asset directory;
- per-architecture SPDX 2.3 SBOM JSON;
- GitHub artifact attestations;
- a package manifest containing version, architecture, package type, and source revision.

The workflow builds Win32 in CI for coverage, but the current release matrix publishes x64 and
ARM64 archives. It accepts only stable `v<major>.<minor>.<patch>` tags, passes the triggering tag
through step-local environment data, and validates the same tag before packaging or publication.
Do not use a download URL until a tagged release exists.

### Trust-file asset contract

The portable archive owns the executable, installer helpers, package documentation, manifest, and
the `LICENSE` file once the release licence is approved. `SHA256SUMS.txt` and the versioned,
per-architecture SPDX JSON files are release-directory siblings generated outside the archive;
the installer does not copy them into the installed directory. Verify those sibling assets before
extracting or installing. The publication workflow refuses to publish while the root `LICENSE`
file is not tracked.

## Portable use

Extract the architecture-matched archive and run `IdleHarbor.exe`. Portable use does not create a
startup entry. It stores `IdleHarbor.ini` beside the executable when launched with `--portable`.
Use the tray menu or documented command-line options to control a session.

## Per-user installation

From an extracted release directory, preview the changes:

```powershell
.\install.ps1 -Startup TaskScheduler -WhatIf
```

Install to the default `%LOCALAPPDATA%\Programs\IdleHarbor` destination:

```powershell
.\install.ps1 -Startup TaskScheduler
```

Installation is per-user and does not require elevation. Startup is disabled by default. Supported
startup choices are:

- `TaskScheduler` — recommended least-privilege interactive task;
- `StartupFolder` — per-user `IdleHarbor.lnk`;
- `RunKey` — per-user `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` value;
- `None` — no startup registration.

Each startup choice runs `--start --minimized`. The installer removes only entries that point to the
same executable and refuses to overwrite a non-owned destination. It writes an ownership marker and
can be run repeatedly from Windows PowerShell 5.1 or PowerShell 7. Install and update operations
snapshot managed files and owned startup state, then restore them after a caught failure. Managed
paths that are symbolic links, junctions, or multiply linked files are rejected so an update cannot
write through the installation boundary. Unexpected user files are preserved.

Uninstall with:

```powershell
%LOCALAPPDATA%\Programs\IdleHarbor\uninstall.ps1
```

Settings are preserved by default. Add `-PurgeData` only when deleting local settings is intentional.
The uninstaller refuses to remove an unverified directory and preserves unexpected files. When the
installer created the Task Scheduler folder, rollback and uninstall also remove that folder after
proving it is empty; a pre-existing or non-empty folder is preserved.

## Verification and trust

After a future release is published, verify the checksum manifest and inspect the signature state:

```powershell
Get-FileHash .\IdleHarbor.exe -Algorithm SHA256
Get-AuthenticodeSignature .\IdleHarbor.exe
```

The release workflow is configured to produce SHA-256, SBOM, and attestation evidence. Authenticode
signing is not claimed until the human-owned signing decision is made. The repository licence is
also pending; do not infer downstream rights from a pre-release archive.
