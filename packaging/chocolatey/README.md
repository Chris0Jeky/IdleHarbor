# Chocolatey package

This directory is the reproducible source for the `idleharbor` Chocolatey community package. It
intentionally ships the x64 portable release only. Before any download, `chocolateyInstall.ps1`
reads the machine-wide `PROCESSOR_ARCHITECTURE` value from
`HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment` and accepts only `AMD64`.
This registry read avoids the process environment's WOW64/emulation rewriting: a Prism x64 process
on ARM64 can report `AMD64` with no `PROCESSOR_ARCHITEW6432`. Process values never override the
registry decision. An x86, ARM64, missing, unknown or unreadable native value fails closed with a
clear message, even if the process reports `PROCESSOR_ARCHITEW6432=AMD64`.

Offline tests inject native and process values, including the Prism case and registry read failure.
Real ARM64/Prism behaviour has not yet been observed on hardware. The native ARM64 archive remains
available from GitHub; a Chocolatey ARM64 strategy is assessed separately (issue #51).

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
