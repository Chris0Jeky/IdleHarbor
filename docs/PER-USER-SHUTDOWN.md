# Per-user installer shutdown

Install and uninstall first identify an exact-path running application. Previewing
with `-WhatIf` does not launch an exit command. For a real operation, a discovered
instance with a foreign or unknown session is rejected before launch.

The newly launched `--exit` command has a 10-second wait. A timed-out command child
may be terminated; the discovered application is never force-terminated. A missing
child or nonzero exit code fails the operation. The command handle is disposed even
when waiting fails. After a successful command, a monotonic five-second poll checks
that the application has actually exited before file changes continue.

These budgets do not impose an absolute deadline on operating-system discovery or
process-creation calls. The existing per-user `Get-OwnedProcesses` discovery still
omits inaccessible process paths and suppressed enumeration errors. This change
bounds shutdown of identified instances; it does not claim fail-closed discovery
of every session or successful real Chocolatey orchestration (issue #51).

`tests/Test-PerUserShutdown.ps1` executes the production shutdown and ShouldProcess
functions with isolated process doubles and real polling delays. Its 24 cases run
in both PowerShell editions through `tests/Test-RegressionScripts.ps1`. The full
packaging suite separately exercises installation and rollback fixtures. Neither
suite replaces interactive Windows or package-manager lifecycle validation.
