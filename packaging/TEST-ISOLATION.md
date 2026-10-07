# Packaging test isolation

Use `packaging/Test-Packaging.ps1` as the public test entry point. It runs the
fixture implementation in a fresh process using the current PowerShell edition's
executable from PSHOME, with no profile and no interactive prompts. It does not
change execution policy. Native exit failures are propagated as test failures.

`Invoke-PackagingFixtureTests.ps1` preserves the original suite, including its
module-restoration logic, at its exact original blob. It is an internal entry
point: direct invocation deliberately bypasses caller isolation. Both files stay
in packaging so fixture paths and script discovery retain their original meaning.

A child process contains global command doubles, the fixture variable and module
auto-loading even if setup or cleanup fails. This is stronger than trying to
restore only the ScheduledTasks module after overwriting a custom caller command.
The same temporary-file ownership, rollback checks and test mutex still apply
inside the child. Process isolation does not sandbox the filesystem or Windows
APIs; the fixture suite's existing safety boundaries remain necessary.

`tests/Test-PackagingIsolation.ps1` runs the full suite with a caller-owned global
function/value and again with the real ScheduledTasks binding, then performs a
real read-only task query. It also verifies a failing child's exit propagation,
unchanged caller state after failure, edition matching and script paths containing
spaces. These checks run under both Windows PowerShell 5.1 and PowerShell 7.
