# WinGet metadata and published-byte checks

Run `node packaging/check-winget.mjs` for offline validation and
`node --test tests/winget_contract_tests.mjs` for regression coverage. Node 22 or
later is required; no package installation or third-party dependency is needed.

For every version directory, the checker requires exactly the tracked version,
installer and en-US default-locale files. It checks identity/version/schema
agreement, pinned license and release-notes URLs, two distinct x64/ARM64 records,
architecture-specific archive URLs and nested executable paths, the portable
command alias, and SHA-256 syntax. It validates the repository's existing plain
scalar layout and field ordering, not every legal YAML spelling. Unsupported
installer layouts fail rather than being partially interpreted. General WinGet
schema validation remains a separate release step.

Offline consistency cannot prove that a well-formed digest is correct. Run
`node packaging/check-winget.mjs --verify-published-checksum` to stream each
validated archive, hash its actual bytes, and compare the result with the pin.
Downloads have a 45-second request budget and 32 MiB byte limit. They are never
extracted or executed. A mismatch or failed HTTP request fails the command.

The WinGet contracts workflow runs offline cases on Windows and Linux, plus the
published-byte check on Windows, for relevant changes, manual runs and daily.
It uses read-only repository permissions and no persisted checkout credentials.
The normal CI/CodeQL workflows remain intact. These checks do not prove package
manager installation, command-shim behavior, or marketplace acceptance, and do
not repoint manifests or publish packages.
