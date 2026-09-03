# Chocolatey package

This directory is the reproducible source for the `idleharbor` Chocolatey community package. It
intentionally ships the x64 portable release only, so `choco install idleharbor` has no download URL
on a 32-bit Windows install; the native ARM64 archive remains available from GitHub while
Chocolatey's architecture-selection behavior is validated separately (issue #51).

The package does not enable startup, create a scheduled task, or launch IdleHarbor during install.
Chocolatey creates the command shim; use `idleharbor --show` to open the settings window.

`chocolateyBeforeModify.ps1` runs from the *installed* package before an upgrade or an uninstall and
closes a running IdleHarbor with its own `--exit` command. `choco upgrade` never runs
`chocolateyUninstall.ps1`, so without that hook a running executable would hold a lock on the package
directory it lives in.

`--exit` only reaches an instance in the caller's own Windows session, because IdleHarbor's
single-instance guard is a session-local mutex and its command goes to a window on that session's
desktop. The hook therefore checks the session of every process it finds and fails with a clear
message when IdleHarbor is running in another one, and it waits a bounded time for `--exit` instead
of forever, so an unattended upgrade reports an error rather than blocking on a dialog nobody can
see.

Build from the repository root:

```powershell
choco pack .\packaging\chocolatey\idleharbor.nuspec --output-directory .\out\chocolatey
```

Before publication, install and uninstall the generated package in a suitable isolated Windows
test environment, then push with the owner's Chocolatey API key:

```powershell
choco install idleharbor --source .\out\chocolatey --version 0.2.0 --yes
choco uninstall idleharbor --yes
choco push .\out\chocolatey\idleharbor.0.2.0.nupkg --source https://push.chocolatey.org/
```

The release URL, SHA-256, licence text, and verification instructions are included in the package.
