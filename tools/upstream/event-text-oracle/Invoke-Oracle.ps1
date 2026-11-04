# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

<#
.SYNOPSIS
Capture or verify explicit-context Java event/value text and narrowly classified reference differences.
.DESCRIPTION
Uses existing production classes from the pinned QD checkout; no SDK build, network or dependency acquisition.
JavaHome is JDK25 (current numeric reference); LegacyJavaHome is JDK8 (separate version observations).
One process at a time, 128MiB Java heap, timeout60 seconds; only an owned timed-out process tree is stopped.
Explicit UTC/full signed32 fixed offsets, eight named zones and en_US/fr_FR/ar_EG locales are checked. Source/class inventories,
raw streams, command UTC bounds/exits, numeric/cache proofs and receipt are retained in a fresh RunDirectory.
Capture replaces only tests/fixtures/event-text oracle/proof/provenance candidates. Verify reads them unchanged.
Exit0 is successful work/help; exit1 is invalid inputs/reference, timeout, process, provenance or observation error.
No arguments, -Help, -HelpRu and Get-Help need no operational paths and start no workload or output creation.
.PARAMETER Mode
Verify (default) compares observations and provenance; Capture writes new reviewed fixture candidates.
.PARAMETER Upstream
Existing pinned QD checkout with built production classes; no source or class is modified.
.PARAMETER JavaHome
Existing JDK25 installation used for current Double.toString and pinned QD text.
.PARAMETER LegacyJavaHome
Existing JDK8 installation, retained separately for exact per-case numeric version classification.
.PARAMETER Logger
Existing qdcxx-run-utc executable.
.PARAMETER RunDirectory
Fresh owned directory inside this checkout's .work; existing directories or reparse ancestors are refused.
.PARAMETER Help
Print input-free English help and exit before path validation or work.
.PARAMETER HelpRu
Print input-free Russian help and exit before path validation or work.
.EXAMPLE
pwsh -NoProfile -File tools/upstream/event-text-oracle/Invoke-Oracle.ps1 -Help
.EXAMPLE
pwsh -NoProfile -File tools/upstream/event-text-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/QD -JavaHome C:/jdk25 -LegacyJavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/event-text-check
#>
[CmdletBinding()]
param(
    [ValidateSet('Capture','Verify')][string]$Mode = 'Verify',
    [string]$Upstream,
    [string]$JavaHome,
    [string]$LegacyJavaHome,
    [string]$Logger,
    [string]$RunDirectory,
    [switch]$Help,
    [switch]$HelpRu
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($HelpRu) {
    Write-Output @"
Захват/проверка Java-текста событий и значений с явным контекстом и точной классификацией различий эталона.
Вызов: pwsh -NoProfile -File tools/upstream/event-text-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <QD> -JavaHome <JDK25> -LegacyJavaHome <JDK8> -Logger <qdcxx-run-utc.exe> -RunDirectory <.work/новый-каталог>
Mode: Verify по умолчанию сравнивает observations/provenance; Capture заменяет только oracle/proof/provenance кандидаты tests/fixtures/event-text.
Upstream — готовый закреплённый QD; JavaHome — JDK25, LegacyJavaHome — отдельный JDK8, Logger — готовый UTC logger; все пути обязательны для работы.
Результат: исходные streams, команды/UTC-время/exits, source/class inventories, числовые/cache proofs и receipt в новом RunDirectory.
Проверяются UTC/full signed32 fixed offsets, восемь именованных зон и локали en_US/fr_FR/ar_EG.
Ограничения: один процесс одновременно, heap128MiB, timeout60 секунд; останавливается только собственное просроченное дерево процессов.
Нет сети, загрузок или сборки SDK; исходники/classes upstream не меняются. Каталог внутри .work должен быть новым, перенаправляющие ancestors запрещены.
Коды: 0 успех/справка, 1 неверный вход/reference, timeout, ошибка процесса/provenance/наблюдений.
Без аргументов/-Help: справка EN; -HelpRu: справка RU; Get-Help: встроенная справка EN. Рабочие пути не нужны, работа/выходные файлы не создаются.
"@
    exit 0
}
if ($Help -or $PSBoundParameters.Count -eq 0) { Get-Help -Full $PSCommandPath; exit 0 }
try {
    if (!$Upstream -or !$JavaHome -or !$LegacyJavaHome -or !$Logger -or !$RunDirectory) { throw 'Required inputs missing; use -Help' }
    $repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
    $upstreamPath = (Resolve-Path -LiteralPath $Upstream).Path
    $expectedRevision = '0ddc0a09779fb61d74cc0b37c6449ff47dcb715f'
    $actualRevision = & git -C $upstreamPath rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $actualRevision -cne $expectedRevision) { throw 'Reference revision differs from pinned QD3.355' }
    $java = (Resolve-Path -LiteralPath (Join-Path $JavaHome 'bin/java.exe')).Path
    $javac = (Resolve-Path -LiteralPath (Join-Path $JavaHome 'bin/javac.exe')).Path
    $legacyJava = (Resolve-Path -LiteralPath (Join-Path $LegacyJavaHome 'bin/java.exe')).Path
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
    $sourceRelative = @('dxlib/src/main/java/com/devexperts/util/TimeFormat.java','dxlib/src/main/java/com/devexperts/util/DayUtil.java','dxlib/src/main/java/com/devexperts/util/WideDecimal.java','dxfeed-api/src/main/java/com/dxfeed/event/impl/EventUtil.java','dxfeed-api/src/main/java/com/dxfeed/event/IndexedEventSource.java','dxfeed-api/src/main/java/com/dxfeed/event/market/OrderSource.java','dxfeed-api/src/main/java/com/dxfeed/event/market/Quote.java','dxfeed-api/src/main/java/com/dxfeed/event/market/Profile.java','dxfeed-api/src/main/java/com/dxfeed/event/market/Summary.java','dxfeed-api/src/main/java/com/dxfeed/event/market/TradeBase.java','dxfeed-api/src/main/java/com/dxfeed/event/market/Trade.java','dxfeed-api/src/main/java/com/dxfeed/event/market/TradeETH.java')
    $sourceRelative += @('Direction','TradingStatus','ShortSaleRestriction','Side','Scope','OrderAction','IcebergType','PriceType','OtcMarketsPriceType','TimeAndSaleType','AuctionType','ImbalanceSide') | ForEach-Object { "dxfeed-api/src/main/java/com/dxfeed/event/market/$_.java" }
    & git -C $upstreamPath diff --quiet HEAD -- $sourceRelative
    if ($LASTEXITCODE -ne 0) { throw 'Pinned upstream event text sources differ from HEAD' }
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
    $versionLines = [regex]::Matches($version, '(?m)^(?:\d{4}-\d{2}-\d{2}T[0-9:.]+Z )?((?:openjdk|java) version "25\.[^\r\n]+)\r?$')
    if ($versionLines.Count -ne 1) { throw 'Oracle requires exactly one real JDK25 version line in bounded output' }
    $javaVersion = $versionLines[0].Groups[1].Value.TrimEnd([char]13)
    $harness = Join-Path $PSScriptRoot 'EventTextOracle.java'
    $classes = Join-Path $run 'classes'
    Invoke-Logged 'javac' $javac @('-J-Xmx128m','--release','8','-encoding','UTF-8','-cp',($classpath -join ';'),'-d',$classes,$harness,(Join-Path $PSScriptRoot 'NumericProfileVerify.java'))
    Invoke-Logged 'legacy-java-version' $legacyJava @('-Xmx128m','-version')
    $legacyVersion = [IO.File]::ReadAllText((Join-Path $run 'legacy-java-version.stderr')) + [IO.File]::ReadAllText((Join-Path $run 'legacy-java-version.stdout'))
    if($legacyVersion -notmatch '(?:openjdk|java) version "1\.8\.') { throw 'Legacy reference requires JDK8' }
    $profiles = @('UTC','fixed:19800000','fixed:-27000000','fixed:1234','fixed:-2147483648','fixed:2147483647','America/New_York','Europe/Berlin','Asia/Kathmandu','Australia/Lord_Howe','Europe/Dublin','Pacific/Apia','SystemV/EST5EDT','Asia/Tokyo')
    $joined = [Text.StringBuilder]::new()
    $profileObservations = @()
    foreach($profile in $profiles) {
        $name = $profile.Replace(':','_').Replace('-','m').Replace('/','_')
        $output = Join-Path $run "$name.tsv"
        Invoke-Logged "java-$name" $java @('-Xmx128m','-Duser.timezone=UTC','-Duser.language=en','-Duser.country=US','-cp',((@($classes)+$classpath)-join ';'),'qdcxx.oracle.EventTextOracle',$output,$profile)
        $lines = [IO.File]::ReadAllLines($output)
        foreach($line in $lines) { [void]$joined.Append($profile).Append([char]9).Append($line).Append([char]10) }
        $profileObservations += [ordered]@{id=$profile;rows=$lines.Count;sha256=(Get-FileHash -LiteralPath $output).Hash.ToLowerInvariant()}
    }
    foreach($locale in @(@('fr','FR'),@('ar','EG'))) {
        $name=$locale -join '-'
        $output=Join-Path $run "locale-$name.tsv"
        Invoke-Logged "java-locale-$name" $java @('-Xmx128m','-Duser.timezone=UTC',"-Duser.language=$($locale[0])","-Duser.country=$($locale[1])",'-cp',((@($classes)+$classpath)-join ';'),'qdcxx.oracle.EventTextOracle',$output,'UTC')
        if((Get-FileHash -LiteralPath $output).Hash -cne (Get-FileHash -LiteralPath (Join-Path $run 'UTC.tsv')).Hash) { throw "Explicit locale changes text: $name" }
    }
    Invoke-Logged 'legacy-java-UTC' $legacyJava @('-Xmx128m','-Duser.timezone=UTC','-Duser.language=en','-Duser.country=US','-cp',((@($classes)+$classpath)-join ';'),'qdcxx.oracle.EventTextOracle',(Join-Path $run 'legacy-UTC.tsv'),'UTC')
    $numericProof=Join-Path $run 'numeric-version-differences.tsv'
    Invoke-Logged 'numeric-version-proof' $java @('-Xmx128m','-cp',$classes,'qdcxx.oracle.NumericProfileVerify',(Join-Path $run 'legacy-UTC.tsv'),(Join-Path $run 'UTC.tsv'),$numericProof)
    $cacheDifferences=[Collections.Generic.List[string]]::new()
    foreach($profile in $profiles) {
        $name=$profile.Replace(':','_').Replace('-','m').Replace('/','_')
        $rows=[IO.File]::ReadAllLines((Join-Path $run "$name.tsv"))
        $cached=@{}
        foreach($line in $rows) {
            $cells=$line.Split([char]9)
            if($cells[0] -eq 'seconds') { $cached[$cells[1]]=$cells[2] }
            if($cells[0] -eq 'secondsFresh' -and $cached[$cells[1]] -cne $cells[2]) {
                if($cells[1] -cne '999' -or $profile -notin @('fixed:1234','fixed:-2147483648','fixed:2147483647','America/New_York','Europe/Berlin','Asia/Kathmandu','Australia/Lord_Howe','Europe/Dublin','Pacific/Apia','SystemV/EST5EDT','Asia/Tokyo')) { throw "Unreviewed TimeFormat cache difference: $profile $($cells[1])" }
                $cacheDifferences.Add(($profile,$cells[1],$cached[$cells[1]],$cells[2],'TimeFormat-UTC-second-cache-with-subsecond-offset') -join [char]9)
            }
        }
    }
    if($cacheDifferences.Count -ne 3) { throw 'Expected exactly three reviewed order-sensitive cache regressions' }
    $cacheProof=Join-Path $run 'upstream-cache-differences.tsv'
    [IO.File]::WriteAllText($cacheProof,($cacheDifferences -join [char]10)+[char]10,[Text.UTF8Encoding]::new($false))
    $observed=Join-Path $run 'oracle.tsv'
    [IO.File]::WriteAllText($observed,$joined.ToString(),[Text.UTF8Encoding]::new($false))
    $observedHash=(Get-FileHash -LiteralPath $observed).Hash.ToLowerInvariant()
    $after = @(ClassInventory)
    $after | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'classes-after.json')
    if(($before|ConvertTo-Json -Depth 5 -Compress) -cne ($after|ConvertTo-Json -Depth 5 -Compress)) { throw 'Production classes changed during capture' }
    $publicProvenance=[ordered]@{revision=$expectedRevision;javaVersion=$javaVersion;javaSha256=(Get-FileHash $java).Hash.ToLowerInvariant();javacSha256=(Get-FileHash $javac).Hash.ToLowerInvariant();harnessSha256=(Get-FileHash $harness).Hash.ToLowerInvariant();launcherSha256=(Get-FileHash $PSCommandPath).Hash.ToLowerInvariant();upstreamSources=$sources;productionClassInventorySha256=(Get-FileHash (Join-Path $run 'classes-before.json')).Hash.ToLowerInvariant();fixtureSha256=$observedHash;rows=@(Get-Content -LiteralPath $observed).Count;profiles=$profileObservations;localeInvariant=$true;numericProfile="Java25 Double.toString";legacyJavaSha256=(Get-FileHash $legacyJava).Hash.ToLowerInvariant();numericProofSha256=(Get-FileHash $numericProof).Hash.ToLowerInvariant();numericVerifierSha256=(Get-FileHash (Join-Path $PSScriptRoot 'NumericProfileVerify.java')).Hash.ToLowerInvariant();cacheProofSha256=(Get-FileHash $cacheProof).Hash.ToLowerInvariant()}
    $fixture=Join-Path $repo 'tests/fixtures/event-text/oracle.tsv'
    $provenance=Join-Path $repo 'tests/fixtures/event-text/provenance.json'
    if($Mode -eq 'Capture') {
        Copy-Item -LiteralPath $observed -Destination $fixture
        Copy-Item -LiteralPath $numericProof -Destination (Join-Path $repo 'tests/fixtures/event-text/numeric-version-differences.tsv')
        Copy-Item -LiteralPath $cacheProof -Destination (Join-Path $repo 'tests/fixtures/event-text/upstream-cache-differences.tsv')
        [IO.File]::WriteAllText($provenance,(($publicProvenance|ConvertTo-Json -Depth 7)+[char]10).Replace([string][char]13,''),[Text.UTF8Encoding]::new($false))
    }
    else {
        if((Get-FileHash -LiteralPath $fixture).Hash.ToLowerInvariant() -cne $observedHash) { throw 'event text fixture differs' }
        $recorded=Get-Content -LiteralPath $provenance -Raw|ConvertFrom-Json
        foreach($property in @('revision','harnessSha256','launcherSha256','fixtureSha256','productionClassInventorySha256','numericProofSha256','numericVerifierSha256','cacheProofSha256')) { if($recorded.$property -cne $publicProvenance[$property]) { throw "Provenance differs: $property" } }
        if(($recorded.upstreamSources|ConvertTo-Json -Depth 5 -Compress) -cne ($sources|ConvertTo-Json -Depth 5 -Compress)) { throw 'Upstream source hashes differ' }
    }
    [ordered]@{status='PASS';mode=$Mode;commands=$commands;provenance=$publicProvenance;loggerSha256=(Get-FileHash $loggerPath).Hash.ToLowerInvariant()}|ConvertTo-Json -Depth 9|Set-Content -LiteralPath (Join-Path $run 'receipt.json')
    Write-Output "$Mode PASS: $($publicProvenance.rows) event text observations; explicit profiles and both additional locales verified"
} catch { Write-Error $_; exit 1 }
