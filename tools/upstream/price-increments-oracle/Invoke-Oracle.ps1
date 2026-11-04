# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
<#
.SYNOPSIS
Capture or verify price-increment observations from pinned production Java QD.
.DESCRIPTION
No arguments, -Help and -HelpRu print input-free help and exit zero without work.
Capture writes only price-increment fixture/provenance; Verify compares exact output and source hashes.
Requires an existing pinned checkout with production classes, JDK8 and qdcxx-run-utc. No network or SDK build.
Git checks have timeout30s. Every Java process, including version discovery, has heap128MiB and timeout60s; owned process trees are killed on timeout.
.PARAMETER Mode
Verify (default) compares checked-in evidence; Capture replaces this oracle's fixture and provenance.
.PARAMETER Upstream
Existing checkout matching upstream.lock with built dxfeed-api, dxlib and qd-core classes.
.PARAMETER JavaHome
Existing JDK8 installation; does not alter JAVA_HOME or formatter JDK settings.
.PARAMETER Logger
Existing qdcxx-run-utc executable recording full commands, streams and exits.
.PARAMETER RunDirectory
Owned evidence directory inside this checkout's .work; run logs can be replaced.
.PARAMETER Help
Print English usage without validating inputs or starting processes.
.PARAMETER HelpRu
Print Russian usage without validating inputs or starting processes.
.EXAMPLE
pwsh -NoProfile -File tools/upstream/price-increments-oracle/Invoke-Oracle.ps1 -Help
.EXAMPLE
pwsh -NoProfile -File tools/upstream/price-increments-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/options
#>
[CmdletBinding()]
param(
    [ValidateSet('Capture','Verify')][string]$Mode='Verify',
    [string]$Upstream,
    [string]$JavaHome,
    [string]$Logger,
    [string]$RunDirectory,
    [switch]$Help,
    [switch]$HelpRu
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if ($HelpRu) {
    Write-Output @"
Захват/проверка price-increment values production Java QD, закреплённого в upstream.lock.
Вызов: pwsh -NoProfile -File tools/upstream/price-increments-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <QD> -JavaHome <JDK8> -Logger <qdcxx-run-utc> -RunDirectory <.work/каталог>
Mode: Verify по умолчанию; Capture заменяет только oracle.tsv и provenance.json этого набора.
Обязательные входы: существующие production classes QD, JDK8, UTC logger, каталог доказательств внутри .work текущего checkout.
Результат: детерминированный TSV, полные логи, хеши исходников/классов, receipt.json.
Ограничения: без сети/сборки SDK; один Java-процесс, heap128MiB, timeout60s, остановка только своего дерева процессов.
Побочные действия: запись/замена собственных run-логов; Capture также заменяет fixture/provenance.
Exit0: успех/справка; Exit1: некорректные входы, reference, процесс, timeout или несовпадение.
Без аргументов/-Help/-HelpRu: справка без рабочих входов, создания каталогов и запуска Java.
"@
    exit 0
}
if ($Help -or $PSBoundParameters.Count -eq 0) {
    Write-Output @"
Capture/verify pinned production Java QD price-increment values.
Usage: pwsh -NoProfile -File tools/upstream/price-increments-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <QD> -JavaHome <JDK8> -Logger <qdcxx-run-utc> -RunDirectory <.work/directory>
Mode: Verify (default) compares; Capture replaces only this oracle's oracle.tsv and provenance.json.
Required inputs: existing pinned production classes, JDK8, UTC logger and an owned run directory inside this checkout's .work.
Output: deterministic TSV, complete logs, source/class hashes and receipt.json. Existing owned logs may be replaced.
Limits: no network/SDK build; one Java process at a time, heap128MiB, timeout60s; only owned process trees are killed.
Exit0: success/help. Exit1: invalid inputs/reference, process failure, timeout or mismatch.
No arguments/-Help/-HelpRu require no inputs, create no directories and start no Java. Get-Help provides full English parameter help.
"@
    exit 0
}
try {
    if (!$Upstream -or !$JavaHome -or !$Logger -or !$RunDirectory) { throw 'Required inputs missing; use -Help' }
    $repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
    $run=[IO.Path]::GetFullPath($RunDirectory,$repo)
    $allowed=Join-Path $repo '.work'
    if (!$run.StartsWith($allowed+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'RunDirectory must be inside this checkout .work' }
    $reference=(Resolve-Path -LiteralPath $Upstream).Path
    $expectedRevision='0ddc0a09779fb61d74cc0b37c6449ff47dcb715f'
    function Invoke-Git([string[]]$arguments) {
        $info=[Diagnostics.ProcessStartInfo]::new()
        $info.FileName='git'; $info.UseShellExecute=$false; $info.CreateNoWindow=$true
        $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
        foreach($argument in $arguments) { $info.ArgumentList.Add($argument) }
        $process=[Diagnostics.Process]::Start($info)
        $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        $timeout= !$process.WaitForExit(30000)
        if($timeout) { $process.Kill($true); $process.WaitForExit() }
        $result=[pscustomobject]@{output=$stdout.GetAwaiter().GetResult().Trim();error=$stderr.GetAwaiter().GetResult();code=$process.ExitCode}
        $process.Dispose()
        if($timeout -or $result.code -ne 0) { throw "Reference Git check failed or timed out: $($result.error)" }
        return $result.output
    }
    $revision=Invoke-Git @('-C',$reference,'rev-parse','HEAD')
    if ($revision -cne $expectedRevision) { throw 'Reference revision differs from upstream.lock' }
    $java=(Resolve-Path -LiteralPath (Join-Path $JavaHome 'bin/java.exe')).Path
    $javac=(Resolve-Path -LiteralPath (Join-Path $JavaHome 'bin/javac.exe')).Path
    $loggerPath=(Resolve-Path -LiteralPath $Logger).Path
    $classpath=@('dxfeed-api','dxlib','qd-core') | ForEach-Object { (Resolve-Path -LiteralPath (Join-Path $reference "$_/target/classes")).Path }
    $sourcePaths=@('dxfeed-api/src/main/java/com/dxfeed/glossary/PriceIncrements.java','dxfeed-api/src/main/java/com/dxfeed/glossary/AdditionalUnderlyings.java','dxlib/src/main/java/com/devexperts/util/MathUtil.java','dxlib/src/main/java/com/devexperts/util/FastDoubleUtil.java')
    Invoke-Git (@('-C',$reference,'diff','--quiet','HEAD','--')+$sourcePaths) | Out-Null
    $sources=@($sourcePaths | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $reference $_)).Hash.ToLowerInvariant()} })
    New-Item -ItemType Directory -Force -Path $run,(Join-Path $run 'classes') | Out-Null
    $commands=[Collections.Generic.List[object]]::new()
    function Invoke-Logged([string]$name,[string]$executable,[string[]]$arguments) {
        $info=[Diagnostics.ProcessStartInfo]::new()
        $info.FileName=$loggerPath; $info.WorkingDirectory=$repo; $info.UseShellExecute=$false; $info.CreateNoWindow=$true
        $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
        foreach($argument in @('--log',(Join-Path $run "$name.utc.log"),'--',$executable)+$arguments) { $info.ArgumentList.Add($argument) }
        $start=[DateTimeOffset]::UtcNow.ToString('o')
        $process=[Diagnostics.Process]::Start($info)
        $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        $timeout= !$process.WaitForExit(60000)
        if($timeout) { $process.Kill($true); $process.WaitForExit() }
        [IO.File]::WriteAllText((Join-Path $run "$name.stdout"),$stdout.GetAwaiter().GetResult())
        [IO.File]::WriteAllText((Join-Path $run "$name.stderr"),$stderr.GetAwaiter().GetResult())
        $code=$process.ExitCode; $process.Dispose()
        $commands.Add([ordered]@{name=$name;startUtc=$start;finishUtc=[DateTimeOffset]::UtcNow.ToString('o');exitCode=$code;timedOut=$timeout})
        if($timeout -or $code -ne 0) { throw "$name failed; logs retained" }
    }
    function ClassInventory {
        for($index=0;$index -lt $classpath.Count;++$index) {
            Get-ChildItem -LiteralPath $classpath[$index] -Recurse -File | Sort-Object FullName | ForEach-Object { [ordered]@{root=$index;path=[IO.Path]::GetRelativePath($classpath[$index],$_.FullName).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()} }
        }
    }
    Invoke-Logged 'java-version' $java @('-Xmx128m','-version')
    $versionText=(Get-Content -LiteralPath (Join-Path $run 'java-version.utc.log')) -join "`n"
    $versionMatch=[regex]::Match($versionText,'(?m)(?:openjdk|java) version "1\.8\.[^\r\n]*')
    if(!$versionMatch.Success) { throw 'Actual version output does not identify JDK8' }
    $before=@(ClassInventory)
    [IO.File]::WriteAllText((Join-Path $run 'classes-before.json'),($before|ConvertTo-Json -Depth 5).Replace("`r`n","`n")+"`n")
    $harness=Join-Path $PSScriptRoot 'PriceIncrementsOracle.java'
    $classes=Join-Path $run 'classes'
    Invoke-Logged 'javac' $javac @('-J-Xmx128m','-encoding','UTF-8','-cp',($classpath -join ';'),'-d',$classes,$harness)
    $observed=Join-Path $run 'observed.tsv'
    Invoke-Logged 'java' $java @('-Xmx128m','-Duser.timezone=UTC','-Duser.language=en','-Duser.country=US','-cp',((@($classes)+$classpath)-join ';'),'qdcxx.oracle.PriceIncrementsOracle',$observed)
    $alternate=Join-Path $run 'observed-fr.tsv'
    Invoke-Logged 'java-fr' $java @('-Xmx128m','-Duser.timezone=Pacific/Honolulu','-Duser.language=fr','-Duser.country=FR','-cp',((@($classes)+$classpath)-join ';'),'qdcxx.oracle.PriceIncrementsOracle',$alternate)
    if((Get-FileHash $alternate).Hash -cne (Get-FileHash $observed).Hash) { throw 'Locale/timezone observations differ' }
    $after=@(ClassInventory)
    [IO.File]::WriteAllText((Join-Path $run 'classes-after.json'),($after|ConvertTo-Json -Depth 5).Replace("`r`n","`n")+"`n")
    if(($before|ConvertTo-Json -Depth 5 -Compress) -cne ($after|ConvertTo-Json -Depth 5 -Compress)) { throw 'Production classes changed during capture' }
    $provenance=[ordered]@{revision=$revision;javaVersion=$versionMatch.Value;javaSha256=(Get-FileHash $java).Hash.ToLowerInvariant();javacSha256=(Get-FileHash $javac).Hash.ToLowerInvariant();harnessSha256=(Get-FileHash $harness).Hash.ToLowerInvariant();launcherSha256=(Get-FileHash $PSCommandPath).Hash.ToLowerInvariant();fixtureSha256=(Get-FileHash $observed).Hash.ToLowerInvariant();upstreamSources=$sources;productionClassInventorySha256=(Get-FileHash (Join-Path $run 'classes-before.json')).Hash.ToLowerInvariant();rows=@(Get-Content $observed).Count}
    $fixture=Join-Path $repo 'tests/fixtures/price_increments/oracle.tsv'
    $provenancePath=Join-Path $repo 'tests/fixtures/price_increments/provenance.json'
    if($Mode -eq 'Capture') {
        Copy-Item -LiteralPath $observed -Destination $fixture
        [IO.File]::WriteAllText($provenancePath,($provenance|ConvertTo-Json -Depth 7).Replace("`r`n","`n")+"`n")
    } else {
        if((Get-FileHash $fixture).Hash.ToLowerInvariant() -cne $provenance.fixtureSha256) { throw 'Glossary observations differ' }
        $recorded=Get-Content -LiteralPath $provenancePath -Raw|ConvertFrom-Json
        foreach($property in @('revision','harnessSha256','launcherSha256','fixtureSha256','productionClassInventorySha256')) { if($recorded.$property -cne $provenance[$property]) { throw "Provenance differs: $property" } }
        if(($recorded.upstreamSources|ConvertTo-Json -Depth 5 -Compress) -cne ($sources|ConvertTo-Json -Depth 5 -Compress)) { throw 'Upstream source hashes differ' }
    }
    [ordered]@{status='PASS';mode=$Mode;commands=$commands;provenance=$provenance}|ConvertTo-Json -Depth 9|Set-Content -LiteralPath (Join-Path $run 'receipt.json')
    Write-Output "$Mode PASS: $($provenance.rows) production price-increment observations"
} catch { Write-Error $_; exit 1 }
