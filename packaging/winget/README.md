# WinGet manifests

This directory tracks the manifests submitted to
[microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) for the `Chris0Jeky.IdleHarbor`
package, so a submission can be reviewed and reproduced from this repository instead of only from
the upstream pull request.

Each released version gets its own directory, `<version>/`, holding the three schema files upstream
requires:

- `Chris0Jeky.IdleHarbor.yaml` - the version manifest;
- `Chris0Jeky.IdleHarbor.installer.yaml` - the installer manifest, including the per-architecture
  `InstallerSha256` values;
- `Chris0Jeky.IdleHarbor.locale.en-US.yaml` - the default locale manifest.

Upstream expects them at `manifests/c/Chris0Jeky/IdleHarbor/<version>/`.

## Preparing a submission

The installer digests come from the release's own `SHA256SUMS.txt`, so the manifests can only be
written after the release workflow has published the archives.

```powershell
winget validate --manifest .\packaging\winget\<version>
```

`winget validate` checks schema and required fields; it does not download the installers. A full
local install test additionally needs `LocalManifestFiles` enabled, which is an administrator
setting.

## Submission status

`0.1.0` was submitted as [winget-pkgs#421663](https://github.com/microsoft/winget-pkgs/pull/421663)
and is waiting on a community moderator. Because that is still a `New-Package` pull request, a later
version is not submitted alongside it: the package has to exist upstream before a version update can
target it. Prepare the new manifests here, and open the version-update pull request once the new
package request has been accepted.
