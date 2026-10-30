#requires -Version 7.4
<#
.SYNOPSIS
Builds an exact QD producer and captures or verifies small offline production tapes.
.DESCRIPTION
Windows working modes use a dedicated clean upstream clone, JDK 8 and Maven 3.6+. Builds qds-file and its reactor
dependencies, compiles RealTapeCorpus.java, then checks production replay and negative cases.
Does not connect to a feed. Capture never changes committed expectations. All logs and metadata
stay in a new repository .work directory. Upstream target directories and the selected Maven
repository are written; never share the clone with another build. No host settings are changed.
.PARAMETER Mode
Capture creates candidate artifacts. Verify also compares committed expectations. No default
working mode: no arguments, -Help and Get-Help show help without starting work.
.PARAMETER Producer
baseline (3.355) or current (separately pinned 3.356); required for working modes.
.PARAMETER UpstreamRoot
Dedicated clean clone of the official repository at the selected exact commit.
.PARAMETER JavaHome
JDK 8 directory, independent of the JDK 21+ used for formatting.
.PARAMETER MavenHome
Maven installation directory containing boot/plexus-classworlds*.jar and bin/m2.conf.
.PARAMETER Runner
Explicit qdcxx-run-utc executable built with tools/dev/Bootstrap.cmake. Owns child processes
before execution; logs are timestamped merged text, not raw separate Java byte streams.
.PARAMETER RunDirectory
New output directory under repository .work. Required; never reused or overwritten.
.PARAMETER MavenRepository
Explicit local Maven cache under repository .work. Required; may be reused sequentially.
.PARAMETER AllowDependencyDownloads
Allow Maven dependency downloads. Default is offline. Fixture generation uses no network.
.PARAMETER ProcessTimeoutSeconds
Java probe timeout, default 15 seconds, range 1..120. A stopped process is incomplete.
.PARAMETER BuildTimeoutSeconds
Maven build timeout, default 600 seconds, range 1..1800.
.PARAMETER HeapMiB
Java probe heap, default 128 MiB, range 32..512. Maven separately has a 512 MiB heap.
.PARAMETER Help
Print usage without resolving inputs, reading the lock or creating files.
.EXAMPLE
Get-Help ./tools/upstream/real-tape-corpus/Invoke-Corpus.ps1 -Full
.NOTES
Exit 0: successful help or all requested checks. Exit 1: failed input, build, timeout, resource
limit, replay or comparison. Per-file artifacts are limited to 65536 bytes. Each subprocess has
an 8 MiB limit per runner log stream. Forced runner termination closes its Windows ownership job.
Linux/macOS working execution is not qualified; help is portable. This is a finite synthetic corpus.
#>
[CmdletBinding()]
param(
    [ValidateSet('Capture', 'Verify')][string]$Mode,
    [ValidateSet('baseline', 'current')][string]$Producer,
    [string]$UpstreamRoot,
    [string]$JavaHome,
    [string]$MavenHome,
    [string]$Runner,
    [string]$RunDirectory,
    [string]$MavenRepository,
    [switch]$AllowDependencyDownloads,
    [ValidateRange(1, 120)][int]$ProcessTimeoutSeconds = 15,
    [ValidateRange(1, 1800)][int]$BuildTimeoutSeconds = 600,
    [ValidateRange(32, 512)][int]$HeapMiB = 128,
    [switch]$Help
)

$usage = @'
Invoke-Corpus.ps1: bounded offline production QD fixtures
  No arguments / -Help: help only, no input resolution or file access
  -Mode Capture|Verify -Producer baseline|current -UpstreamRoot PATH -JavaHome JDK8
    -MavenHome PATH -Runner QDCXX_RUN_UTC -MavenRepository PATH -RunDirectory NEW_PATH
    [-AllowDependencyDownloads] [-ProcessTimeoutSeconds 15] [-BuildTimeoutSeconds 600]
    [-HeapMiB 128]
Capture writes private candidates; Verify also checks exact committed artifact bytes.
Outputs: artifacts/, UTC runner logs, source/classpath/dependency hashes and metadata.json in .work.
Writes upstream target directories and the explicit .work Maven cache; use a dedicated clone.
Working modes are Windows-only; help is portable. The native runner owns Java before execution.
No feed connection; dependencies are offline unless explicitly enabled. Timeouts are incomplete.
Limits: three records/case, 65536 bytes/artifact, 8 MiB/raw log stream; full help: Get-Help -Full.
Exit 0: help or all requested checks; exit 1: failed input/build/replay/verification/resource stop.
'@
if ($Help -or $PSBoundParameters.Count -eq 0) {
    $usage
    return
}
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (!$IsWindows) { throw 'Working modes are qualified on Windows only; no work has started' }
foreach ($required in @('Mode', 'Producer', 'UpstreamRoot', 'JavaHome', 'MavenHome', 'Runner', 'RunDirectory', 'MavenRepository')) {
    if (!$PSBoundParameters.ContainsKey($required) -or !(Get-Variable $required -ValueOnly)) {
        throw "Missing -$required; use -Help"
    }
}
foreach ($key in @('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS')) {
    if ([Environment]::GetEnvironmentVariable($key)) { throw "Unset $key for isolated execution" }
}
$repo = (Resolve-Path "$PSScriptRoot/../../..").Path
$work = [IO.Path]::GetFullPath((Join-Path $repo '.work'))
function WorkPath([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    if (!$full.StartsWith($work + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Run directory and Maven cache must be below repository .work'
    }
    $ancestor = $full
    while ($ancestor) {
        if ((Test-Path -LiteralPath $ancestor) -and ((Get-Item -LiteralPath $ancestor).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Reparse points are not allowed for working paths'
        }
        $parent = Split-Path $ancestor -Parent
        if (!$parent -or $parent -eq $ancestor) { break }
        $ancestor = $parent
    }
    return $full
}
$RunDirectory = WorkPath $RunDirectory
$MavenRepository = WorkPath $MavenRepository
if (Test-Path -LiteralPath $RunDirectory) { throw 'RunDirectory must be new' }
$UpstreamRoot = (Resolve-Path -LiteralPath $UpstreamRoot).Path
foreach ($outputPath in @($RunDirectory, $MavenRepository)) {
    if ($outputPath -eq $UpstreamRoot -or $outputPath.StartsWith($UpstreamRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Run directory and Maven cache must not be inside the producer checkout'
    }
}
$JavaHome = (Resolve-Path -LiteralPath $JavaHome).Path
$MavenHome = (Resolve-Path -LiteralPath $MavenHome).Path
$Runner = (Resolve-Path -LiteralPath $Runner).Path
if (!(Test-Path -LiteralPath $Runner -PathType Leaf)) { throw 'Runner must be an explicit executable' }
$java = Join-Path $JavaHome 'bin/java.exe'
$javac = Join-Path $JavaHome 'bin/javac.exe'
if (!$IsWindows) { $java = Join-Path $JavaHome 'bin/java'; $javac = Join-Path $JavaHome 'bin/javac' }
foreach ($path in @($java, $javac, "$MavenHome/bin/m2.conf")) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing runtime component: $path" }
}
$boot = @(Get-ChildItem -LiteralPath "$MavenHome/boot" -Filter 'plexus-classworlds*.jar')
if ($boot.Count -ne 1) { throw 'Expected one Maven classworlds boot JAR' }
$lock = Get-Content "$PSScriptRoot/producer.lock.json" -Raw | ConvertFrom-Json
$pin = $lock.producers.$Producer
function GitValue([string[]]$Arguments) {
    $result = (& git -C $UpstreamRoot @Arguments 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw "Upstream git query failed: $result" }
    return $result.Trim()
}
function SourceTextHash([string]$Path) {
    $text = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($Path)).Replace("`r`n", "`n")
    $hash = [Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($text))
    return [Convert]::ToHexString($hash).ToLowerInvariant()
}
function AssertSource {
    if ((GitValue @('rev-parse', 'HEAD')) -cne $pin.commit) { throw 'Wrong producer commit' }
    if ((GitValue @('config', '--get', 'remote.origin.url')) -cne $lock.repository) { throw 'Wrong producer origin' }
    if (GitValue @('status', '--porcelain', '--untracked-files=all')) { throw 'Dirty producer checkout' }
    foreach ($entry in $pin.sourceHashes) {
        if ((SourceTextHash "$UpstreamRoot/$($entry.path)") -cne $entry.sha256Lf) {
            throw "Producer source hash mismatch: $($entry.path)"
        }
    }
}
AssertSource
New-Item -ItemType Directory -Path $RunDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $MavenRepository | Out-Null
New-Item -ItemType Directory -Path "$RunDirectory/raw", "$RunDirectory/harness", "$RunDirectory/build-work" | Out-Null
$metadata = [ordered]@{
    startedUtc = [DateTime]::UtcNow.ToString('o'); mode = $Mode; producer = $Producer
    producerVersion = $pin.version; producerCommit = $pin.commit; repository = $lock.repository
    sourceHashes = $pin.sourceHashes
    sourceByteHashes = @($pin.sourceHashes | ForEach-Object { [ordered]@{ path = $_.path; sha256 = (Get-FileHash "$UpstreamRoot/$($_.path)" -Algorithm SHA256).Hash.ToLowerInvariant() } })
    state = 'INCOMPLETE'; commands = [Collections.Generic.List[object]]::new()
    javaSourceSha256 = (Get-FileHash "$PSScriptRoot/RealTapeCorpus.java" -Algorithm SHA256).Hash.ToLowerInvariant()
    launcherSha256 = (Get-FileHash $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
    producerLockSha256 = (Get-FileHash "$PSScriptRoot/producer.lock.json" -Algorithm SHA256).Hash.ToLowerInvariant()
    javaSha256 = (Get-FileHash $java -Algorithm SHA256).Hash.ToLowerInvariant()
    javacSha256 = (Get-FileHash $javac -Algorithm SHA256).Hash.ToLowerInvariant()
    mavenConfigurationSha256 = (Get-FileHash "$MavenHome/bin/m2.conf" -Algorithm SHA256).Hash.ToLowerInvariant()
    runnerSha256 = (Get-FileHash $Runner -Algorithm SHA256).Hash.ToLowerInvariant()
    logFormat = 'UTC merged UTF-8 runner text; normalized merged payload; separate runner diagnostics; not raw Java streams'
    heapMiB = $HeapMiB; processTimeoutSeconds = $ProcessTimeoutSeconds
    buildTimeoutSeconds = $BuildTimeoutSeconds; dependencyDownloads = [bool]$AllowDependencyDownloads
    settings = $lock.settings; demo = 'NOT RUN: optional live acquisition is not implemented'
}
# Invoke-Corpus and Test-Lifetime call this function with explicit argv and expected exit.
# The native runner establishes process ownership before execution. Payload logs are decoded
# merged text; native stream identity and raw Java bytes are deliberately not claimed.
function Native([string]$Name, [string]$Exe, [string[]]$Arguments, [int]$Timeout, [int]$ExpectedExit = 0) {
    $command = [ordered]@{ name = $Name; executable = $Exe; arguments = $Arguments
        cwd = "$RunDirectory/build-work"; startedUtc = [DateTime]::UtcNow.ToString('o') }
    $metadata.commands.Add($command)
    $info = [Diagnostics.ProcessStartInfo]::new($Runner)
    $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true
    $info.WorkingDirectory = "$RunDirectory/build-work"
    $info.Environment['TZ'] = 'UTC'
    foreach ($arg in @('--log', "$RunDirectory/raw/$Name.utc.log", '--', $Exe) + $Arguments) { $info.ArgumentList.Add($arg) }
    $process = $null; $out = $null; $err = $null
    try {
        $out = [IO.File]::Create("$RunDirectory/raw/$Name.stdout")
        $err = [IO.File]::Create("$RunDirectory/raw/$Name.stderr")
        $process = [Diagnostics.Process]::Start($info)
        $command['processId'] = $process.Id
        $outCopy = $process.StandardOutput.BaseStream.CopyToAsync($out)
        $errCopy = $process.StandardError.BaseStream.CopyToAsync($err)
        $timer = [Diagnostics.Stopwatch]::StartNew()
        while (!($process.WaitForExit(0) -and $outCopy.IsCompleted -and $errCopy.IsCompleted)) {
            if ($timer.Elapsed.TotalSeconds -ge $Timeout -or $out.Length -gt 8MB -or $err.Length -gt 8MB) {
                $command['resourceStop'] = $true
                if (!$process.HasExited) { $process.Kill($true) }
                if (!$process.WaitForExit(5000)) { throw 'Runner termination was not confirmed' }
                break
            }
            Start-Sleep -Milliseconds 20
        }
        # Root exit does not disable the deadline. Never await a descendant-held pipe forever.
        if (!$outCopy.IsCompleted -or !$errCopy.IsCompleted) {
            $process.StandardOutput.BaseStream.Dispose(); $process.StandardError.BaseStream.Dispose()
            $command['resourceStop'] = $true
        }
        if (!$outCopy.IsCompleted -or !$errCopy.IsCompleted) { throw 'Runner stream drain did not complete within budget' }
        $null = $outCopy.GetAwaiter().GetResult(); $null = $errCopy.GetAwaiter().GetResult()
        $command['exitCode'] = $process.ExitCode
        $command['finishedUtc'] = [DateTime]::UtcNow.ToString('o')
        if ($out.Length -gt 8MB -or $err.Length -gt 8MB) { $command['resourceStop'] = $true }
        if ($command.Contains('resourceStop') -or $process.ExitCode -ne $ExpectedExit) {
            throw "Failed $Name; see raw logs (resource stop or unexpected exit)"
        }
        $out.Dispose(); $err.Dispose(); $out = $null; $err = $null
        $lines = @(Get-Content -LiteralPath "$RunDirectory/raw/$Name.stdout")
        if ($lines.Count -lt 2 -or $lines[0] -notmatch '^\S+ COMMAND \[' -or $lines[-1] -notmatch "^\S+ EXIT $ExpectedExit$") {
            throw 'Incomplete native runner log'
        }
        $payload = @($lines | Select-Object -Skip 1 | Select-Object -SkipLast 1 | ForEach-Object {
            if ($_ -notmatch '^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z (.*)$') { throw 'Malformed runner record' }
            $Matches[1]
        })
        [IO.File]::WriteAllText("$RunDirectory/raw/$Name.payload", (($payload -join "`n") + $(if ($payload.Count) { "`n" } else { '' })), [Text.UTF8Encoding]::new($false))
        if ($timer.Elapsed.TotalSeconds -ge $Timeout) {
            $command['resourceStop'] = $true
            throw 'Runner payload finalization exceeded the process budget'
        }
    } finally {
        if ($null -ne $process) {
            if (!$process.HasExited) { $process.Kill($true); $null = $process.WaitForExit(5000) }
            $process.Dispose()
        }
        if ($null -ne $out) { $out.Dispose() }
        if ($null -ne $err) { $err.Dispose() }
    }
}
function HashFiles([string]$Root) {
    @(Get-ChildItem -LiteralPath $Root -Recurse -File | ForEach-Object {
        [ordered]@{ path = [IO.Path]::GetRelativePath($Root, $_.FullName).Replace('\', '/')
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    } | Sort-Object { $_.path })
}
try {
    Native 'java-version' $java @('-version') $ProcessTimeoutSeconds
    $version = Get-Content "$RunDirectory/raw/java-version.payload" -Raw
    if ($version -notmatch 'version "1\.8\.') { throw 'Producer requires JDK 8' }
    $mavenPrefix = @('-Xmx512m', '-Dfile.encoding=UTF-8', '-Duser.timezone=UTC', "-Dmaven.home=$MavenHome",
        "-Dclassworlds.conf=$MavenHome/bin/m2.conf", "-Dmaven.multiModuleProjectDirectory=$UpstreamRoot",
        '-cp', $boot[0].FullName, 'org.codehaus.plexus.classworlds.launcher.Launcher')
    Native 'maven-version' $java ($mavenPrefix + @('-version')) $ProcessTimeoutSeconds
    $mavenVersion = Get-Content "$RunDirectory/raw/maven-version.payload" -Raw
    if ($mavenVersion -notmatch 'Apache Maven (?<version>\d+\.\d+\.\d+)') { throw 'Cannot determine Maven version' }
    if ([version]$Matches.version -lt [version]'3.6.0') { throw 'Maven 3.6+ required' }
    $mavenArgs = @('-B', '-f', "$UpstreamRoot/pom.xml", '-pl', 'qds-file', '-am',
        "-Dmaven.repo.local=$MavenRepository", '-Dmaven.build.cache.enabled=false',
        '-Dmaven.test.skip=true', 'clean', 'compile')
    if (!$AllowDependencyDownloads) { $mavenArgs = @('-o') + $mavenArgs }
    Native 'build' $java ($mavenPrefix + $mavenArgs) $BuildTimeoutSeconds
    AssertSource
    $entries = @($lock.classDirectories | ForEach-Object { (Resolve-Path "$UpstreamRoot/$_/target/classes").Path })
    $metadata['classpath'] = $entries
    $metadata['classpathFiles'] = @($entries | ForEach-Object { [ordered]@{ module = [IO.Path]::GetRelativePath($UpstreamRoot, $_).Replace('\', '/'); files = @(HashFiles $_) } })
    $metadata['mavenRuntimeFiles'] = @(HashFiles "$MavenHome/lib") + @(HashFiles "$MavenHome/boot")
    $metadata['dependencyJars'] = @(Get-ChildItem $MavenRepository -Recurse -Filter '*.jar' | Sort-Object FullName | ForEach-Object {
        [ordered]@{ path = [IO.Path]::GetRelativePath($MavenRepository, $_.FullName).Replace('\', '/'); sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
    # Relative working paths avoid the observed Windows JDK 8 absolute-Unicode-classpath limit.
    $relativeEntries = @($entries | ForEach-Object { [IO.Path]::GetRelativePath("$RunDirectory/build-work", $_) })
    $javaSource = [IO.Path]::GetRelativePath("$RunDirectory/build-work", "$PSScriptRoot/RealTapeCorpus.java")
    Native 'javac' $javac @('-J-Dfile.encoding=UTF-8', '-encoding', 'UTF-8', '-cp', ($relativeEntries -join [IO.Path]::PathSeparator), '-d', '../harness', $javaSource) $ProcessTimeoutSeconds
    $metadata['harnessFiles'] = @(HashFiles "$RunDirectory/harness")
    $cp = (@('../harness') + $relativeEntries) -join [IO.Path]::PathSeparator
    $prefix = @("-Xmx$($HeapMiB)m", '-Dfile.encoding=UTF-8', '-Duser.language=en', '-Duser.country=US', '-Duser.timezone=UTC', '-cp', $cp, 'qdcxx.oracle.RealTapeCorpus')
    Native 'help-tests' $java ($prefix + @('help-tests', '../help-tests')) $ProcessTimeoutSeconds
    Native 'generate' $java ($prefix + @('generate', '../artifacts')) $ProcessTimeoutSeconds
    Native 'repeat' $java ($prefix + @('generate', '../repeat')) $ProcessTimeoutSeconds
    foreach ($name in @('generate', 'repeat')) {
        foreach ($stream in @('payload', 'stderr')) {
            if ((Get-Content "$RunDirectory/raw/$name.$stream" -Raw) -match '(?m)^E\s|Failed to read file|Failed to write tape file|Failed to close |Failed to create directory or to move file|File was not completely parsed') {
                throw 'Production generation/replay logged an error'
            }
        }
    }
    Native 'repeat-verify' $java ($prefix + @('verify', '../repeat', '../artifacts')) $ProcessTimeoutSeconds
    $expected = if ($Mode -eq 'Verify') { [IO.Path]::GetRelativePath("$RunDirectory/build-work", "$repo/tests/fixtures/protocol/real-tape/producer/$Producer/expected") } else { '../artifacts' }
    Native 'verify' $java ($prefix + @('verify', '../artifacts', $expected)) $ProcessTimeoutSeconds
    Native 'negatives' $java ($prefix + @('negatives', '../artifacts', $expected, '../negative-work')) $ProcessTimeoutSeconds
    $metadata['artifactFiles'] = @(HashFiles "$RunDirectory/artifacts")
    $metadata['state'] = 'VERIFIED'
} catch {
    $metadata['failure'] = $_.Exception.Message
    throw
} finally {
    $metadata['finishedUtc'] = [DateTime]::UtcNow.ToString('o')
    $metadata | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath "$RunDirectory/metadata.json" -Encoding utf8NoBOM
}
"$Mode verified: $Producer ($($pin.commit)); private results: $RunDirectory"
