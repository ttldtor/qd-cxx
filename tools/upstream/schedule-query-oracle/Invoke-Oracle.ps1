# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

<#
.SYNOPSIS
Capture or verify actual pinned Java Schedule day/session queries.
.DESCRIPTION
Uses existing production classes and the exact reference JDK8/TZDB identity in the schedule manifest.
No upstream rebuild, download, network or reference writes. Each owned process has a 128MiB Java heap
and a 120-second timeout; only its own timed-out process tree is stopped. RunDirectory must be a new
directory inside this checkout's .work; logs, streams, observations and class inventories are retained.
Capture writes only schedule oracle/provenance candidates after both timezone/locale runs agree.
Verify compares without replacing fixtures. No arguments, -Help and -HelpRu require no working inputs,
create no run directory and launch no Java work. Exit0 success/help; exit1 input/provenance/process/check failure.
.PARAMETER Mode
Verify (default) compares tracked fixture/provenance; Capture records a new actual observation candidate.
.PARAMETER Upstream
Exact existing locked QD checkout with built dxfeed-api, dxlib and qd-core production classes.
.PARAMETER JavaHome
Reference JDK8 installation matching resources/schedule/manifest.json; independent from formatter JDK.
.PARAMETER Logger
Existing native qdcxx-run-utc executable for complete UTC command/exit logs.
.PARAMETER RunDirectory
New owned path inside this checkout's .work. Existing paths are refused to preserve previous receipts.
.PARAMETER Help
Input-free English help, without workload side effects.
.PARAMETER HelpRu
Input-free Russian help, without workload side effects.
.EXAMPLE
pwsh -NoProfile -File tools/upstream/schedule-query-oracle/Invoke-Oracle.ps1 -Help
.EXAMPLE
pwsh -NoProfile -File tools/upstream/schedule-query-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/schedule-check-new
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
    Write-Output @'
Захват/проверка фактических Schedule/Day/Session-запросов и timezone-конвенций закреплённого Java QD.
Вызов: pwsh -NoProfile -File tools/upstream/schedule-query-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <QD> -JavaHome <JDK8> -Logger <qdcxx-run-utc> -RunDirectory <.work/новый-каталог>
Mode: Verify по умолчанию, сравнивает fixture/provenance; Capture записывает кандидаты только после совпадения двух запусков.
Для работы обязательны все четыре пути, готовые production classes и точный JDK8/TZDB из manifest расписаний.
Результат: полные UTC-логи, raw streams, class inventories и наблюдения UTC/en и Pacific-Honolulu/fr.
RunDirectory должен быть новым внутри .work текущего checkout; существующий каталог не изменяется.
Ограничения: heap128MiB, timeout120 секунд на собственный процесс; без сети, загрузок и upstream-сборки/записи.
Останавливается только собственное дерево процессов по timeout. Capture меняет только schedule oracle/provenance.
Без аргументов/-Help/-HelpRu: справка без рабочих входов, создания каталога и запуска Java.
Exit0: успех/справка. Exit1: входы, provenance, timeout, процесс или несовпадение наблюдений.
'@
    exit 0
}
if ($Help -or $PSBoundParameters.Count -eq 0) { Get-Help -Full $PSCommandPath; exit 0 }
try {
    if (!$Upstream -or !$JavaHome -or !$Logger -or !$RunDirectory) { throw 'Required inputs missing; use -Help' }
    $repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
    $run = [IO.Path]::GetFullPath($RunDirectory,$repo)
    $allowed = Join-Path $repo '.work'
    if (!$run.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'RunDirectory must be inside checkout .work' }
    if (Test-Path -LiteralPath $run) { throw 'RunDirectory must be new; previous logs are preserved' }
    $reference = (Resolve-Path -LiteralPath $Upstream).Path
    $revision = '0ddc0a09779fb61d74cc0b37c6449ff47dcb715f'
    if ((& git -C $reference rev-parse HEAD) -cne $revision -or $LASTEXITCODE -ne 0) { throw 'Wrong upstream revision' }
    $suffix = if ($IsWindows) { '.exe' } else { '' }
    $java = (Resolve-Path -LiteralPath (Join-Path $JavaHome "bin/java$suffix")).Path
    $javac = (Resolve-Path -LiteralPath (Join-Path $JavaHome "bin/javac$suffix")).Path
    $loggerPath = (Resolve-Path -LiteralPath $Logger).Path
    $manifest = Get-Content -LiteralPath (Join-Path $repo 'resources/schedule/manifest.json') -Raw | ConvertFrom-Json
    $tzdb = Join-Path $JavaHome 'jre/lib/tzdb.dat'
    if ((Get-FileHash -LiteralPath $java).Hash -ine $manifest.zoneIdentityProfile.javaSha256 -or
        (Get-FileHash -LiteralPath $tzdb).Hash -ine $manifest.zoneIdentityProfile.tzdbSha256) { throw 'Reference JDK/TZDB differs from the reviewed identity profile' }
    $classpath = @('dxfeed-api','dxlib','qd-core') | ForEach-Object { (Resolve-Path -LiteralPath (Join-Path $reference "$_/target/classes")).Path }
    $sourcePaths = @('dxfeed-api/src/main/java/com/dxfeed/schedule/Schedule.java','dxfeed-api/src/main/java/com/dxfeed/schedule/Day.java','dxfeed-api/src/main/java/com/dxfeed/schedule/Session.java','dxfeed-api/src/main/java/com/dxfeed/schedule/SessionType.java','dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfile.java','dxfeed-api/src/main/resources/com/dxfeed/schedule/schedule.properties','dxlib/src/main/java/com/devexperts/util/TimeUtil.java','dxlib/src/main/java/com/devexperts/util/DayUtil.java','dxlib/src/main/java/com/devexperts/util/TimeFormat.java')
    $sourcePaths += @('dxfeed-api/src/main/java/com/dxfeed/schedule/DayFilter.java','dxfeed-api/src/main/java/com/dxfeed/schedule/SessionFilter.java')
    & git -C $reference diff --quiet HEAD -- $sourcePaths
    if ($LASTEXITCODE -ne 0) { throw 'Pinned production sources changed' }
    $sources = @($sourcePaths | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $reference $_)).Hash.ToLowerInvariant()} })
    New-Item -ItemType Directory -Path $run,(Join-Path $run 'classes') | Out-Null
    function Inventory {
        for ($index=0; $index -lt $classpath.Count; ++$index) {
            Get-ChildItem -LiteralPath $classpath[$index] -Recurse -File | Sort-Object FullName | ForEach-Object { [ordered]@{root=$index;path=[IO.Path]::GetRelativePath($classpath[$index],$_.FullName).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()} }
        }
    }
    $before = @(Inventory)
    $before | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'classes-before.json')
    $commands = [Collections.Generic.List[object]]::new()
    function Run([string]$name,[string]$executable,[string[]]$arguments) {
        $info = [Diagnostics.ProcessStartInfo]::new()
        $info.FileName=$loggerPath; $info.WorkingDirectory=$repo; $info.UseShellExecute=$false; $info.CreateNoWindow=$true
        $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
        foreach($argument in @('--log',(Join-Path $run "$name.utc.log"),'--',$executable)+$arguments){$info.ArgumentList.Add($argument)}
        $start=[DateTimeOffset]::UtcNow.ToString('o')
        $process=[Diagnostics.Process]::Start($info)
        $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
        $timeout=!$process.WaitForExit(120000)
        if($timeout){$process.Kill($true);$process.WaitForExit()}
        [IO.File]::WriteAllText((Join-Path $run "$name.stdout"),$stdout.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $run "$name.stderr"),$stderr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        $code=$process.ExitCode;$process.Dispose()
        $commands.Add([ordered]@{name=$name;startUtc=$start;finishUtc=[DateTimeOffset]::UtcNow.ToString('o');exitCode=$code;timedOut=$timeout})
        if($timeout -or $code -ne 0){throw "$name failed; raw streams retained"}
    }
    Run 'java-version' $java @('-Xmx128m','-version')
    $version=[IO.File]::ReadAllText((Join-Path $run 'java-version.stderr')) + [IO.File]::ReadAllText((Join-Path $run 'java-version.stdout'))
    if($version -notmatch 'version "1\.8\.'){throw 'JDK8 required'}
    $publicVersion = (($version -split '\r?\n' | ForEach-Object { $_ -replace '^\d{4}-\d{2}-\d{2}T\S+ ', '' } | Where-Object { $_ -match '^(openjdk|java) version |Runtime Environment|Server VM ' }) -join "`n").Trim()
    if(!$publicVersion){throw 'Missing deterministic Java version lines'}
    $harness=Join-Path $PSScriptRoot 'ScheduleQueryOracle.java'
    $cases=Join-Path $repo 'tests/fixtures/schedule-query/cases.tsv'
    $identityInput=Join-Path $repo 'resources/schedule/java8-zone-ids.txt'
    $classes=Join-Path $run 'classes'
    Run 'javac' $javac @('-J-Xmx128m','-encoding','UTF-8','-cp',($classpath -join [IO.Path]::PathSeparator),'-d',$classes,$harness)
    foreach($profile in @('UTC','Pacific-Honolulu')) {
        $timezone=if($profile -eq 'UTC'){'UTC'}else{'Pacific/Honolulu'}
        $language=if($profile -eq 'UTC'){'en'}else{'fr'}
        Run "java-$profile" $java @('-Xmx128m','-Dfile.encoding=UTF-8','-Dcom.dxfeed.schedule.download=','-Dcom.devexperts.util.TimeUtil.skipTimeZoneValidation=false',"-Duser.timezone=$timezone","-Duser.language=$language",'-cp',((@($classes)+$classpath)-join [IO.Path]::PathSeparator),'qdcxx.oracle.ScheduleQueryOracle',$cases,(Join-Path $run "$profile.tsv"),$identityInput,(Join-Path $run "$profile.metadata.tsv"),(Join-Path $run "$profile.legacy.tsv"))
    }
    $observed=Join-Path $run 'UTC.tsv'
    $observedHash=(Get-FileHash -LiteralPath $observed).Hash.ToLowerInvariant()
    if($observedHash -cne (Get-FileHash -LiteralPath (Join-Path $run 'Pacific-Honolulu.tsv')).Hash.ToLowerInvariant()){throw 'Timezone/locale changed observations'}
    $identityObservations=@()
    foreach($pair in @(@('metadata','java8-zone-metadata.tsv'),@('legacy','java8-systemv-transitions.tsv'))){
        $utcPath=Join-Path $run "UTC.$($pair[0]).tsv"
        $alternate=Join-Path $run "Pacific-Honolulu.$($pair[0]).tsv"
        $tracked=Join-Path $repo "resources/time-zone-data/$($pair[1])"
        $hash=(Get-FileHash -LiteralPath $utcPath).Hash.ToLowerInvariant()
        if($hash -cne (Get-FileHash -LiteralPath $alternate).Hash.ToLowerInvariant() -or
           $hash -cne (Get-FileHash -LiteralPath $tracked).Hash.ToLowerInvariant()){
            throw "Actual timezone identity observations differ: $($pair[0])"
        }
        $identityObservations += [ordered]@{path="resources/time-zone-data/$($pair[1])";sha256=$hash;rows=@(Get-Content -LiteralPath $utcPath).Count}
    }
    $after=@(Inventory)
    $after|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $run 'classes-after.json')
    if(($before|ConvertTo-Json -Depth 5 -Compress) -cne ($after|ConvertTo-Json -Depth 5 -Compress)){throw 'Production classes changed'}
    $provenance=[ordered]@{schemaVersion=1;revision=$revision;javaVersion=$publicVersion;javaSha256=(Get-FileHash -LiteralPath $java).Hash.ToLowerInvariant();javacSha256=(Get-FileHash -LiteralPath $javac).Hash.ToLowerInvariant();tzdbSha256=(Get-FileHash -LiteralPath $tzdb).Hash.ToLowerInvariant();harnessSha256=(Get-FileHash -LiteralPath $harness).Hash.ToLowerInvariant();launcherSha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash.ToLowerInvariant();casesSha256=(Get-FileHash -LiteralPath $cases).Hash.ToLowerInvariant();upstreamSources=$sources;productionClassInventorySha256=(Get-FileHash -LiteralPath (Join-Path $run 'classes-before.json')).Hash.ToLowerInvariant();fixtureSha256=$observedHash;rows=@(Get-Content -LiteralPath $observed).Count;timezoneLocaleInvariant=$true;scope='Actual production factories and constructed rule fields; no date-specific Day/Session query implementation'}
    $provenance.scope='Actual production Schedule/Day/Session queries and hot updates; actual Java TimeZone raw components and complete legacy identity observations; IANA2026f differences are separate contextual data expectations'
    $provenance.identityObservations=$identityObservations
    $provenance.identityInputSha256=(Get-FileHash -LiteralPath $identityInput).Hash.ToLowerInvariant()
    $fixture=Join-Path $repo 'tests/fixtures/schedule-query/oracle.tsv'
    $metadata=Join-Path $repo 'tests/fixtures/schedule-query/provenance.json'
    if($Mode -eq 'Capture') {
        Copy-Item -LiteralPath $observed -Destination $fixture
        [IO.File]::WriteAllText($metadata,(($provenance|ConvertTo-Json -Depth 8).Replace("`r",'').TrimEnd("`n")+"`n"),[Text.UTF8Encoding]::new($false))
    } else {
        if((Get-FileHash -LiteralPath $fixture).Hash.ToLowerInvariant() -cne $observedHash){throw 'Schedule fixture differs'}
        $recorded=Get-Content -LiteralPath $metadata -Raw|ConvertFrom-Json
        foreach($key in @('revision','javaSha256','tzdbSha256','casesSha256','harnessSha256','launcherSha256','fixtureSha256','productionClassInventorySha256','identityInputSha256')){if($recorded.$key -cne $provenance[$key]){throw "Provenance differs: $key"}}
        if(($recorded.identityObservations|ConvertTo-Json -Depth 5 -Compress) -cne ($identityObservations|ConvertTo-Json -Depth 5 -Compress)){throw 'Timezone identity provenance differs'}
        if(($recorded.upstreamSources|ConvertTo-Json -Depth 5 -Compress) -cne ($sources|ConvertTo-Json -Depth 5 -Compress)){throw 'Upstream source provenance differs'}
    }
    [ordered]@{status='PASS';mode=$Mode;commands=$commands;provenance=$provenance;loggerSha256=(Get-FileHash -LiteralPath $loggerPath).Hash.ToLowerInvariant()}|ConvertTo-Json -Depth 10|Set-Content -LiteralPath (Join-Path $run 'receipt.json')
    Write-Output "$Mode PASS: $($provenance.rows) actual Schedule observations, two timezone/locale profiles"
} catch { Write-Error $_; exit 1 }
