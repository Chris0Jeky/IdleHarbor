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

`0.1.0` was submitted as [winget-pkgs#421663](https://github.com/microsoft/winget-pkgs/pull/421663).
Every upstream validation check passed and it is waiting on a community moderator, who are
volunteers.

`0.2.0` is prepared in `0.2.0/` and passes `winget validate`, but is deliberately not submitted yet.
`#421663` is still a `New-Package` pull request, so the package does not exist upstream and there is
nothing for a version update to target. Opening a second `New-Package` request for the same
identifier would put two competing submissions in the moderation queue. Open the version-update pull
request once `#421663` has been accepted; both versions then coexist upstream and `winget install`
resolves to the newer one.
