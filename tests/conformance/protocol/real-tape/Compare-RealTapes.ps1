#requires -Version 7.4
<#
.SYNOPSIS
Compares one complete ordinary binary tape with the locked production Java parser.
.DESCRIPTION
Reads a fixed input without changing it, runs an explicit version-7 or version-8 C++ projection, and
compares all schema snapshots, source bindings, record order and raw values. Java discovery
uses genuine unknown-record skips; acceptance requires zero skips in the mapped pass.
One instrumented fixed-source stream proves EOF and read/close status. A literal-file
production FileReader pass must produce the same sequence. Full logs remain below .work.
Does not build or modify upstream, install dependencies, or connect to a network endpoint.
.PARAMETER InputFile
Read-only binary tape. Default maximum is 8 MiB.
.PARAMETER Inspector
Built qdcxx-tape-inspect supporting --wire-schema ordinary-v1 and output version 7.
.PARAMETER UpstreamRoot
Clean prebuilt Java QD checkout at upstream.lock, including qds-file dependencies.
.PARAMETER JavaHome
JDK 8 oracle home; the formatter uses a separate JDK 21 or newer.
.PARAMETER Runner
Explicit qdcxx-run-utc executable; owns the Windows process job before child execution.
.PARAMETER RunDirectory
New private result directory below repository .work. Required; never reused.
.PARAMETER TapeMode
Explicit Java source-time policy: legacy means NONE, raw-message-v1 means MESSAGE.
Existing ordinary profiles retain their family restrictions; CppMode selects C++ independently.
.PARAMETER CppMode
Optional independent C++ mode: legacy, raw-message-v1 or binary-catalog-v1. Defaults to
TapeMode. Binary catalog selects output8; Java source NONE/MESSAGE still follows TapeMode.
.PARAMETER MaxInputBytes
Source byte cap; default 8388608, range 1..67108864.
.PARAMETER MaxRecords
Wire record cap; default 1000000, range 1..10000000.
.PARAMETER MaxOutputBytes
C++ output and each child stream cap; default 268435456, range 1..1073741824.
.PARAMETER TimeoutSeconds
Wall time cap per owned child process; default 60, range 1..300.
.PARAMETER HeapMiB
Java runtime heap MiB; default 128, range 32..1024.
.PARAMETER Help
Shows help without any working inputs, dependency lookup or file work.
.NOTES
Exit 0 means full equality and observed EOF; exceptions/nonzero indicate rejection or
incomplete work. Interrupted runs retain status INCOMPLETE. Input paths, names and values
may appear in private subprocess logs and command metadata; do not publish the run directory.
#>
[CmdletBinding()]
param(
    [string]$InputFile, [string]$Inspector, [string]$UpstreamRoot, [string]$JavaHome, [string]$Runner,
    [string]$RunDirectory,
    [ValidateSet('legacy', 'raw-message-v1')][string]$TapeMode,
    [ValidateSet('legacy', 'raw-message-v1', 'binary-catalog-v1')][string]$CppMode,
    [ValidateRange(1, 67108864)][long]$MaxInputBytes = 8388608,
    [ValidateRange(1, 10000000)][long]$MaxRecords = 1000000,
    [ValidateRange(1, 1073741824)][long]$MaxOutputBytes = 268435456,
    [ValidateRange(1, 300)][int]$TimeoutSeconds = 60,
    [ValidateRange(32, 1024)][int]$HeapMiB = 128,
    [switch]$Help
)
if ($Help -or $PSBoundParameters.Count -eq 0) {
    @'
Compare-RealTapes.ps1 -InputFile PATH -Inspector EXE -UpstreamRoot PATH -JavaHome JDK8 -Runner EXE
                     -RunDirectory NEW_REPOSITORY_WORK_PATH -TapeMode legacy|raw-message-v1
                     [-CppMode legacy|raw-message-v1|binary-catalog-v1]
                     [-MaxInputBytes 8388608] [-MaxRecords 1000000]
                     [-MaxOutputBytes 268435456] [-TimeoutSeconds 60] [-HeapMiB 128]
Read-only full schema/source-ID/order/raw-value/symbol/zero-flags/time comparison.
CppMode defaults to TapeMode; binary-catalog-v1 selects strict output8 ordinary subset.
TapeMode always selects Java NONE/MESSAGE timing independently; never inferred from CppMode.
Requires clean prebuilt locked Java QD classes; never builds or changes upstream/input.
RunDirectory must be new, below repository .work; all raw logs/paths/values stay private.
Hard maxima: input 64 MiB, records 10000000, output 1 GiB, row 4 MiB.
Each owned child has a 1..300-second bound (default 60), Java heap 32..1024 MiB (128).
Windows-qualified native runner owns descendants before execution; target streams stay raw.
Bootstrap compiler diagnostics are UTC text. Root deadline plus at most 1 s cleanup.
EOF is measured on an instrumented fixed file stream, cross-checked against literal-file
production FileReader sequence. Ordinary types 2/8/10/40/56/72/88/120, zero flags only.
Exit 0: whole-file equality and clean EOF. Exception/nonzero: failure or incomplete result.
No arguments / -Help: identical help, empty stderr, no dependencies or file work.
'@
    return
}
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (!$InputFile -or !$Inspector -or !$UpstreamRoot -or !$JavaHome -or !$Runner -or !$RunDirectory -or !$TapeMode) {
    throw 'InputFile, Inspector, UpstreamRoot, JavaHome, Runner, RunDirectory and TapeMode are required. Use -Help.'
}
if (!$PSBoundParameters.ContainsKey('CppMode')) { $CppMode = $TapeMode }
$cppProjection = if ($CppMode -eq 'binary-catalog-v1') { 'binary-catalog-v1' } else { 'ordinary-v1' }
$repo = (Resolve-Path "$PSScriptRoot/../../../..").Path
$tool = Join-Path $repo 'tools/upstream/real-tape-acceptance'
. (Join-Path $tool 'Acceptance.Common.ps1')
$run = Get-AcceptanceWorkPath $repo $RunDirectory -RequireNew
$inputPath = (Resolve-Path -LiteralPath $InputFile).Path
$inspectorPath = (Resolve-Path -LiteralPath $Inspector).Path
$runnerPath = (Resolve-Path -LiteralPath $Runner).Path
$upstream = (Resolve-Path -LiteralPath $UpstreamRoot).Path
if ((Get-Item -LiteralPath $inputPath).Length -gt $MaxInputBytes) { throw 'Input exceeds explicit byte budget.' }
$lock = Get-Content -LiteralPath (Join-Path $repo 'upstream.lock') -Raw
if ($lock -notmatch '(?m)^commit = "([0-9a-f]{40})"') { throw 'Cannot read upstream lock.' }
$expectedCommit = $Matches[1]
$commit = (& git -C $upstream rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $commit -cne $expectedCommit) { throw 'Upstream commit mismatch.' }
$dirty = @(& git -C $upstream status --porcelain --untracked-files=no)
if ($LASTEXITCODE -ne 0 -or $dirty.Count -ne 0) { throw 'Upstream tracked source must remain clean.' }
$java = Get-AcceptanceJava $JavaHome 'java'
$javac = Get-AcceptanceJava $JavaHome 'javac'
$modules = @('dxlib', 'mars', 'dxfeed-promise', 'dxfeed-api', 'dxlib-qd-sandbox', 'auth', 'proto',
    'qd-core', 'proto-ssl', 'qd-rmi', 'dxfeed-impl', 'dxfeed-scheme', 'dxfeed-ipf-filter', 'qds-file')
$classPaths = @($modules | ForEach-Object {
    $path = Join-Path $upstream "$_/target/classes"
    if (!(Test-Path -LiteralPath $path -PathType Container)) { throw "Prebuilt locked module missing: $_" }
    $path
})
New-Item -ItemType Directory -Path $run, (Join-Path $run 'classes') | Out-Null
$commands = [Collections.Generic.List[object]]::new()
$source = Join-Path $tool 'RealTapeAcceptance.java'
$workerSource = Join-Path $tool 'RawChild.java'
$beforeHash = (Get-FileHash -LiteralPath $inputPath).Hash.ToLowerInvariant()
$classpath = $classPaths -join [IO.Path]::PathSeparator
$metadata = [ordered]@{
    status = 'INCOMPLETE'; startedUtc = [DateTime]::UtcNow.ToString('o'); upstreamCommit = $commit
    sourceSha256 = (Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant()
    workerSourceSha256 = (Get-FileHash -LiteralPath $workerSource).Hash.ToLowerInvariant()
    runnerSha256 = (Get-FileHash -LiteralPath $runnerPath).Hash.ToLowerInvariant()
    runnerSourcesBefore = @(@('tools/dev/run_utc.cpp', 'tools/dev/process_windows.cpp', 'tools/dev/process.hpp') | ForEach-Object {
        [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $repo $_)).Hash.ToLowerInvariant() }
    })
    processQualification = 'Windows native runner job; POSIX launcher not qualified'
    launcherSha256 = (Get-FileHash -LiteralPath $PSCommandPath).Hash.ToLowerInvariant()
    helperSha256 = (Get-FileHash -LiteralPath (Join-Path $tool 'Acceptance.Common.ps1')).Hash.ToLowerInvariant()
    inspectorSha256 = (Get-FileHash -LiteralPath $inspectorPath).Hash.ToLowerInvariant()
    input = [ordered]@{ path = $inputPath; bytes = (Get-Item -LiteralPath $inputPath).Length; sha256Before = $beforeHash }
    limits = [ordered]@{ inputBytes = $MaxInputBytes; records = $MaxRecords; outputBytes = $MaxOutputBytes
        timeoutSeconds = $TimeoutSeconds; heapMiB = $HeapMiB; workerHeapMiB = 32; bootstrapHeapMiB = 128
        maximumNativeCleanupSeconds = 1 }
    tapeMode = $TapeMode; cppMode = $CppMode; cppProjection = $cppProjection; commands = $commands
}
try {
    $bootstrap = Initialize-AcceptanceWorker $runnerPath $java $javac $workerSource $run $TimeoutSeconds $repo
    $commands.Add($bootstrap)
    $metadata.workerClassSha256Before = (Get-FileHash -LiteralPath (Join-Path $run 'worker/qdcxx/oracle/RawChild.class')).Hash.ToLowerInvariant()
    $version = Invoke-AcceptanceProcess $java @('-version') $run 'java-version' $TimeoutSeconds $MaxOutputBytes $repo
    $commands.Add($version); Assert-AcceptanceProcess $version
    $versionText = Get-Content -LiteralPath $version.stderr -Raw
    if ($versionText -notmatch 'version "1\.8\.') { throw 'Oracle requires JDK 8.' }
    $metadata.javaVersion = $versionText
    $metadata.javaSha256 = (Get-FileHash -LiteralPath $java).Hash.ToLowerInvariant()
    $metadata.javacSha256 = (Get-FileHash -LiteralPath $javac).Hash.ToLowerInvariant()
    $inventory = [Collections.Generic.List[string]]::new()
    foreach ($classes in $classPaths) {
        foreach ($file in (Get-ChildItem -LiteralPath $classes -File -Recurse | Sort-Object FullName)) {
            $relative = [IO.Path]::GetRelativePath($upstream, $file.FullName).Replace('\', '/')
            $inventory.Add($relative + "`t" + (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant())
        }
    }
    $inventoryPath = Join-Path $run 'classpath.tsv'
    [IO.File]::WriteAllText($inventoryPath, ($inventory -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
    $metadata.classpathInventorySha256 = (Get-FileHash -LiteralPath $inventoryPath).Hash.ToLowerInvariant()
    $metadata.classpathFiles = $inventory.Count
    $compile = Invoke-AcceptanceProcess $javac @('-encoding', 'UTF-8', '-cp', $classpath, '-d',
        (Join-Path $run 'classes'), $source) $run 'javac' $TimeoutSeconds $MaxOutputBytes $repo
    $commands.Add($compile); Assert-AcceptanceProcess $compile
    $metadata.oracleClassesBefore = @(Get-ChildItem -LiteralPath (Join-Path $run 'classes') -File -Recurse | Sort-Object FullName | ForEach-Object {
        [ordered]@{ path = [IO.Path]::GetRelativePath((Join-Path $run 'classes'), $_.FullName)
            sha256 = (Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant() }
    })
    $runtime = @("-Xmx${HeapMiB}m", '-Duser.timezone=UTC', '-Dfile.encoding=UTF-8', '-cp',
        ((Join-Path $run 'classes') + [IO.Path]::PathSeparator + $classpath), 'qdcxx.oracle.RealTapeAcceptance')
    $metadata.runtime = $runtime
    $inspectArgs = @('--input', $inputPath, '--wire-schema', $cppProjection, '--output-format', 'tsv',
        '--tape-mode', $CppMode, '--max-input', "$MaxInputBytes", '--max-output', "$MaxOutputBytes",
        '--max-records', "$MaxRecords", '--max-fields', '67108864', '--max-values', '67108864',
        '--max-frames', '1000000', '--max-timing-events', '1000000')
    $inspect = Invoke-AcceptanceProcess $inspectorPath $inspectArgs $run 'cpp' $TimeoutSeconds $MaxOutputBytes $repo
    $commands.Add($inspect); Assert-AcceptanceProcess $inspect
    $compareArgs = @('--input', $inputPath, '--cpp-output', $inspect.stdout, '--tape-mode', $TapeMode,
        '--cpp-projection', $cppProjection,
        '--max-input', "$MaxInputBytes", '--max-records', "$MaxRecords", '--max-output', "$MaxOutputBytes")
    $compare = Invoke-AcceptanceProcess $java ($runtime + $compareArgs) $run 'java-compare' $TimeoutSeconds $MaxOutputBytes $repo
    $commands.Add($compare); Assert-AcceptanceProcess $compare
    $metadata.cppProjectionSha256 = (Get-FileHash -LiteralPath $inspect.stdout).Hash.ToLowerInvariant()
    $metadata.result = Get-Content -LiteralPath $compare.stdout
    $metadata.status = 'PASS'
} finally {
    $metadata.input.sha256After = (Get-FileHash -LiteralPath $inputPath).Hash.ToLowerInvariant()
    $metadata.input.unchanged = $metadata.input.sha256After -ceq $beforeHash
    if (!$metadata.input.unchanged) { $metadata.status = 'FAIL-INPUT-CHANGED' }
    if ((Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant() -cne $metadata.sourceSha256) {
        $metadata.status = 'FAIL-SOURCE-CHANGED'
    }
    if ((Get-FileHash -LiteralPath $workerSource).Hash.ToLowerInvariant() -cne $metadata.workerSourceSha256 -or
        (Get-FileHash -LiteralPath $runnerPath).Hash.ToLowerInvariant() -cne $metadata.runnerSha256) { $metadata.status = 'FAIL-WORKER-RUNNER-CHANGED' }
    $metadata.runnerSourcesAfter = @(@('tools/dev/run_utc.cpp', 'tools/dev/process_windows.cpp', 'tools/dev/process.hpp') | ForEach-Object {
        [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $repo $_)).Hash.ToLowerInvariant() }
    })
    if (($metadata.runnerSourcesBefore | ConvertTo-Json -Depth 3 -Compress) -cne
        ($metadata.runnerSourcesAfter | ConvertTo-Json -Depth 3 -Compress)) { $metadata.status = 'FAIL-RUNNER-SOURCES-CHANGED' }
    if ($metadata.Contains('workerClassSha256Before')) {
        $metadata.workerClassSha256After = (Get-FileHash -LiteralPath (Join-Path $run 'worker/qdcxx/oracle/RawChild.class')).Hash.ToLowerInvariant()
        if ($metadata.workerClassSha256After -cne $metadata.workerClassSha256Before) { $metadata.status = 'FAIL-WORKER-CLASS-CHANGED' }
    }
    if ($metadata.Contains('oracleClassesBefore')) {
        $metadata.oracleClassesAfter = @(Get-ChildItem -LiteralPath (Join-Path $run 'classes') -File -Recurse | Sort-Object FullName | ForEach-Object {
            [ordered]@{ path = [IO.Path]::GetRelativePath((Join-Path $run 'classes'), $_.FullName)
                sha256 = (Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant() }
        })
        if (($metadata.oracleClassesBefore | ConvertTo-Json -Depth 3 -Compress) -cne
            ($metadata.oracleClassesAfter | ConvertTo-Json -Depth 3 -Compress)) { $metadata.status = 'FAIL-ORACLE-CLASSES-CHANGED' }
    }
    $metadata.inspectorSha256After = (Get-FileHash -LiteralPath $inspectorPath).Hash.ToLowerInvariant()
    if ($metadata.inspectorSha256After -cne $metadata.inspectorSha256) {
        $metadata.status = 'FAIL-INSPECTOR-CHANGED'
    }
    if ((Get-FileHash -LiteralPath $PSCommandPath).Hash.ToLowerInvariant() -cne $metadata.launcherSha256 -or
        (Get-FileHash -LiteralPath (Join-Path $tool 'Acceptance.Common.ps1')).Hash.ToLowerInvariant() -cne $metadata.helperSha256) {
        $metadata.status = 'FAIL-HARNESS-CHANGED'
    }
    $metadata.finishedUtc = [DateTime]::UtcNow.ToString('o')
    $metadata | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $run 'metadata.json') -Encoding utf8NoBOM
}
if ($metadata.status -cne 'PASS') { throw 'Acceptance did not complete; inspect private metadata.' }
"PASS: full ordinary tape projection; private evidence in $run"
