#requires -Version 7.4
<#
.SYNOPSIS
Runs finite read-only tape inventory and locked production FileReader checks.
.DESCRIPTION
Requires a clean prebuilt locked upstream checkout and JDK 8. Writes private logs and
hashed metadata only below repository .work; never builds upstream or changes input.
Use -Help without dependencies. Throws on setup/selftest/hash failure; replay failures
are retained as incomplete observations in metadata and logs, not whole-file success.
.PARAMETER ReplayDeliveryLimit
Maximum accepted Java callback deliveries, including fanout. Default 1000000.
.PARAMETER TimeoutSeconds
Wall-time bound for each owned child process. Default 30 seconds.
.PARAMETER HeapMiB
Java runtime heap limit. Default 128 MiB.
.PARAMETER PrivateFile
Optional read-only tape, at most 8 MiB. Exact paths and captures stay private.
.PARAMETER WorkDir
Private output directory below repository .work. Default .work/raw-inventory.
.PARAMETER UpstreamRoot
Clean checkout at upstream.lock, with prebuilt qds-file dependencies.
.PARAMETER JavaHome
Oracle JDK 8 home; independent of the Java formatter JDK.
.PARAMETER Help
Displays invocation and limits without resolving dependencies or reading tape input.
.PARAMETER Inspector
Optional current C++ tape inspector executable. Captures first result with the
explicit oracle-field-types-v1/default-quote-v1/default-profile-v1 local profiles
and raw-message-v1 tape mode.
#>
[CmdletBinding()]
param(
    [string]$UpstreamRoot,
    [string]$JavaHome,
    [string]$WorkDir = '.work/raw-inventory',
    [string]$PrivateFile,
    [string]$Inspector,
    [ValidateRange(1, 100000000)][int]$ReplayDeliveryLimit = 1000000,
    [ValidateRange(1, 300)][int]$TimeoutSeconds = 30,
    [ValidateRange(32, 1024)][int]$HeapMiB = 128,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Help) {
    @'
Check-Inventory.ps1 -UpstreamRoot PATH -JavaHome JDK8 [-WorkDir PATH]
                    [-PrivateFile PATH] [-ReplayDeliveryLimit N]
                    [-TimeoutSeconds N] [-HeapMiB N]
                    [-Inspector TAPE_INSPECTOR]
Read-only structural inventory and locked production FileReader checks.
Requires clean prebuilt locked upstream classes; does not build or modify upstream.
WorkDir: repository .work only, default .work/raw-inventory; private logs/metadata.
PrivateFile: optional, at most 8 MiB; input SHA-256 verified unchanged.
ReplayDeliveryLimit: 1..100000000 callback deliveries, default 1000000.
TimeoutSeconds: 1..300 per owned process, default 30; timed-out child is terminated.
HeapMiB: 32..1024 Java runtime heap MiB, default 128.
Inspector: optional C++ tape inspector; field-types/Quote/Profile, raw-message-v1.
Exit 0: required checks returned; replay may still be incomplete (inspect metadata).
Nonzero/exception: invalid setup, failed required check, or changed input.
-Help requires no dependencies and starts no inventory/replay work.
'@
    return
}
if (!$UpstreamRoot -or !$JavaHome) {
    throw 'UpstreamRoot and JavaHome are required. Use -Help for usage and limits.'
}
$repo = (Resolve-Path "$PSScriptRoot/../../..").Path
$upstream = (Resolve-Path -LiteralPath $UpstreamRoot).Path
$output = [IO.Path]::GetFullPath($WorkDir, $repo)
$ignoredRoot = [IO.Path]::GetFullPath((Join-Path $repo '.work')) + [IO.Path]::DirectorySeparatorChar
if (!$output.StartsWith($ignoredRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Inventory output must stay under the repository .work directory; private output must not be published.'
}
$lock = Get-Content -LiteralPath (Join-Path $repo 'upstream.lock') -Raw
if ($lock -notmatch '(?m)^commit = "([0-9a-f]{40})"') {
    throw 'Cannot read locked upstream commit.'
}
$expectedCommit = $Matches[1]
$actualCommit = (& git -C $upstream rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -cne $expectedCommit) {
    throw 'Upstream HEAD differs from upstream.lock.'
}
$dirty = @(& git -C $upstream status --porcelain --untracked-files=no)
if ($LASTEXITCODE -ne 0 -or $dirty.Count -ne 0) {
    throw 'Upstream tracked sources must be clean.'
}
$java = Join-Path $JavaHome 'bin/java.exe'
$javac = Join-Path $JavaHome 'bin/javac.exe'
$version = (& $java -version 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0 -or $version -notmatch 'version "1\.8\.') {
    throw 'Locked oracle requires JDK 8.'
}
$modules = @('dxlib', 'mars', 'dxfeed-promise', 'dxfeed-api', 'dxlib-qd-sandbox', 'auth', 'proto',
    'qd-core', 'proto-ssl', 'qd-rmi', 'dxfeed-impl', 'dxfeed-scheme', 'dxfeed-ipf-filter', 'qds-file')
$classPaths = @($modules | ForEach-Object {
    $classes = Join-Path $upstream "$_/target/classes"
    if (!(Test-Path -LiteralPath $classes -PathType Container)) {
        throw "Build locked upstream qds-file first: missing $classes"
    }
    $classes
})
New-Item -ItemType Directory -Force -Path $output, (Join-Path $output 'classes') | Out-Null
$classpath = $classPaths -join [IO.Path]::PathSeparator
$commands = [Collections.Generic.List[object]]::new()
function Invoke-Bounded([string]$Executable, [string[]]$Arguments, [string]$Name, [bool]$AllowFailure = $false) {
    $start = [DateTime]::UtcNow.ToString('o')
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $repo
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($argument in $Arguments) {
        $info.ArgumentList.Add($argument)
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    if (!$process.Start()) { throw "Cannot start $Name" }
    $stdout = [IO.File]::Create((Join-Path $output "$Name.stdout"))
    $stderr = [IO.File]::Create((Join-Path $output "$Name.stderr"))
    try {
        $outTask = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
        $errTask = $process.StandardError.BaseStream.CopyToAsync($stderr)
        $timedOut = !$process.WaitForExit($TimeoutSeconds * 1000)
        if ($timedOut) {
            $process.Kill($true)
            $process.WaitForExit()
        }
        $null = $outTask.GetAwaiter().GetResult()
        $null = $errTask.GetAwaiter().GetResult()
        $exitCode = $process.ExitCode
    } finally {
        $stdout.Dispose()
        $stderr.Dispose()
        $process.Dispose()
    }
    $commands.Add([ordered]@{ name = $Name; executable = $Executable; arguments = $Arguments;
        startedUtc = $start; finishedUtc = [DateTime]::UtcNow.ToString('o'); timeoutSeconds = $TimeoutSeconds;
        timedOut = $timedOut; exitCode = $exitCode })
    if (!$AllowFailure -and ($timedOut -or $exitCode -ne 0)) {
        throw "$Name failed; inspect ignored local logs."
    }
}

$metadata = [ordered]@{ startedUtc = [DateTime]::UtcNow.ToString('o'); upstreamCommit = $actualCommit;
    limits = [ordered]@{ replayDeliveryLimit = $ReplayDeliveryLimit; timeoutSeconds = $TimeoutSeconds; heapMiB = $HeapMiB };
    javaVersion = $version; sourceSha256 = (Get-FileHash "$PSScriptRoot/RawTapeInventory.java").Hash.ToLowerInvariant();
    launcherSha256 = (Get-FileHash $PSCommandPath).Hash.ToLowerInvariant(); commands = $commands }
try {
    Invoke-Bounded $javac @('-encoding', 'UTF-8', '-cp', $classpath, '-d', (Join-Path $output 'classes'),
        "$PSScriptRoot/RawTapeInventory.java") 'javac'
    $runtime = @("-Xmx${HeapMiB}m", '-Duser.timezone=UTC', '-Dfile.encoding=UTF-8', '-cp',
        ((Join-Path $output 'classes') + [IO.Path]::PathSeparator + $classpath), 'qdcxx.oracle.RawTapeInventory')
    Invoke-Bounded $java ($runtime + '--help') 'help'
    Invoke-Bounded $java ($runtime + 'selftest') 'selftest'
    # Equal processed/physical sizes do not identify compression: pad a small gzip
    # container to the exact 4096-byte decompressed marker sequence length.
    $markers = [byte[]]::new(4096)
    $plain = Join-Path $output 'generated-markers.qtp'
    [IO.File]::WriteAllBytes($plain, $markers)
    $compressed = [IO.MemoryStream]::new()
    try {
        $gzip = [IO.Compression.GZipStream]::new($compressed, [IO.Compression.CompressionLevel]::Optimal, $true)
        try { $gzip.Write($markers, 0, $markers.Length) } finally { $gzip.Dispose() }
        if ($compressed.Length -ge $markers.Length) { throw 'Generated gzip must be smaller than its marker input.' }
        $padded = [byte[]]::new($markers.Length)
        [Array]::Copy($compressed.ToArray(), $padded, $compressed.Length)
        $gzipPath = Join-Path $output 'generated-padded.qtp.gz'
        [IO.File]::WriteAllBytes($gzipPath, $padded)
    } finally { $compressed.Dispose() }
    foreach ($case in @(@{ name = 'padded-gzip'; path = $gzipPath }, @{ name = 'plain-markers'; path = $plain })) {
        Invoke-Bounded $java ($runtime + @('replay', $case.path)) "generated-$($case.name)"
        $result = @(Get-Content -LiteralPath (Join-Path $output "generated-$($case.name).stdout") |
            Where-Object { $_ -match '^replay-result\t' })
        if ($result.Count -ne 1 -or $result[0] -notmatch '^replay-result\treturned-byte-match\t' -or
            $result[0] -notmatch '\tprocessed-bytes\t4096\tfile-bytes\t4096$') {
            throw 'Generated replay must report a neutral byte match without inferring source compression.'
        }
    }
    if ($Inspector) {
        $Inspector = (Resolve-Path -LiteralPath $Inspector).Path
        $metadata.inspectorSha256 = (Get-FileHash -LiteralPath $Inspector -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $metadata.portableInputs = @()
    $corpus = Import-Csv -LiteralPath (Join-Path $repo 'tests/fixtures/protocol/real-tape/corpus.tsv') -Delimiter "`t"
    foreach ($entry in $corpus) {
        $inputPath = Join-Path $repo $entry.path
        $hash = (Get-FileHash -LiteralPath $inputPath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($hash -cne $entry.sha256) { throw 'Portable corpus input hash differs from manifest.' }
        $metadata.portableInputs += [ordered]@{ id = $entry.id; sha256 = $hash }
        Invoke-Bounded $java ($runtime + @('inventory', $inputPath)) "portable-$($entry.id)"
        if ($Inspector) {
            Invoke-Bounded $Inspector @('--input', $inputPath, '--profile', $entry.cpp_profile,
                '--tape-mode', 'raw-message-v1', '--output-format', 'tsv') "portable-cpp-$($entry.id)" $true
            $observation = $commands[$commands.Count - 1]
            if ($observation.timedOut -or $observation.exitCode -ne [int]$entry.cpp_exit) {
                throw 'Portable corpus C++ result differs from the declared expectation.'
            }
            $completion = @(Get-Content -LiteralPath (Join-Path $output "portable-cpp-$($entry.id).stdout") |
                Where-Object { $_ -match '^complete\t' })
            if ([int]$entry.cpp_exit -eq 0) {
                if ($completion.Count -ne 1) { throw 'Portable positive case requires one completion row.' }
                $cells = $completion[0].Split("`t")
                if ($cells.Count -ne 6 -or $cells[3] -cne $entry.cpp_emitted -or $cells[4] -cne $entry.cpp_processed) {
                    throw 'Portable positive case mapped/processed counts differ from the manifest.'
                }
            } else {
                if ($completion.Count -ne 0) { throw 'Portable negative case must not report completion.' }
                $diagnostics = @(Get-Content -LiteralPath (Join-Path $output "portable-cpp-$($entry.id).stderr") |
                    Where-Object { $_ -match '^incomplete(?:-v2)?\t' })
                if ($diagnostics.Count -ne 1) { throw 'Portable negative case requires one diagnostic row.' }
                $cells = $diagnostics[0].Split("`t")
                if ($cells.Count -lt 9 -or $cells[2] -cne $entry.cpp_stage -or
                    ($entry.cpp_reason -ne '-' -and ($cells.Count -ne 10 -or $cells[9] -cne $entry.cpp_reason))) {
                    throw 'Portable negative case stage/reason differs from the manifest.'
                }
            }
        }
    }
    if ($PrivateFile) {
        $sample = (Resolve-Path -LiteralPath $PrivateFile).Path
        if ((Get-Item -LiteralPath $sample).Length -gt 8MB) {
            throw 'Private input exceeds the finite 8 MiB inventory/replay file budget.'
        }
        $before = Get-FileHash -LiteralPath $sample -Algorithm SHA256
        $metadata.privateInput = [ordered]@{ path = $sample; bytes = (Get-Item -LiteralPath $sample).Length;
            sha256Before = $before.Hash.ToLowerInvariant() }
        try {
            Invoke-Bounded $java ($runtime + @('inventory', $sample)) 'private-inventory'
            Invoke-Bounded $java ($runtime + @('replay', $sample, "$ReplayDeliveryLimit")) 'private-replay' $true
            if ($Inspector) {
                foreach ($profile in @('oracle-field-types-v1', 'default-quote-v1', 'default-profile-v1')) {
                    Invoke-Bounded $Inspector @('--input', $sample, '--profile', $profile,
                        '--tape-mode', 'raw-message-v1', '--output-format', 'tsv') "private-cpp-$profile" $true
                }
            }
        } finally {
            $after = (Get-FileHash -LiteralPath $sample -Algorithm SHA256).Hash.ToLowerInvariant()
            $metadata.privateInput.sha256After = $after
            if ($after -cne $metadata.privateInput.sha256Before) { throw 'Private input changed during inspection.' }
        }
    }
} finally {
    $metadata.finishedUtc = [DateTime]::UtcNow.ToString('o')
    $metadata | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'metadata.json') -Encoding utf8
}
'Inventory checks complete; results and optional private replay diagnostics stay in ignored local output.'
