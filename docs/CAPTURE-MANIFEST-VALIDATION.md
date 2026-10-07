# Capture manifest validation

The capture tool's pure formatter uses a fixed two-space, LF JSON representation
for its ordered manifest values. It is exercised without launching a window,
changing foreground focus, moving a pointer or recapturing any image.

The original exact-layout/round-trip suite and additional scalar/control-character/
Unicode/integer cases are registered in Test-RegressionScripts for both Windows
PowerShell 5.1 and PowerShell 7. The added evidence check reads the five committed
PNGs, verifies their PNG headers, dimensions and SHA-256 against the manifest, and
rejects duplicate or unsafe filenames. This validates existing evidence identity,
not the present application's appearance or a fresh desktop capture.

The manifest rewrite preserves all existing data and only changes formatting.
The formatter is scoped to the capture tool's string, integer, boolean, null,
ordered-object and array values; it is not a replacement for a general-purpose
JSON serializer. Date/time values in the production manifest are explicitly
converted to strings before formatting. Upstream desktop capture guards, staged
promotion and cleanup remain unchanged.

The continuation handoff records the original worker contribution in PR #113;
that PR's final description records reconciliation and both-edition CI evidence.
Issue #70 concerns serialization. Native capture portability, corner privacy and
harness refactoring remain separately tracked under #49, #50 and #59.
