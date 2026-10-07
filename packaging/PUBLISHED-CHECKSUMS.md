# Published artifact verification

The ordinary packaging suite checks offline agreement among package metadata,
release URLs and `VERIFICATION.txt`. Agreement alone cannot prove that the pinned
digest belongs to the published archive rather than a consistently copied wrong
architecture's digest.

The `Published checksum` workflow runs the existing verifier with
`-VerifyPublishedChecksum`. It downloads the immutable release URL named by the
Chocolatey package and compares the bytes' SHA-256 with the actual installer pin.
It runs for packaging-related pull requests and main-branch changes, on manual
dispatch, and on a daily 04:17 UTC schedule (GitHub may delay scheduled execution).
The job has a five-minute timeout, read-only repository permissions and no saved
checkout credentials. It does not install, launch, publish, or rewrite a package.

Local network-enabled verification remains opt-in:

```powershell
.\packaging\Test-ChocolateyPackage.ps1 -VerifyPublishedChecksum
```

A failed download and a digest mismatch both fail the job. Diagnose a mismatch
against the release's trust assets; do not replace a pin merely to make CI green.
If the package intentionally targets the previous release during the repoint
window, this verifies that previous release's archive, not an unpublished build.
WinGet artifact checks and actual package-manager lifecycle proof are separate.
