# Validation entry points

Run these from a checkout. Native builds require Visual Studio 2022 Build Tools;
Node checks require Node 22 or later and no third-party package installation.

```powershell
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64 -DIDLEHARBOR_BUILD_TESTS=ON
cmake --build build/x64 --config Release --parallel
ctest --test-dir build/x64 -C Release --output-on-failure
.\tests\Test-RegressionScripts.ps1
.\packaging\Test-Packaging.ps1
node --test tests/site_release_tests.mjs tests/winget_contract_tests.mjs
node tools/check-site-release.mjs
node packaging/check-winget.mjs
node observatory/check.mjs
```

Run PowerShell suites under both Windows PowerShell 5.1 and PowerShell 7. The
public Test-Packaging script launches its fixture implementation using the same
edition from PSHOME, without profiles, interactive prompts or a policy override.
It propagates a failed child exit. Do not call Invoke-PackagingFixtureTests.ps1
directly to test caller isolation. See [test isolation](TEST-ISOLATION.md).

The following commands deliberately require network access. They download/hash
published archives without extraction, execution, installation or publication:

```powershell
.\packaging\Test-ChocolateyPackage.ps1 -VerifyPublishedChecksum
node packaging/check-winget.mjs --verify-published-checksum
```

## Automated gates

CI builds x64/x86/ARM64, executes ten CTests on x64/x86, and runs the registered
script/packaging checks in both PowerShell editions. ARM64 execution is skipped,
not inferred from its successful cross-build. CodeQL analyzes the C++ build.

Published checksum runs on relevant PR/main changes, manual dispatch and a daily
04:17 UTC schedule. WinGet contracts runs its offline Windows/Linux cases and
published-archive checks on relevant changes, manual dispatch and daily at
04:31 UTC. Scheduled execution can be delayed. Both workflows are read-only and
disable persisted checkout credentials. A new pin should never be accepted merely
to silence a mismatch; compare the released trust assets and intended architecture.

The WinGet parser enforces the current tracked plain-scalar/ordered installer
layout. Keep upstream WinGet schema validation as a separate release step.
Metadata agreement cannot substitute for published-byte verification, and neither
can substitute for actual Chocolatey/WinGet installation and cleanup evidence.

Current implementation, exact-head evidence and unverified native/owner gates are
summarized in [PROJECT_STATE](../PROJECT_STATE.md). Re-run the relevant gates after
changing a head; a prior revision's log does not qualify a later revision.
