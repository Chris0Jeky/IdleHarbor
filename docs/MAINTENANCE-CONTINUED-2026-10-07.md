# Maintenance continuation: 2026-10-07

This continues the [first maintenance handoff](MAINTENANCE-2026-10-07.md).
GitHub is authoritative for heads, issue state, reviews and merge evidence.
The consolidation starts from main fb116642fa662b28319ef09925dd812addbd8ff4,
which includes every delivery below. No release, marketplace publication,
installed-user update, new hosting activation or native screenshot capture occurred.

## Qualified deliveries

| PR | Delivered change | Qualified head | CI / CodeQL runs |
| --- | --- | --- | --- |
| [#103](https://github.com/Chris0Jeky/IdleHarbor/pull/103) | Preserve single-dash config filenames; 109 parser cases | 6e90fc53 | 37553943561 / 37553943562 |
| [#104](https://github.com/Chris0Jeky/IdleHarbor/pull/104) | Bound per-user shutdown; 24 cases | 9ce3f3e2 | 37554898796 / 37554898860 |
| [#105](https://github.com/Chris0Jeky/IdleHarbor/pull/105) | Complete normalized GPLv3 integrity; nine fixtures | df0b562a | 37555835918 / 37555835898 |
| [#106](https://github.com/Chris0Jeky/IdleHarbor/pull/106) | Automated actual Chocolatey archive verification | 43321a32 | 37555638972 / 37555638967 |
| [#107](https://github.com/Chris0Jeky/IdleHarbor/pull/107) | Bounded release lag; 14 cases | a5e4c5e5 | 37558384806 / 37558384778 |
| [#108](https://github.com/Chris0Jeky/IdleHarbor/pull/108) | WinGet contracts; 28 cases and four archive checks | 0555820c | 37558519875 / 37558519800 |
| [#109](https://github.com/Chris0Jeky/IdleHarbor/pull/109) | Truthful preview summaries and owned/unowned residue | 3e27625b | 37559729337 / 37559729341 |
| [#112](https://github.com/Chris0Jeky/IdleHarbor/pull/112) | Retryable failed power cleanup, interval validation, pause-status coverage | 2bb90eb9 | 37560674412 / 37560674540 |
| [#111](https://github.com/Chris0Jeky/IdleHarbor/pull/111) | Caller-isolated packaging fixture process | 2c408142 | 37561691814 / 37561691781 |

All listed heads passed the named runs before merge. PR descriptions retain full
SHAs, inspected job IDs, synthetic-merge identities and scope limitations.
Empty review threads are not independent approval. Subsequent heads need their
own validation; this table is evidence for the listed implementation revisions.

## Regression evidence

The CLI fixture reproduced five filename failures before #103; per-user shutdown
reproduced 20 of 24 failures before #104. License tests showed four corrupted texts
passed the old marker-only guard before #105. Its independent byte-level reference
is Debian's installed GPL-3 text, not a successful GNU endpoint download.

#107's test-first run 37558115481 at a53f6992 passed allowed current/predecessor
windows but exposed nine stale or malformed-history acceptance gaps. Fixed run
37558384806 passed all 14 cases in both editions; PS5.1 job112589809899 was inspected.

#109's run 37559218065 at b032a93a exposed ten misleading-summary/residue assertions
while every byte-preservation and real-operation check passed. Fixed run
37559729337 passed the six fixture operations; PS5.1 job112594051986 was inspected.
The consolidation strengthens the same snapshots with LastWriteTimeUtc ticks,
retaining the useful extra timestamp assertion from concurrent #110.

#112's new tests failed 15 power-cleanup assertions and 12 interval-bound assertions
against unchanged sources. The fixes passed GCC and Clang C++20 with warnings as
errors. Windows job112596967872 built both the real application and the test-private
API-double executable, then passed all ten CTests. The all-pause-reason tests add
coverage; existing status formatting was not broken and was not changed.

#111 originally restored the ScheduledTasks module but not a custom caller-owned
function or fixture variable. Test-first run37561376196 at8170e905 reproduced those
two losses after a complete suite, while the native-module query already passed.
Fixed PS5.1 job112600168935 in run37561691814 proves caller state survives success
and child failure, nonzero exits stay failures, spaced paths and the same edition
work, and a real read-only ScheduledTasks query succeeds after the suite. PS7 also
passes. The original implementation blob523fcc2ef05030a8b025f4b549b71702096014a6
is retained unchanged as Invoke-PackagingFixtureTests.ps1 behind the public launcher.

## Actual archive bytes

The published-checksum run37555639019 downloaded the Chocolatey v0.2.0 x64 archive
and verified its pin. WinGet run37558519990, inspected job112590234131, downloaded
all four manifest targets and compared streamed bytes with their SHA-256 pins:

| Version / architecture | Bytes | SHA-256 |
| --- | --- | --- |
| 0.1.0 / x64 | 311191 | b3bc7e5714543cee3877e94f3c74ecfedb30d3599decef316e57715fdf9f6d28 |
| 0.1.0 / ARM64 | 286414 | 7109d42e248568027581a9110ebdb357c2d908c02872708a00fa9acfda7c904b |
| 0.2.0 / x64 | 317712 | 18b4a517ef767c005a6d01ba53c13a79ff50e4a956bd7c43f3635b17b80c75f7 |
| 0.2.0 / ARM64 | 292440 | 419b9ec0be40546aa262c3dddfb34fb648c47775459a7ff66d31c16f66dc3d38 |

No ZIP was extracted or executed by those verification jobs. Offline metadata
agreement, successful downloads, upstream schema validation and actual package
manager installation are different acceptance gates.

## Concurrent contribution reconciliation

#110 duplicates the preview production changes already merged in #109. Its
additional timestamp-stability check is retained here without replacing #109's
qualified behavior, including warnings for genuinely unowned residue. The
consolidation PR records final validation and the superseding disposition.
#111 was advanced on its existing branch rather than replaced with a competing PR;
its original module-restoration work is preserved in the internal fixture source.

CHANGELOG now records the complete continuation in Unreleased without changing
historical dated releases. PROJECT_STATE distinguishes implementation, test doubles,
real downloaded-byte evidence, native validation gaps and owner actions. The
[validation guide](../packaging/VALIDATION.md) names the public commands and workflows.

## Remaining acceptance gates

Issues #40, #66, #65, #68, #38 and #48 have qualified fixes in the PRs above.
#86 remains open for unhook-handle recovery, main.cpp helper tests and visible
cleanup-error presentation. Low-level PowerRequest state tracking does not prove
every UI caller of void Clear presents failure correctly.

#77's unbounded per-user and Chocolatey copies are fixed, but the actual Chocolatey
hook-abort and lifecycle question remains with #51. Per-user discovery still
suppresses inaccessible paths and enumeration errors. Do not claim universally
fail-closed lookup or a hard wall-clock deadline for OS discovery calls.

#42/#46 concern tray/accessible status, #55/#56 focus/combo interactions, #37
known-folder ownership relocation, and #49/#50/#59/#70 capture and native harness
behavior. These need their corresponding tests rather than inference from a
headless build. ARM64 still has cross-build evidence, not ARM64 runtime execution.
Owner-only credentials, marketplace publication, Search Console and #6's social
preview Settings upload remain outside this maintenance work.
