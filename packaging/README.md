# IdleHarbor distribution

IdleHarbor is distributed as a native Windows executable with no application runtime or network
dependency. The portable archive is the canonical package; the PowerShell installer is an optional
per-user convenience layer around that archive.

## Portable use

Extract the architecture-matched archive and run `IdleHarbor.exe`. Portable use does not create a
startup entry. Use the tray menu or the documented command-line options to control a session.

## Per-user installation

From an extracted release directory, run:

```powershell
.\install.ps1 -Startup TaskScheduler
```

The default destination is `%LOCALAPPDATA%\Programs\IdleHarbor`. Installation is per-user and does
not require elevation. Supported startup modes are `TaskScheduler` (recommended), `StartupFolder`,
`RunKey`, and `None`. The scheduled task runs only for the signed-in user with least privilege.

Use `-WhatIf` to preview changes. Uninstall with:

```powershell
%LOCALAPPDATA%\Programs\IdleHarbor\uninstall.ps1
```

Settings are preserved by default. Add `-PurgeData` only when the settings and user data should be
removed. The scripts refuse to remove a directory without an IdleHarbor ownership marker and never
overwrite a startup entry that points somewhere else.

## Verification

Verify the release archive or executable against `SHA256SUMS.txt` before running it:

```powershell
Get-FileHash .\IdleHarbor.exe -Algorithm SHA256
Get-AuthenticodeSignature .\IdleHarbor.exe
```

Unsigned builds may produce a Windows SmartScreen warning. Do not disable endpoint protection to
run an unverified build; use the published checksum and provenance information instead.
