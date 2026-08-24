# IdleHarbor distribution

IdleHarbor is distributed as a native Windows executable with no application runtime or network
dependency. The portable archive is the canonical package; the PowerShell installer is an optional
per-user convenience layer around that archive. Download only from the canonical
[`v0.2.0` release page](https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v0.2.0).

## Release artifacts

The `v0.2.0` release includes these exact assets:

- [`IdleHarbor-0.2.0-windows-x64-portable.zip`](https://github.com/Chris0Jeky/IdleHarbor/releases/download/v0.2.0/IdleHarbor-0.2.0-windows-x64-portable.zip);
- [`IdleHarbor-0.2.0-windows-arm64-portable.zip`](https://github.com/Chris0Jeky/IdleHarbor/releases/download/v0.2.0/IdleHarbor-0.2.0-windows-arm64-portable.zip);
- `SHA256SUMS.txt` for the final asset directory;
- per-architecture SPDX 2.3 SBOM JSON;
- GitHub artifact attestations;
- a package manifest containing version, architecture, package type, and source revision.

The workflow builds Win32 in CI for coverage, but the current release matrix publishes x64 and
ARM64 archives. It accepts only stable `v<major>.<minor>.<patch>` tags, passes the triggering tag
through step-local environment data, and validates the same tag before packaging or publication.

### Trust-file asset contract

The portable archive owns the executable, installer helpers, package documentation, manifest, and
the complete GPLv3 `LICENSE` and `THIRD-PARTY-NOTICES.md`. `SHA256SUMS.txt` and the versioned,
per-architecture SPDX JSON files are release-directory siblings generated outside the archive;
the installer does not copy them into the installed directory. Verify those sibling assets before
extracting or installing. The publication workflow refuses to publish while the root `LICENSE`
file is not tracked.

## Portable use

Extract the architecture-matched archive and run `IdleHarbor.exe`. Portable use does not create a
startup entry. It stores `IdleHarbor.ini` beside the executable when launched with `--portable`.
Use the tray menu or documented command-line options to control a session.

For the simplest persistent per-user setup without automatic startup, verify and extract the ZIP,
then run:

```powershell
.\install.ps1 -Startup None
```

This creates the default Start Menu launcher and leaves startup disabled. The ZIP remains the
canonical distribution; installation is optional.

## Per-user installation

From an extracted release directory, preview the changes:

```powershell
.\install.ps1 -Startup TaskScheduler -WhatIf
```

Install to the default `%LOCALAPPDATA%\Programs\IdleHarbor` destination:

```powershell
.\install.ps1 -Startup TaskScheduler
```

Installation is per-user and does not require elevation. The Start Menu launcher is created by
default; automatic startup remains disabled by default and is configured independently. Use
`-StartMenu None` when no launcher is wanted:

```powershell
.\install.ps1 -StartMenu None -Startup None
```

The default launcher is `%APPDATA%\Microsoft\Windows\Start Menu\Programs\IdleHarbor.lnk` and
opens the installed executable with `--show`, using the installation directory as its working
directory and the executable as its icon. Supported automatic-startup choices are:

- `TaskScheduler` — recommended least-privilege interactive task;
- `StartupFolder` — per-user `IdleHarbor.lnk`;
- `RunKey` — per-user `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` value;
- `None` — no startup registration.

Each startup choice runs `--start --minimized`. The installer removes only entries that point to the
same executable and refuses to overwrite a non-owned destination. The Start Menu path is preflighted
before any installation mutation: directories, reparse points, multiply-linked files, and foreign
shortcuts are rejected when creation is requested. An exact shortcut found without a prior installer
ownership claim is never claimed by the marker. `-StartMenu None` removes only a marker-claimed
shortcut that is still an exact match; changed or foreign shortcuts are preserved with a warning.
A first install invoked with the
source and destination set to the same directory also requires an already-valid IdleHarbor ownership
marker; this prevents a marker from claiming unrelated files in an extracted directory. Once that
marker exists, a same-directory reinstall remains supported. The installer writes an ownership marker
including the Start Menu path and ownership bit, and can be run repeatedly from Windows PowerShell
5.1 or PowerShell 7. Install and update operations snapshot managed files and owned startup state,
including exact Start Menu shortcut bytes, then restore them after a caught failure. A complete
rollback or successful commit removes the temporary transaction directory. If managed-file restoration
is incomplete, the installer retains that directory and reports its exact recovery path so the prior
bytes remain available for manual repair. Managed paths that are symbolic links, junctions, or
multiply linked files are rejected so an update cannot write through the installation boundary.
Unexpected user files are preserved.

On a managed laptop, do not choose Task Scheduler, StartupFolder, or RunKey unless policy permits
both IdleHarbor and that persistence mechanism. If endpoint controls block the executable or
installer, do not disable or independently whitelist around them.

Uninstall with:

```powershell
%LOCALAPPDATA%\Programs\IdleHarbor\uninstall.ps1
```

Settings are preserved by default. Add `-PurgeData` only when deleting local settings is intentional.
The uninstaller refuses to remove an unverified directory and preserves unexpected files. When the
installer created the Task Scheduler folder, rollback and uninstall also remove that folder after
proving it is empty; a pre-existing or non-empty folder is preserved.

## Cutting a release

The order matters, because the package-manager entries pin a SHA-256 that does not exist until the
release workflow has published the archive.

1. Bump `CMakeLists.txt`, `include/idleharbor/version.hpp`, `resources/IdleHarbor.rc`, and
   `resources/app.manifest`, date the `CHANGELOG.md` section, and update the version references in
   the documentation -- but *not* the five in `docs/index.html`, which are step 5. That page is the
   live site the moment this merges, and the release it would advertise does not exist until step 2.
   `Test-ReleaseVersion.ps1 -Tag vX.Y.Z` proves the four source files agree.
2. Merge that, then push the annotated tag `vX.Y.Z`. `release.yml` builds x64 and ARM64, packages
   the portable archives and SPDX SBOMs, generates `SHA256SUMS.txt`, attests every asset, and
   publishes the GitHub release.
3. Repoint `chocolatey/` at the published x64 archive: the nuspec version and its pinned URLs, the
   installer URL and `checksum64`, `VERIFICATION.txt`, and the uninstaller's versioned folder. Take
   the digest from the release's own `SHA256SUMS.txt`, then prove it against the real download:

   ```powershell
   .\Test-ChocolateyPackage.ps1 -VerifyPublishedChecksum
   ```

4. Submit or update the WinGet manifests in [`winget`](winget) once the release page is live.
5. Update the five version references in `docs/index.html` — the JSON-LD `softwareVersion`,
   `downloadUrl`, and `installUrl`, the download button's label, and the `gh attestation verify`
   example. The project site is served from `docs/`, so it is published by merging, not by a
   workflow.

Between steps 2 and 3 the Chocolatey package deliberately still names the previous release.
`Test-ChocolateyPackage.ps1` allows that and refuses the reverse — a package version ahead of the
source cannot correspond to an archive anyone can download.

## Verification and trust

After downloading, verify the checksum manifest and inspect the signature state:

```powershell
Get-FileHash .\IdleHarbor.exe -Algorithm SHA256
Get-AuthenticodeSignature .\IdleHarbor.exe
```

The release publishes SHA-256, SBOM, and attestation evidence. `v0.2.0` is
intentionally unsigned, so `Get-AuthenticodeSignature` is expected to report `NotSigned`. The source
and archives are licensed `GPL-3.0-only`; each portable archive includes the complete licence text.

Package-manager submission status is documented in the main README. Chocolatey package source and
its verification instructions live in [`chocolatey`](chocolatey); it installs the portable x64
build without enabling startup.
