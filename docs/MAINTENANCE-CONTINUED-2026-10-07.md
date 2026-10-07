# Maintenance continuation: 2026-10-07

This continues the [first maintenance handoff](MAINTENANCE-2026-10-07.md).
GitHub is authoritative for heads, issues, reviews and merge evidence. The final
consolidation incorporates main 64c0f9514c30c6e5b64cc233b1bad0e5a06f9a02,
including every implementation below. Its own exact-head validation is recorded
in PR #114 rather than inferred from prior runs.

No release, marketplace publication, installed-user update, new hosting activation
or native screenshot recapture occurred in this continuation.

## Qualified deliveries

| PR | Delivered change | Qualified head | CI / CodeQL runs |
| --- | --- | --- | --- |
| [#103](https://github.com/Chris0Jeky/IdleHarbor/pull/103) | Single-dash config filenames; 109 parser cases | 6e90fc53 | 37553943561 / 37553943562 |
| [#104](https://github.com/Chris0Jeky/IdleHarbor/pull/104) | Bounded per-user shutdown; 24 cases | 9ce3f3e2 | 37554898796 / 37554898860 |
| [#105](https://github.com/Chris0Jeky/IdleHarbor/pull/105) | Complete normalized GPLv3 integrity; nine fixtures | df0b562a | 37555835918 / 37555835898 |
| [#106](https://github.com/Chris0Jeky/IdleHarbor/pull/106) | Automated actual Chocolatey archive verification | 43321a32 | 37555638972 / 37555638967 |
| [#107](https://github.com/Chris0Jeky/IdleHarbor/pull/107) | Bounded release lag; 14 cases | a5e4c5e5 | 37558384806 / 37558384778 |
| [#108](https://github.com/Chris0Jeky/IdleHarbor/pull/108) | WinGet contracts; 28 cases and four archive checks | 0555820c | 37558519875 / 37558519800 |
| [#109](https://github.com/Chris0Jeky/IdleHarbor/pull/109) | Truthful previews and owned/unowned residue | 3e27625b | 37559729337 / 37559729341 |
| [#112](https://github.com/Chris0Jeky/IdleHarbor/pull/112) | Retryable power cleanup, interval validation, pause-status coverage | 2bb90eb9 | 37560674412 / 37560674540 |
| [#111](https://github.com/Chris0Jeky/IdleHarbor/pull/111) | Caller-isolated packaging fixture process | 2c408142 | 37561691814 / 37561691781 |
| [#113](https://github.com/Chris0Jeky/IdleHarbor/pull/113) | Deterministic capture JSON and committed-image evidence checks | df5f0e37 | 37562787440 / 37562787446 |

All listed heads passed the named runs before merge. PR descriptions retain full
SHAs, inspected job IDs, synthetic-merge identities and scope limits. Empty review
threads are not independent approval. Later heads require their own validation.

## Regression and integration evidence

The CLI fixture reproduced five filename failures before #103; per-user shutdown
reproduced 20 of 24 failures before #104. License tests showed four corrupted texts
passed the old marker-only guard before #105. Its independent byte-level reference
is Debian's installed GPL-3 text, not a successful GNU endpoint download.

#107's test-first run 37558115481 at a53f6992 passed current/predecessor windows but
exposed nine stale or malformed-history acceptance gaps. Fixed run 37558384806
passed all 14 cases in both editions; PS5.1 job 112589809899 was inspected.

#109's run 37559218065 at b032a93a exposed ten summary/residue failures while every
byte-preservation and real-operation check passed. Fixed run 37559729337 passed
six fixture operations; PS5.1 job 112594051986 was inspected. The consolidation
strengthens those snapshots with LastWriteTimeUtc ticks, retaining #110's useful
extra assertion. Its earlier head d1bae7de passed all four workflows, including
CI 37562443605; PS5.1 job 112602532734 was inspected. That earlier result does not
replace the refreshed final integrated-head proof in #114.

#112's tests failed 15 power-cleanup and 12 interval-bound assertions against
unchanged sources. Fixed suites passed GCC and Clang C++20 with warnings as errors.
Windows job 112596967872 built the real application and the private API-double
target, then passed all ten CTests. All-pause-reason tests add coverage; existing
status formatting was not changed. UI cleanup-error presentation remains separate.

#111 originally restored the ScheduledTasks module but not a custom caller function
or fixture variable. Run 37561376196 at 8170e905 reproduced those two losses after
a full suite; the native-module query already passed. Fixed PS5.1 job 112600168935
in run 37561691814 proves caller state survives success and child failure, nonzero
exits stay failures, spaced paths and edition matching work, and a real read-only
ScheduledTasks query succeeds afterward. PS7 passes too. Original implementation
blob 523fcc2ef05030a8b025f4b549b71702096014a6 remains unchanged behind the launcher.

#113 retains the original worker's formatter, exact-layout fixture and manifest
rewrite. Reconciliation keeps every newer suite registration. Inspected PS7 job
112603633415 in run 37562787440 passes the fixed expected layout, ten extra formatter
edge cases, all five committed PNG hashes/dimensions, prior suites and packaging.
The PS5.1 lane passes the same checks. The manifest rewrite was independently
reproduced from the original parsed values at exact blob 1d80a686; it changes no
evidence data. This is serialization and existing-image identity proof, not a
new desktop capture. See [capture validation](CAPTURE-MANIFEST-VALIDATION.md).

## Actual archive bytes

Published-checksum run 37555639019 downloaded the Chocolatey v0.2.0 x64 ZIP and
verified its pin. WinGet run 37558519990, inspected job 112590234131, downloaded all
four manifest targets and compared streamed bytes with their SHA-256 pins:

| Version / architecture | Bytes | SHA-256 |
| --- | --- | --- |
| 0.1.0 / x64 | 311191 | b3bc7e5714543cee3877e94f3c74ecfedb30d3599decef316e57715fdf9f6d28 |
| 0.1.0 / ARM64 | 286414 | 7109d42e248568027581a9110ebdb357c2d908c02872708a00fa9acfda7c904b |
| 0.2.0 / x64 | 317712 | 18b4a517ef767c005a6d01ba53c13a79ff50e4a956bd7c43f3635b17b80c75f7 |
| 0.2.0 / ARM64 | 292440 | 419b9ec0be40546aa262c3dddfb34fb648c47775459a7ff66d31c16f66dc3d38 |

No ZIP was extracted or executed. Metadata agreement, successful downloads, schema
validation and actual package-manager installation are distinct acceptance gates.

## Concurrent contribution reconciliation

#110 duplicates the preview implementation already merged in #109. Its timestamp
check is retained without replacing #109's qualified behavior and genuine-unowned
residue warnings. #114 records the final superseding disposition. #111 and #113
were advanced on their existing branches, retaining their original contributions,
not replaced with competing PRs or presented as independent reviews.

CHANGELOG records the continuation in Unreleased without changing historical dated
releases. PROJECT_STATE separates implementation, test doubles, downloaded bytes,
native gaps and owner actions. The [validation guide](../packaging/VALIDATION.md)
names the public entry points and workflows. The first handoff and archived state
are retained as historical evidence rather than overwritten with current claims.

## Remaining acceptance gates

#40, #66, #65, #68, #38, #48 and #70 have qualified fixes above. #86 stays open for
unhook-handle recovery, main.cpp helper tests and visible cleanup-error reporting.
Low-level PowerRequest state does not prove all UI callers present failures.

#77's unbounded per-user and Chocolatey copies are fixed, but actual Chocolatey
hook-abort and lifecycle behavior remains with #51. Per-user discovery suppresses
inaccessible paths and enumeration errors; do not claim universally fail-closed
lookup or hard wall-clock deadlines for OS discovery calls.

#42/#46 concern tray/accessible status, #55/#56 focus/combo interactions, #37 known-
folder ownership relocation, and #49/#50/#59 native capture and harness behavior.
These need their specific evidence, not inference from a headless build. ARM64
still has cross-build evidence, not ARM64 runtime execution. Owner credentials,
marketplace publication, Search Console and #6's social-preview upload remain
outside this maintenance work.
