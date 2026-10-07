# Complete GPLv3 text integrity

The publication gate requires a tracked root `LICENSE` and compares its complete
normalized text with a fixed SHA-256. Recognizable headings alone cannot detect a
missing section or an altered clause.

Canonical reference: GNU GPL version 3, 29 June 2007, plain-text distribution at
https://www.gnu.org/licenses/gpl-3.0.txt (readable reference:
https://www.gnu.org/licenses/gpl-3.0.html).

The accepted LF UTF-8 representation is 35,149 bytes with SHA-256:

```text
3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986
```

On 2026-10-07, the tracked text was compared byte-for-byte with the independently
installed Debian `/usr/share/common-licenses/GPL-3`; both files had this digest.
The GNU plain-text endpoint was not successfully downloaded during that check, so
that local independent-file comparison is the recorded byte-level provenance.
The digest is not generated from each candidate during validation, and no network
request is made by the publication gate.

The gate decodes text, replaces CRLF with LF, and hashes UTF-8 without a BOM. It
accepts the normal LF/CRLF and UTF-8 BOM checkout variants. It does not trim, reflow,
remove clauses, or ignore appended content. The existing license choice remains
GPL-3.0-only; neither this check nor the tests modify the root license.

`tests/Test-ReleaseLicense.ps1` runs nine isolated tracked/untracked Git fixtures
in both PowerShell editions, including header-only text, a missing section17,
changed clauses and appended text. Do not update the reference digest merely to
accept a modified candidate. Any intended reference-format change requires an
independent full-text comparison, documentation and matching regression fixtures.
