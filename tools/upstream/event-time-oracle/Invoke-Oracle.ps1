# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

<#
.SYNOPSIS
Capture or verify deterministic event-time observations against pinned Java QD.
.DESCRIPTION
Uses existing production classes and JDK8. Starts one bounded Java process at a time, no network or
SDK build. Capture replaces only the event-time fixture/provenance; Verify compares without replacing them.
Upstream, JavaHome, Logger and RunDirectory are required for operational Capture/Verify modes.
RunDirectory must be inside this checkout's .work directory. Java processes use a 128MiB heap limit
and a 60-second timeout; only the launcher's own timed-out process tree is stopped.
Full command logs and class/source inventories are written to RunDirectory. Exit0 is success/help;
exit1 is invalid arguments, unavailable/mismatched reference, timeout, process or observation failure.
.PARAMETER Mode
Verify (default) compares observations; Capture writes reviewed fixture/provenance candidates.
.PARAMETER Upstream
Existing Java QD checkout at the exact upstream.lock revision, with built production classes.
.PARAMETER JavaHome
Existing oracle JDK8 installation; formatter JDK is separate.
.PARAMETER Logger
Existing qdcxx-run-utc executable for complete command/exit logging.
.PARAMETER RunDirectory
New owned evidence directory inside this checkout's .work; existing directories are refused.
.PARAMETER Help
Print this help without validating inputs, creating directories or launching processes.
.PARAMETER HelpRu
Print Russian help without inputs or work.
.EXAMPLE
pwsh -File tools/upstream/event-time-oracle/Invoke-Oracle.ps1 -Help
.EXAMPLE
pwsh -File tools/upstream/event-time-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/event-time-check
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
    Write-Output @"
Захват/проверка полей времени событий закреплённого Java QD3.355.
Вызов: pwsh -File tools/upstream/event-time-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <QD> -JavaHome <JDK8> -Logger <qdcxx-run-utc.exe> -RunDirectory <.work/каталог>
Mode: Verify по умолчанию, сравнивает fixture/provenance; Capture заменяет только event-time fixture/provenance кандидатами.
Upstream, JavaHome, Logger, RunDirectory обязательны для работы. Нужны готовые production classes, JDK8 и UTC logger.
Результат: полные логи команд, raw streams, class inventories, наблюдения UTC/Pacific-Honolulu и receipt в RunDirectory.
Ограничения: один процесс одновременно, heap128MiB, timeout60 секунд; без сети, загрузок и сборки SDK.
Каталог должен находиться внутри .work текущего checkout; существующий каталог запрещён, прежние логи сохраняются.
Exit0: успех/справка. Exit1: вход, reference/provenance/наблюдения, timeout или процесс завершился с ошибкой.
-Help/-HelpRu доступны без рабочих входов и не создают каталоги/не начинают Java; Get-Help показывает справку EN.
"@
    exit 0
}
if ($Help -or $PSBoundParameters.Count -eq 0) { Get-Help -Full $PSCommandPath; exit 0 }
try {
    if (!$Upstream -or !$JavaHome -or !$Logger -or !$RunDirectory) { throw 'Required inputs missing; use -Help' }
    $repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
    $upstreamPath = (Resolve-Path -LiteralPath $Upstream).Path
    $expectedRevision = '0ddc0a09779fb61d74cc0b37c6449ff47dcb715f'
    $actualRevision = & git -C $upstreamPath rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $actualRevision -cne $expectedRevision) { throw 'Reference revision differs from pinned QD3.355' }
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
    $sourceRelative = @('dxfeed-api/src/main/java/com/dxfeed/event/market/Quote.java','dxfeed-api/src/main/java/com/dxfeed/event/market/Profile.java','dxfeed-api/src/main/java/com/dxfeed/event/market/MarketEventSymbols.java','dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/QuoteMapping.java','dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/ProfileMapping.java','dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/MarketEventMapping.java','dxfeed-impl/src/main/java/com/dxfeed/event/market/QuoteDelegate.java','dxfeed-impl/src/main/java/com/dxfeed/event/market/ProfileDelegate.java','dxfeed-impl/src/main/java/com/dxfeed/event/market/MarketEventDelegateImpl.java','dxfeed-impl/src/main/java/com/dxfeed/event/market/MarketFactoryImpl.java','dxfeed-impl/src/main/java/com/dxfeed/api/impl/EventDelegate.java','qd-core/src/main/java/com/devexperts/qd/ng/RecordMapping.java','dxfeed-impl/src/main/java/com/dxfeed/event/market/MarketEventDelegateSet.java','dxfeed-impl/src/main/java/com/dxfeed/api/impl/EventDelegateSet.java','qd-core/src/main/java/com/devexperts/qd/util/TimeSequenceUtil.java','dxfeed-api/src/main/java/com/dxfeed/event/impl/TimeNanosUtil.java','dxfeed-impl/src/main/java/com/dxfeed/api/impl/SchemeProperties.java')
    & git -C $upstreamPath diff --quiet HEAD -- $sourceRelative
    if ($LASTEXITCODE -ne 0) { throw 'Pinned upstream event-time sources differ from HEAD' }
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
    Invoke-Logged 'java-version' $java @('-Xmx128m','-version')
    $version = ([IO.File]::ReadAllText((Join-Path $run 'java-version.stderr')) + [IO.File]::ReadAllText((Join-Path $run 'java-version.stdout'))).Trim()
    $javaVersion = [regex]::Match($version, '(?m)^(?:\S+Z )?((?:openjdk|java) version "1\.8\.[^"\r\n]+")\r?$').Groups[1].Value
    if (!$javaVersion) { throw 'Oracle requires JDK8' }
    $harness = Join-Path $PSScriptRoot 'EventTimeOracle.java'
    $classes = Join-Path $run 'classes'
    Invoke-Logged 'javac' $javac @('-J-Xmx128m','-encoding','UTF-8','-cp',($classpath -join ';'),'-d',$classes,$harness)
    $cases = [ordered]@{default=@();sequence=@('-Ddxscheme.enabled.Sequence=Quote');nano=@('-Ddxscheme.enabled.TimeNanoPart=Quote');both=@('-Ddxscheme.nanoTime=true');millis=@('-Ddxscheme.nanoTime=true','-Ddxscheme.bat=millis');'millis-default'=@('-Ddxscheme.bat=millis');minimal=@();reordered=@('-Ddxscheme.nanoTime=true');missing=@()}
    foreach ($zone in @('UTC','Pacific/Honolulu')) {
        $zoneName = $zone.Replace('/','-')
        foreach ($case in $cases.GetEnumerator()) {
            Invoke-Logged "java-$zoneName-$($case.Key)" $java (@('-Xmx128m',"-Duser.timezone=$zone",'-Duser.language=en','-Duser.country=US','-Dcom.dxfeed.event.market.impl.Quote.exchanges=Qz')+$case.Value+@('-cp',((@($classes)+$classpath)-join ';'),'qdcxx.oracle.EventTimeOracle',(Join-Path $run "$zoneName-$($case.Key).tsv"),$case.Key))
        }
    }
    $observed = Join-Path $run 'oracle.tsv'
    $joined = ($cases.Keys | ForEach-Object { [IO.File]::ReadAllText((Join-Path $run "UTC-$_.tsv")) }) -join ''
    [IO.File]::WriteAllText($observed,$joined,[Text.UTF8Encoding]::new($false))
    foreach($case in $cases.Keys) {
        if((Get-FileHash (Join-Path $run "UTC-$case.tsv")).Hash -cne (Get-FileHash (Join-Path $run "Pacific-Honolulu-$case.tsv")).Hash) { throw "Timezone changed event-time observations: $case" }
    }
    $observedHash=(Get-FileHash -LiteralPath $observed).Hash.ToLowerInvariant()
    $representations=Join-Path $run 'UTC-default-representability.tsv'
    $representationHash=(Get-FileHash -LiteralPath $representations).Hash.ToLowerInvariant()
    if($representationHash -cne (Get-FileHash (Join-Path $run 'Pacific-Honolulu-default-representability.tsv')).Hash.ToLowerInvariant()) { throw 'Timezone changed regional representability observations' }
    $after = @(ClassInventory)
    $after | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'classes-after.json')
    if(($before|ConvertTo-Json -Depth 5 -Compress) -cne ($after|ConvertTo-Json -Depth 5 -Compress)) { throw 'Production classes changed during capture' }
    $publicProvenance=[ordered]@{revision=$expectedRevision;javaVersion=$javaVersion;javaSha256=(Get-FileHash $java).Hash.ToLowerInvariant();javacSha256=(Get-FileHash $javac).Hash.ToLowerInvariant();harnessSha256=(Get-FileHash $harness).Hash.ToLowerInvariant();launcherSha256=(Get-FileHash $PSCommandPath).Hash.ToLowerInvariant();upstreamSources=$sources;productionClassInventorySha256=(Get-FileHash (Join-Path $run 'classes-before.json')).Hash.ToLowerInvariant();fixtureSha256=$observedHash;rows=@(Get-Content -LiteralPath $observed).Count;timezoneInvariant=$true}
    $fixture=Join-Path $repo 'tests/fixtures/event-times/oracle.tsv'
    $representationFixture=Join-Path $repo 'tests/fixtures/event-times/representability.tsv'
    $provenance=Join-Path $repo 'tests/fixtures/event-times/provenance.json'
    $publicProvenance['representabilitySha256']=$representationHash
    $publicProvenance['representabilityRows']=@(Get-Content -LiteralPath $representations).Count
    if($Mode -eq 'Capture') { Copy-Item -LiteralPath $observed -Destination $fixture; Copy-Item -LiteralPath $representations -Destination $representationFixture; [IO.File]::WriteAllText($provenance,(($publicProvenance|ConvertTo-Json -Depth 7)+[char]10).Replace([string][char]13,''),[Text.UTF8Encoding]::new($false)) }
    else {
        if((Get-FileHash -LiteralPath $fixture).Hash.ToLowerInvariant() -cne $observedHash) { throw 'event-time fixture differs' }
        if((Get-FileHash -LiteralPath $representationFixture).Hash.ToLowerInvariant() -cne $representationHash) { throw 'Regional representability fixture differs' }
        $recorded=Get-Content -LiteralPath $provenance -Raw|ConvertFrom-Json
        foreach($property in @('revision','harnessSha256','launcherSha256','fixtureSha256','productionClassInventorySha256','representabilitySha256','representabilityRows')) { if($recorded.$property -cne $publicProvenance[$property]) { throw "Provenance differs: $property" } }
        if(($recorded.upstreamSources|ConvertTo-Json -Depth 5 -Compress) -cne ($sources|ConvertTo-Json -Depth 5 -Compress)) { throw 'Upstream source hashes differ' }
    }
    [ordered]@{status='PASS';mode=$Mode;commands=$commands;provenance=$publicProvenance;loggerSha256=(Get-FileHash $loggerPath).Hash.ToLowerInvariant()}|ConvertTo-Json -Depth 9|Set-Content -LiteralPath (Join-Path $run 'receipt.json')
    Write-Output "$Mode PASS: $($publicProvenance.rows) event-time observations; both timezones match"
} catch { Write-Error $_; exit 1 }
