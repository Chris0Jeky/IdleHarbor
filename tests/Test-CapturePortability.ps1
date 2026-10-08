[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$checks = 0
function Assert-True([bool]$Condition, [string]$Message) {
    $script:checks++
    if (-not $Condition) { throw $Message }
}

# Load only production functions: never run the capture workflow or touch the desktop.
$source = Join-Path (Split-Path -Parent $PSScriptRoot) 'tools\Capture-IdleHarborScreenshots.ps1'
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($source, [ref]$tokens, [ref]$errors)
Assert-True ($errors.Count -eq 0) 'The capture script must parse.'
foreach ($name in @('Initialize-CaptureNative', 'ConvertTo-CaptureNativeArgument', 'Write-CaptureSettings', 'Start-CaptureOwner')) {
    $definition = $ast.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
    }, $true)
    Assert-True ($null -ne $definition) "Missing production function: $name"
    . ([scriptblock]::Create($definition.Extent.Text))
}

Initialize-CaptureNative
$firstType = 'IdleHarbor.Capture.Native' -as [type]
Initialize-CaptureNative
Assert-True ($null -ne $firstType) 'The first helper initialization must compile the real type.'
Assert-True ([object]::ReferenceEquals($firstType, ('IdleHarbor.Capture.Native' -as [type]))) 'The second initialization must reuse the same type.'

$tempParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$tempRoot = Join-Path $tempParent ('IdleHarbor capture portability ' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot | Out-Null
$launchInterceptActive = $false
try {
    # Build a harmless .NET Framework console probe in Windows PowerShell, even
    # when this suite runs in PowerShell 7. Its Main receives the actual native argv.
    $probe = Join-Path $tempRoot 'argument probe.exe'
    $compiler = Join-Path $tempRoot 'compile probe.ps1'
    @'
param([string]$Output)
$ErrorActionPreference = 'Stop'
Add-Type -OutputAssembly $Output -OutputType ConsoleApplication -TypeDefinition @"
using System;
using System.IO;
using System.Text;
public static class CaptureArgumentProbe {
    public static int Main(string[] args) {
        if (args.Length < 1) return 2;
        string[] encoded = new string[args.Length - 1];
        for (int i = 1; i < args.Length; ++i)
            encoded[i - 1] = Convert.ToBase64String(Encoding.Unicode.GetBytes(args[i]));
        File.WriteAllLines(args[0], encoded);
        return 0;
    }
}
"@
'@ | Set-Content -LiteralPath $compiler -Encoding UTF8
    $windowsPowerShell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $compileArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        (ConvertTo-CaptureNativeArgument $compiler), (ConvertTo-CaptureNativeArgument $probe))
    $compile = Microsoft.PowerShell.Management\Start-Process -FilePath $windowsPowerShell -ArgumentList $compileArgs -PassThru -WindowStyle Hidden
    Assert-True ($compile.WaitForExit(30000)) 'Probe compilation must finish within 30 seconds.'
    Assert-True ($compile.ExitCode -eq 0 -and (Test-Path -LiteralPath $probe)) 'Native argument probe must compile.'
    $compile.Dispose()

    $config = Join-Path $tempRoot 'configuration path with spaces.ini'
    foreach ($paused in @($false, $true)) {
        Write-CaptureSettings $config $paused
        $settings = [IO.File]::ReadAllText($config)
        Assert-True ($settings -match '(?m)^emergency_hotkey=false\r?$') 'Capture settings must not claim the emergency hotkey.'
        Assert-True ($settings -match '(?m)^pause_when_locked=true\r?$' -and $settings -match '(?m)^pause_when_disconnected=true\r?$') 'Capture safeguards remain enabled.'
    }

    # Intercept only the launch site to collect its production ArgumentList, then
    # send that exact list to the real probe. No owner is launched or window inspected.
    $executablePath = $probe
    $script:ownerArguments = $null
    function Start-Process {
        param($FilePath, $ArgumentList, [switch]$PassThru)
        $script:ownerArguments = $ArgumentList
        throw 'CapturePortabilityLaunchIntercept'
    }
    $launchInterceptActive = $true
    try { Start-CaptureOwner $config; throw 'Expected launch interception.' }
    catch { Assert-True ($_.Exception.Message -eq 'CapturePortabilityLaunchIntercept') 'Intercept the real owner launch before desktop work.' }
    Remove-Item Function:\Start-Process
    $launchInterceptActive = $false

    $output = Join-Path $tempRoot 'received arguments.txt'
    $probeArgs = @((ConvertTo-CaptureNativeArgument $output)) + $script:ownerArguments
    $process = Microsoft.PowerShell.Management\Start-Process -FilePath $probe -ArgumentList $probeArgs -PassThru -WindowStyle Hidden
    Assert-True ($process.WaitForExit(5000)) 'Argument probe must finish within five seconds.'
    Assert-True ($process.ExitCode -eq 0) 'Argument probe succeeds.'
    $process.Dispose()
    $received = @([IO.File]::ReadAllLines($output) | ForEach-Object { [Text.Encoding]::Unicode.GetString([Convert]::FromBase64String($_)) })
    Assert-True ($received.Count -eq 3) 'Owner invocation has exactly three native arguments.'
    Assert-True ($received[0] -ceq '--show' -and $received[1] -ceq '--config' -and $received[2] -ceq $config) 'The real launch retains the entire spaced configuration path.'

    $unquotedArgs = @((ConvertTo-CaptureNativeArgument $output), '--show', '--config', $config)
    $process = Microsoft.PowerShell.Management\Start-Process -FilePath $probe -ArgumentList $unquotedArgs -PassThru -WindowStyle Hidden
    Assert-True ($process.WaitForExit(5000)) 'The old unquoted invocation finishes.'
    Assert-True ($process.ExitCode -eq 0) 'The old invocation reaches the native argv boundary.'
    $process.Dispose()
    Assert-True ([IO.File]::ReadAllLines($output).Count -gt 3) 'Negative control reproduces the old spaced-path splitting defect.'

    # Discriminating CRT cases: empty arguments, embedded quotes, and backslashes
    # before quotes or the closing delimiter must survive as literal values.
    $values = @('', 'plain', 'two words', 'C:\with spaces\', 'say "hi"', 'slash\"quote')
    $probeArgs = @((ConvertTo-CaptureNativeArgument $output)) + @($values | ForEach-Object { ConvertTo-CaptureNativeArgument $_ })
    $process = Microsoft.PowerShell.Management\Start-Process -FilePath $probe -ArgumentList $probeArgs -PassThru -WindowStyle Hidden
    Assert-True ($process.WaitForExit(5000)) 'Edge-case probe finishes.'
    Assert-True ($process.ExitCode -eq 0) 'Edge-case probe succeeds.'
    $process.Dispose()
    $received = @([IO.File]::ReadAllLines($output) | ForEach-Object { [Text.Encoding]::Unicode.GetString([Convert]::FromBase64String($_)) })
    Assert-True ($received.Count -eq $values.Count) 'Empty values retain their argv position.'
    for ($index = 0; $index -lt $values.Count; $index++) {
        Assert-True ($received[$index] -ceq $values[$index]) "Native argv edge case $index round-trips."
    }
}
finally {
    if ($launchInterceptActive) { Remove-Item Function:\Start-Process }
    $resolved = [IO.Path]::GetFullPath($tempRoot)
    if ([IO.Path]::GetDirectoryName($resolved) -ne $tempParent.TrimEnd('\') -or
        [IO.Path]::GetFileName($resolved) -notlike 'IdleHarbor capture portability *') {
        throw 'Refusing cleanup outside the owned temporary fixture directory.'
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
Write-Host "Capture portability passed ($checks checks; no capture, input, foreground or IdleHarbor launch)."
