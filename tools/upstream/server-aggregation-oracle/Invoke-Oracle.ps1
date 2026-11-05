# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

[CmdletBinding()]
param(
    [ValidateSet('Capture','Verify')][string]$Mode = 'Verify',
    [string]$Upstream,
    [string]$JavaHome,
    [string]$Logger,
    [string]$RunDirectory,
    [switch]$Help,
    [switch]$HelpRu
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($HelpRu) {
 Write-Output 'Захват/проверка actual production Java8 server aggregation request/info/wire semantics. -Mode Verify (default) или Capture; обязательны -Upstream <pinned QD>, -JavaHome <JDK8>, -Logger <UTC logger>, -RunDirectory <новый каталог внутри .work>. Capture заменяет только tests/fixtures/server-aggregation/oracle.tsv и provenance.json; Verify сравнивает. Создаются private UTC/raw/class inventories/receipt. Один процесс, heap128MiB, timeout60s; только собственный loopback для autoconnect; без внешней сети/SDK rebuild. Exit0 успех/справка, Exit1 invalid input/reference/timeout/process/mismatch. -Help/-HelpRu и no args не запускают работу; Get-Help содержит EN параметры.'
 exit 0
}
if ($Help -or $PSBoundParameters.Count -eq 0) {Get-Help -Full $PSCommandPath; exit 0}
try {
    if (!$Upstream -or !$JavaHome -or !$Logger -or !$RunDirectory) { throw 'Required inputs missing; use -Help' }
    $repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
    $upstreamPath = (Resolve-Path -LiteralPath $Upstream).Path
    $expectedRevision = '0ddc0a09779fb61d74cc0b37c6449ff47dcb715f'
    $java = (Resolve-Path -LiteralPath (Join-Path $JavaHome 'bin/java.exe')).Path
    $javac = (Resolve-Path -LiteralPath (Join-Path $JavaHome 'bin/javac.exe')).Path
    $loggerPath = (Resolve-Path -LiteralPath $Logger).Path
    $run = [IO.Path]::GetFullPath($RunDirectory,$repo)
    $allowed = Join-Path $repo '.work'
    if (!$run.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'RunDirectory must be inside this checkout .work' }
    $ancestor = [IO.DirectoryInfo]::new($run).Parent
    while ($ancestor -and $ancestor.FullName.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) {
        if ($ancestor.Exists -and ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'RunDirectory ancestors must not redirect outside .work' }
        $ancestor = $ancestor.Parent
    }
    if (Test-Path -LiteralPath $run) { throw 'RunDirectory must be a new directory; prior evidence is never overwritten' }
    New-Item -ItemType Directory -Force -Path $run,(Join-Path $run 'classes') | Out-Null
    $classpath = @('dxlib','mars','dxfeed-promise','dxfeed-api','dxlib-qd-sandbox','auth','proto','qd-core','proto-ssl','qd-rmi','dxfeed-impl','dxfeed-scheme','dxfeed-ipf-filter','qds-file') | ForEach-Object { (Resolve-Path -LiteralPath (Join-Path $upstreamPath "$_/target/classes")).Path }
    $sourceRelative = @('dxfeed-api/src/main/java/com/dxfeed/api/DXEndpoint.java','dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXEndpointImpl.java','qd-core/src/main/java/com/devexperts/qd/qtp/QDEndpoint.java','qd-core/src/main/java/com/devexperts/qd/qtp/MessageAdapter.java','qd-core/src/main/java/com/devexperts/qd/qtp/DistributorAdapter.java','qd-core/src/main/java/com/devexperts/qd/qtp/AgentAdapter.java','qd-core/src/main/java/com/devexperts/qd/qtp/ChannelShaper.java','qd-core/src/main/java/com/devexperts/qd/qtp/ProtocolDescriptor.java','qd-core/src/main/java/com/devexperts/qd/qtp/MessageDescriptor.java','qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java','qd-core/src/main/java/com/devexperts/qd/qtp/AbstractQTPComposer.java','dxlib/src/main/java/com/devexperts/util/TimePeriod.java','dxlib/src/main/java/com/devexperts/util/TimePeriodInfo.java','qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java','qd-core/src/main/java/com/devexperts/qd/qtp/MessageConnectors.java','qd-core/src/main/java/com/devexperts/qd/qtp/socket/ClientSocketConnector.java','dxlib/src/main/java/com/devexperts/io/StreamInput.java','dxlib/src/main/java/com/devexperts/util/FastDoubleUtil.java','dxlib/src/main/java/com/devexperts/io/BufferedInput.java')
    $sources = @($sourceRelative | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $upstreamPath $_)).Hash.ToLowerInvariant()} })
    function ClassInventory {
        for ($index = 0; $index -lt $classpath.Count; ++$index) {
            Get-ChildItem -LiteralPath $classpath[$index] -Recurse -File | Sort-Object FullName | ForEach-Object { [ordered]@{root=$index;path=[IO.Path]::GetRelativePath($classpath[$index],$_.FullName).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()} }
        }
    }
    $before = @(ClassInventory)
    $before | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'classes-before.json')
    $commands = [Collections.Generic.List[object]]::new()
    function Invoke-Logged([string]$name,[string]$executable,[string[]]$arguments) {
        $info = [Diagnostics.ProcessStartInfo]::new()
        $info.FileName=$loggerPath; $info.WorkingDirectory=$repo; $info.UseShellExecute=$false; $info.CreateNoWindow=$true
        $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
        foreach($argument in @('--log',(Join-Path $run "$name.utc.log"),'--',$executable)+$arguments) { $info.ArgumentList.Add($argument) }
        $started = [DateTime]::UtcNow.ToString('o')
        $process = [Diagnostics.Process]::Start($info)
        $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        $timedOut = !$process.WaitForExit(60000)
        if($timedOut) { $process.Kill($true); $process.WaitForExit() }
        [IO.File]::WriteAllText((Join-Path $run "$name.stdout"),$stdout.GetAwaiter().GetResult())
        [IO.File]::WriteAllText((Join-Path $run "$name.stderr"),$stderr.GetAwaiter().GetResult())
        $exitCode=$process.ExitCode; $process.Dispose()
        $commands.Add([ordered]@{name=$name;startUtc=$started;finishUtc=[DateTime]::UtcNow.ToString('o');exitCode=$exitCode;timedOut=$timedOut})
        if($timedOut -or $exitCode -ne 0) { throw "$name failed; full logs retained" }
    }
    $gitPath=(Get-Command git -CommandType Application | Select-Object -First 1).Source
    Invoke-Logged 'git-head' $gitPath @('-C',$upstreamPath,'rev-parse','HEAD')
    $revisionLines=[regex]::Matches([IO.File]::ReadAllText((Join-Path $run 'git-head.stdout')),'(?m)(?:^|\s)([a-f0-9]{40})\r?$')
    if($revisionLines.Count -ne 1 -or $revisionLines[0].Groups[1].Value -cne $expectedRevision) {throw 'Reference revision differs from pinned QD3.355'}
    Invoke-Logged 'git-diff' $gitPath (@('-C',$upstreamPath,'diff','--quiet','HEAD','--')+$sourceRelative)
    Invoke-Logged 'java-version' $java @('-Xmx128m','-version')
    $version = ([IO.File]::ReadAllText((Join-Path $run 'java-version.stderr')) + [IO.File]::ReadAllText((Join-Path $run 'java-version.stdout'))).Trim()
    $versionLines = [regex]::Matches($version, '(?m)^(?:\d{4}-\d{2}-\d{2}T[0-9:.]+Z )?((?:openjdk|java) version "1\.8\.[^\r\n]+)\r?$')
    if ($versionLines.Count -ne 1) { throw 'Oracle requires exactly one real JDK8 version line in bounded output' }
    $javaVersion = $versionLines[0].Groups[1].Value.TrimEnd([char]13)
    $harness = Join-Path $PSScriptRoot 'ServerAggregationOracle.java'
    $classes = Join-Path $run 'classes'
    Invoke-Logged 'javac' $javac @('-J-Xmx128m','-encoding','UTF-8','-cp',($classpath -join ';'),'-d',$classes,$harness)
    foreach ($zone in @('UTC','Pacific/Honolulu')) {
        $zoneName=$zone.Replace('/','-')
        Invoke-Logged "java-$zoneName" $java @('-Xmx128m',"-Duser.timezone=$zone",'-Duser.language=en','-Duser.country=US','-cp',((@($classes)+$classpath)-join ';'),'ServerAggregationOracle',(Join-Path $run "$zoneName.tsv"))
    }
    $observed=Join-Path $run 'UTC.tsv'
    if((Get-FileHash $observed).Hash -cne (Get-FileHash (Join-Path $run 'Pacific-Honolulu.tsv')).Hash) {throw 'Timezone changed server aggregation observations'}
    $observedHash=(Get-FileHash -LiteralPath $observed).Hash.ToLowerInvariant()
    $after = @(ClassInventory)
    $after | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'classes-after.json')
    if(($before|ConvertTo-Json -Depth 5 -Compress) -cne ($after|ConvertTo-Json -Depth 5 -Compress)) { throw 'Production classes changed during capture' }
    $publicProvenance=[ordered]@{revision=$expectedRevision;javaVersion=$javaVersion;javaSha256=(Get-FileHash $java).Hash.ToLowerInvariant();javacSha256=(Get-FileHash $javac).Hash.ToLowerInvariant();harnessSha256=(Get-FileHash $harness).Hash.ToLowerInvariant();launcherSha256=(Get-FileHash $PSCommandPath).Hash.ToLowerInvariant();upstreamSources=$sources;productionClassFiles=@($before | Where-Object { $_.path.EndsWith('.class') }).Count;productionResourceFiles=@($before | Where-Object { !$_.path.EndsWith('.class') }).Count;productionClassInventorySha256=(Get-FileHash (Join-Path $run 'classes-before.json')).Hash.ToLowerInvariant();fixtureSha256=$observedHash;rows=@(Get-Content -LiteralPath $observed).Count;timezoneInvariant=$true}
    $fixture=Join-Path $repo 'tests/fixtures/server-aggregation/oracle.tsv'
    $provenance=Join-Path $repo 'tests/fixtures/server-aggregation/provenance.json'
    if($Mode -eq 'Capture') {
        Copy-Item -LiteralPath $observed -Destination $fixture
        [IO.File]::WriteAllText($provenance,(($publicProvenance|ConvertTo-Json -Depth 7)+[char]10).Replace([string][char]13,''),[Text.UTF8Encoding]::new($false))
    }
    else {
        if((Get-FileHash -LiteralPath $fixture).Hash.ToLowerInvariant() -cne $observedHash) { throw 'Configuration fixture differs' }
        $recorded=Get-Content -LiteralPath $provenance -Raw|ConvertFrom-Json
        foreach($property in @('revision','harnessSha256','launcherSha256','fixtureSha256','productionClassInventorySha256')) { if($recorded.$property -cne $publicProvenance[$property]) { throw "Provenance differs: $property" } }
        if(($recorded.upstreamSources|ConvertTo-Json -Depth 5 -Compress) -cne ($sources|ConvertTo-Json -Depth 5 -Compress)) { throw 'Upstream source hashes differ' }
    }
    [ordered]@{status='PASS';mode=$Mode;commands=$commands;provenance=$publicProvenance;loggerSha256=(Get-FileHash $loggerPath).Hash.ToLowerInvariant()}|ConvertTo-Json -Depth 9|Set-Content -LiteralPath (Join-Path $run 'receipt.json')
    Write-Output "$Mode PASS: $($publicProvenance.rows) server aggregation observations; both timezones match"
} catch { [Console]::Error.WriteLine($_.Exception.Message); [Console]::Error.WriteLine('Use -Help or -HelpRu for parameters, limits and side effects.'); exit 1 }

<#
.SYNOPSIS
Capture or verify actual pinned Java server aggregation request/info/wire observations.
.DESCRIPTION
Uses existing pinned production classes and JDK8, one process at a time, heap128MiB, timeout60s.
Requires all four operational paths. RunDirectory must be a new contained nonredirected .work directory.
Capture replaces only tests/fixtures/server-aggregation/oracle.tsv and provenance.json. Verify compares them.
Writes full UTC commands/exits, raw streams, complete class inventories and receipt privately. No SDK build or external network; one owned loopback listener proves actual physical reconnect.
Exit0 success/help; exit1 invalid inputs, reference mismatch, timeout, process or observation failure.
No arguments, -Help and -HelpRu require no inputs and start no workload. Only own timed-out children are stopped.
.PARAMETER Mode
Verify (default) compares; Capture writes reviewed fixture/provenance candidates.
.PARAMETER Upstream
Existing checkout at upstream.lock revision with built production classes; never rebuilt.
.PARAMETER JavaHome
Existing JDK8 root. Java heap128MiB and 60s deadline, including version command.
.PARAMETER Logger
Existing qdcxx-run-utc executable for full command/UTC/exit evidence.
.PARAMETER RunDirectory
Fresh evidence directory inside this checkout .work. Existing paths and redirecting ancestors are refused.
.PARAMETER Help
Show English help without paths, processes or files. Get-Help is also supported.
.PARAMETER HelpRu
Show Russian help without paths, processes or files.
.EXAMPLE
pwsh -File tools/upstream/server-aggregation-oracle/Invoke-Oracle.ps1 -Help
#>
