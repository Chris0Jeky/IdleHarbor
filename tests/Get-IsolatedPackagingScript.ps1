function Get-IsolatedPackagingScript([string]$Path, [string]$TestRoot, [string]$ProgramsPath) {
    $tokens = $null; $errors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile($Path, [ref]$tokens, [ref]$errors)
    if ($errors.Count) { throw "Cannot parse $Path" }
    $text = $ast.Extent.Text
    $startup = (Join-Path $TestRoot 'startup\IdleHarbor.lnk').Replace("'", "''")
    $programs = (Join-Path $ProgramsPath 'IdleHarbor.lnk').Replace("'", "''")
    # Execute the production lifecycle with injected discovery only. Do not access
    # real profile folders, Run keys, or installed/running IdleHarbor instances.
    $replacements = @{
        'Get-StartMenuLinkPath' = "return '$programs'"
        'Get-StartupLinkPath' = "return '$startup'"
        'Get-RunCommand' = 'return $null'
        'Get-OwnedProcesses' = 'param([string]$Executable); return'
    }
    $nodes = @($ast.FindAll({ param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] }, $true) |
        Where-Object { $replacements.ContainsKey($_.Name) } | Sort-Object { $_.Extent.StartOffset } -Descending)
    foreach ($name in $replacements.Keys) {
        if (@($nodes | Where-Object { $_.Name -eq $name }).Count -ne 1) {
            throw "Cannot isolate packaging boundary $name in $Path"
        }
    }
    foreach ($node in $nodes) {
        $replacement = 'function ' + $node.Name + ' { ' + $replacements[$node.Name] + ' }'
        $text = $text.Remove($node.Extent.StartOffset, $node.Extent.EndOffset - $node.Extent.StartOffset).Insert($node.Extent.StartOffset, $replacement)
    }
    return [scriptblock]::Create($text)
}
