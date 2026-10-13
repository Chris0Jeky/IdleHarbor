# Removes one IdleHarbor test directory under the user temp path.
# Returns $null on success. Returns a refusal or removal error and does not
# throw, so a caller can invoke it from finally without hiding the test failure.
function Remove-IdleHarborScopedTemp {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$LeafPattern,

        [Parameter(Mandatory)]
        [string]$RefusalNoun
    )

    $temporaryPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    $resolved = [IO.Path]::GetFullPath($Path)
    $underTemp = $resolved.StartsWith($temporaryPrefix, [StringComparison]::OrdinalIgnoreCase)
    $leafMatches = (Split-Path -Leaf $resolved) -like $LeafPattern
    if (-not ($underTemp -and $leafMatches)) {
        return "Refusing to remove an unexpected ${RefusalNoun}: $resolved"
    }
    if (-not (Test-Path -LiteralPath $resolved)) {
        return $null
    }
    try {
        Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction Stop
    }
    catch {
        return "Could not remove ${RefusalNoun}: $resolved. $($_.Exception.Message)"
    }
    return $null
}
