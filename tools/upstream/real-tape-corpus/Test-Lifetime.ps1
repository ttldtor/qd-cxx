#requires -Version 7.4
<#
.SYNOPSIS
Reproduces inherited Java pipes and timeout against the corpus launcher's actual Native function.
.DESCRIPTION
Uses an explicit native UTC runner, JDK 8 and a new .work directory. Compiles a finite test child,
then checks root exit with a child retaining stdout/stderr and active-root timeout. The runner
owns the Java tree before execution. Tests inspect only their recorded parent/child process IDs.
.PARAMETER Runner
Explicit qdcxx-run-utc executable. No download or build is performed.
.PARAMETER JavaHome
Existing JDK 8 path; javac/java are run with finite deadlines through the runner.
.PARAMETER RunDirectory
New directory below repository .work for logs, classes, test PID files and metadata.
.PARAMETER Help
Print usage without resolving inputs or writing files. No arguments is equivalent.
.NOTES
Exit 0: help or both regressions passed. Exit 1: failed check/input. Writes only its new private
directory. Root-exit case has a 3-second budget; timeout case has 1 second plus bounded cleanup.
#>
[CmdletBinding()]
param([string]$Runner, [string]$JavaHome, [string]$RunDirectory, [switch]$Help)
if ($Help -or $PSBoundParameters.Count -eq 0) {
    'Test-Lifetime.ps1 -Runner QDCXX_RUN_UTC -JavaHome JDK8 -RunDirectory NEW_WORK_PATH; no arguments / -Help: help only.'
    return
}
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (!$Runner -or !$JavaHome -or !$RunDirectory) { throw 'Runner, JavaHome and RunDirectory required' }
if (!$IsWindows) { throw 'This forced-runner timeout regression is qualified on Windows only' }
$repo = (Resolve-Path "$PSScriptRoot/../../..").Path
$RunDirectory = [IO.Path]::GetFullPath($RunDirectory)
if (!$RunDirectory.StartsWith((Join-Path $repo '.work') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $RunDirectory)) {
    throw 'RunDirectory must be new and below repository .work'
}
$Runner = (Resolve-Path -LiteralPath $Runner).Path
$JavaHome = (Resolve-Path -LiteralPath $JavaHome).Path
$ancestor = $RunDirectory
while ($ancestor) {
    if ((Test-Path -LiteralPath $ancestor) -and ((Get-Item -LiteralPath $ancestor).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Reparse point in working path' }
    $parent = Split-Path $ancestor -Parent
    if (!$parent -or $parent -eq $ancestor) { break }
    $ancestor = $parent
}
New-Item -ItemType Directory -Path $RunDirectory | Out-Null
New-Item -ItemType Directory -Path "$RunDirectory/raw", "$RunDirectory/build-work", "$RunDirectory/harness" | Out-Null
$metadata = [ordered]@{ state = 'INCOMPLETE'; commands = [Collections.Generic.List[object]]::new(); cases = [Collections.Generic.List[object]]::new()
    launcherSha256 = (Get-FileHash "$PSScriptRoot/Invoke-Corpus.ps1" -Algorithm SHA256).Hash.ToLowerInvariant()
    probeSourceSha256 = (Get-FileHash "$PSScriptRoot/tests/ProcessLifetimeProbe.java" -Algorithm SHA256).Hash.ToLowerInvariant()
    runnerSha256 = (Get-FileHash $Runner -Algorithm SHA256).Hash.ToLowerInvariant()
}
# Parse the actual production function rather than copying the implementation into this test.
$ast = [Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot/Invoke-Corpus.ps1", [ref]$null, [ref]$null)
$function = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Native' }, $true)
Invoke-Expression $function.Extent.Text
try {
    $source = [IO.Path]::GetRelativePath("$RunDirectory/build-work", "$PSScriptRoot/tests/ProcessLifetimeProbe.java")
    Native 'compile-probe' "$JavaHome/bin/javac.exe" @('-J-Dfile.encoding=UTF-8', '-encoding', 'UTF-8', '-d', '../harness', $source) 15
    foreach ($case in @('root-exit', 'active-timeout')) {
        $directory = New-Item -ItemType Directory -Path "$RunDirectory/$case"
        $sleep = if ($case -eq 'root-exit') { '0' } else { '20000' }
        $timeout = if ($case -eq 'root-exit') { 3 } else { 1 }
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $failed = $false
        try { Native $case "$JavaHome/bin/java.exe" @('-Xmx32m', '-Dfile.encoding=UTF-8', '-cp', '../harness', 'qdcxx.oracle.ProcessLifetimeProbe', 'parent', "../$case", $sleep) $timeout } catch { $failed = $true }
        if ($failed -ne ($case -eq 'active-timeout')) { throw "Unexpected result: $case" }
        if ($case -eq 'active-timeout' -and (!$metadata.commands[-1].Contains('resourceStop') -or !$metadata.commands[-1].resourceStop)) { throw 'Expected a deadline resource stop, not another failure' }
        if ($timer.Elapsed.TotalSeconds -gt $timeout + 5) { throw "Unbounded completion: $case" }
        $pids = @('parent', 'child') | ForEach-Object { [int](Get-Content "$($directory.FullName)/$_.pid" -Raw) }
        if (@($pids | ForEach-Object { Get-Process -Id $_ -ErrorAction SilentlyContinue }).Count -ne 0) { throw "Owned Java process survived: $case" }
        $metadata.cases.Add([ordered]@{ case = $case; failedAsExpected = $failed; timeoutSeconds = $timeout; elapsedSeconds = $timer.Elapsed.TotalSeconds; processIds = $pids; remainingOwnedProcesses = 0 })
    }
    $metadata.state = 'VERIFIED'
} finally {
    $metadata | ConvertTo-Json -Depth 8 | Set-Content "$RunDirectory/metadata.json" -Encoding utf8NoBOM
}
'PASS: inherited-pipe root exit and active-root timeout are bounded; owned parent/child PIDs absent.'
