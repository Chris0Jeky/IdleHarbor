# Installer previews

Use `install.ps1 -WhatIf` or `uninstall.ps1 -WhatIf` to preview an operation.
Final summaries say `Previewed`, not `Installed` or `Uninstalled`. Installation
previews describe the requested startup mode rather than implying it was applied.
The existing ShouldProcess gates still decide every mutation.

Uninstall preview subtracts only validated managed leaf paths and the ownership
marker from its simulated directory residue. Files retained solely by WhatIf are
not mislabeled as unexpected, while genuinely unowned files remain visible in
warnings and are preserved. This is an output simulation, not a deletion pass.
Real-operation summaries and ownership boundaries are unchanged.

`tests/Test-PackagingPreview.ps1` runs six actual fixture operations, including
fresh install, update and uninstall previews, real installation/removal, and
unowned residue. Hash snapshots prove preview preservation of source files,
managed files and ownership marker bytes. It disables startup and Start Menu
creation and isolates application/task discovery. It never launches a real app.
The suite runs in Windows PowerShell 5.1 and PowerShell 7 through the registered
script runner; the full packaging/rollback suite remains a separate CI step.
