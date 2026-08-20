# Chocolatey package

This directory is the reproducible source for the `idleharbor` Chocolatey community package. The
first package intentionally uses the x64 portable release; the native ARM64 archive remains
available from GitHub while Chocolatey's architecture-selection behavior is validated separately.

The package does not enable startup, create a scheduled task, or launch IdleHarbor during install.
Chocolatey creates the command shim; use `idleharbor --show` to open the settings window.

Build from the repository root:

```powershell
choco pack .\packaging\chocolatey\idleharbor.nuspec --output-directory .\out\chocolatey
```

Before publication, install and uninstall the generated package in a suitable isolated Windows
test environment, then push with the owner's Chocolatey API key:

```powershell
choco install idleharbor --source .\out\chocolatey --version 0.1.0 --yes
choco uninstall idleharbor --yes
choco push .\out\chocolatey\idleharbor.0.1.0.nupkg --source https://push.chocolatey.org/
```

The release URL, SHA-256, licence text, and verification instructions are included in the package.
