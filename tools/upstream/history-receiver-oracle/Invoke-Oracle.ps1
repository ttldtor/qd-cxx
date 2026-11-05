# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

<#
.SYNOPSIS
Capture or verify actual pinned Java HISTORY receiver and standard TimeAndSale observations.
.DESCRIPTION
Uses existing pinned production classes and JDK8, one process at a time, heap128MiB, timeout60s.
Requires all four operational paths. RunDirectory must be a new contained nonredirected .work directory.
Capture replaces only tests/fixtures/history-receiver/oracle.tsv, provenance.json and seven wire-step-N.qtp files. Verify compares them.
Writes full UTC commands/exits, raw streams, complete class inventories and receipt privately. No SDK build/network.
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
pwsh -File tools/upstream/history-receiver-oracle/Invoke-Oracle.ps1 -Help
#>
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
 Write-Output 'Захват/проверка actual production Java8 HISTORY receiver/standard TimeAndSale. -Mode Verify (default) или Capture; обязательны -Upstream <pinned QD>, -JavaHome <JDK8>, -Logger <UTC logger>, -RunDirectory <новый каталог внутри .work>. Capture заменяет только tests/fixtures/history-receiver/oracle.tsv, provenance.json и семь wire-step-N.qtp; Verify сравнивает. Создаются private UTC/raw/class inventories/receipt. Один процесс, heap128MiB, timeout60s; без сети/SDK rebuild. Exit0 успех/справка, Exit1 invalid input/reference/timeout/process/mismatch. -Help/-HelpRu и no args не запускают работу; Get-Help содержит EN параметры.'
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
    $sourceRelative = @('dxfeed-api/src/main/java/com/dxfeed/event/TimeSeriesEvent.java','dxfeed-api/src/main/java/com/dxfeed/event/market/TimeAndSale.java','dxfeed-impl/src/main/java/com/dxfeed/api/impl/EventDelegate.java','dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXFeedImpl.java','dxfeed-impl/src/main/java/com/dxfeed/event/market/TimeAndSaleDelegate.java','dxfeed-impl/src/main/java/com/dxfeed/event/market/MarketFactoryImpl.java','qd-core/src/main/java/com/devexperts/qd/impl/matrix/History.java')
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
    $harness = Join-Path $PSScriptRoot 'HistoryReceiverOracle.java'
    $classes = Join-Path $run 'classes'
    Invoke-Logged 'javac' $javac @('-J-Xmx128m','-encoding','UTF-8','-cp',($classpath -join ';'),'-d',$classes,$harness)
    foreach ($zone in @('UTC','Pacific/Honolulu')) {
        $zoneName=$zone.Replace('/','-')
        Invoke-Logged "java-$zoneName" $java @('-Xmx128m',"-Duser.timezone=$zone",'-Duser.language=en','-Duser.country=US','-cp',((@($classes)+$classpath)-join ';'),'HistoryReceiverOracle',(Join-Path $run "$zoneName.tsv"))
    }
    $observed=Join-Path $run 'UTC.tsv'
    if((Get-FileHash $observed).Hash -cne (Get-FileHash (Join-Path $run 'Pacific-Honolulu.tsv')).Hash) {throw 'Timezone changed history-receiver observations'}
    $wireFiles = @(Get-ChildItem (Join-Path $run 'UTC.tsv.wire') -File | Sort-Object Name)
    if ($wireFiles.Count -ne 7 -or ($wireFiles | Measure-Object Length -Sum).Sum -gt 65536) {throw 'Unexpected wire fixture count/size'}
    $wireHashes = @($wireFiles | ForEach-Object {
        if ((Get-FileHash $_.FullName).Hash -cne (Get-FileHash (Join-Path (Join-Path $run 'Pacific-Honolulu.tsv.wire') $_.Name)).Hash) {throw 'Timezone wire mismatch'}
        [ordered]@{path=$_.Name;sha256=(Get-FileHash $_.FullName).Hash.ToLowerInvariant();bytes=$_.Length}
    })
    $observedHash=(Get-FileHash -LiteralPath $observed).Hash.ToLowerInvariant()
    $after = @(ClassInventory)
    $after | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'classes-after.json')
    if(($before|ConvertTo-Json -Depth 5 -Compress) -cne ($after|ConvertTo-Json -Depth 5 -Compress)) { throw 'Production classes changed during capture' }
    $publicProvenance=[ordered]@{revision=$expectedRevision;javaVersion=$javaVersion;javaSha256=(Get-FileHash $java).Hash.ToLowerInvariant();javacSha256=(Get-FileHash $javac).Hash.ToLowerInvariant();harnessSha256=(Get-FileHash $harness).Hash.ToLowerInvariant();launcherSha256=(Get-FileHash $PSCommandPath).Hash.ToLowerInvariant();upstreamSources=$sources;productionClassInventorySha256=(Get-FileHash (Join-Path $run 'classes-before.json')).Hash.ToLowerInvariant();fixtureSha256=$observedHash;rows=@(Get-Content -LiteralPath $observed).Count;timezoneInvariant=$true;wire=$wireHashes}
    $fixture=Join-Path $repo 'tests/fixtures/history-receiver/oracle.tsv'
    $provenance=Join-Path $repo 'tests/fixtures/history-receiver/provenance.json'
    if($Mode -eq 'Capture') {
        Copy-Item -LiteralPath $observed -Destination $fixture
        foreach ($wire in $wireFiles) {Copy-Item -LiteralPath $wire.FullName -Destination (Join-Path (Split-Path $fixture) $wire.Name)}
        [IO.File]::WriteAllText($provenance,(($publicProvenance|ConvertTo-Json -Depth 7)+[char]10).Replace([string][char]13,''),[Text.UTF8Encoding]::new($false))
    }
    else {
        if((Get-FileHash -LiteralPath $fixture).Hash.ToLowerInvariant() -cne $observedHash) { throw 'Time-series fixture differs' }
        foreach ($wire in $wireHashes) {
            if ((Get-FileHash (Join-Path (Split-Path $fixture) $wire.path)).Hash.ToLowerInvariant() -cne $wire.sha256) {throw 'Wire fixture differs'}
        }
        $recorded=Get-Content -LiteralPath $provenance -Raw|ConvertFrom-Json
        if (($recorded.wire|ConvertTo-Json -Depth 5 -Compress) -cne ($wireHashes|ConvertTo-Json -Depth 5 -Compress)) {throw 'Wire provenance differs'}
        foreach($property in @('revision','harnessSha256','launcherSha256','fixtureSha256','productionClassInventorySha256')) { if($recorded.$property -cne $publicProvenance[$property]) { throw "Provenance differs: $property" } }
        if(($recorded.upstreamSources|ConvertTo-Json -Depth 5 -Compress) -cne ($sources|ConvertTo-Json -Depth 5 -Compress)) { throw 'Upstream source hashes differ' }
    }
    [ordered]@{status='PASS';mode=$Mode;commands=$commands;provenance=$publicProvenance;loggerSha256=(Get-FileHash $loggerPath).Hash.ToLowerInvariant()}|ConvertTo-Json -Depth 9|Set-Content -LiteralPath (Join-Path $run 'receipt.json')
    Write-Output "$Mode PASS: $($publicProvenance.rows) history-receiver observations; both timezones match"
} catch { [Console]::Error.WriteLine($_.Exception.Message); [Console]::Error.WriteLine('Use -Help or -HelpRu for parameters, limits and side effects.'); exit 1 }
