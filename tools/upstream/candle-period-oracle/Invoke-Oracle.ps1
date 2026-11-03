# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

<#
.SYNOPSIS
Capture or verify deterministic candle-period observations against pinned Java QD.
.DESCRIPTION
Uses existing production classes and JDK8. Starts one bounded Java process at a time, no network or
SDK build. Capture replaces only the candle-period fixture/provenance; Verify compares without replacing them.
Upstream, JavaHome, Logger and RunDirectory are required for Capture/Verify; Mode defaults to Verify.
RunDirectory must be inside this checkout's .work; its own logs/observations can be replaced.
Each Java process has a 128MiB maximum heap and a 60-second timeout. Only its own timed-out process tree is stopped.
No arguments, -Help and -HelpRu print help without inputs, file creation or Java/SDK work.
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
New or existing owned evidence directory; logs and observations can be replaced there.
.PARAMETER Help
Print this help without validating inputs, creating directories or launching processes.
.PARAMETER HelpRu
Print Russian help without inputs or work.
.EXAMPLE
pwsh -File tools/upstream/candle-period-oracle/Invoke-Oracle.ps1 -Help
.EXAMPLE
pwsh -File tools/upstream/candle-period-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/candle-period-check
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
Захват/проверка значений периодов свечей закреплённого Java QD3.355.
Вызов: pwsh -File tools/upstream/candle-period-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <QD> -JavaHome <JDK8> -Logger <qdcxx-run-utc.exe> -RunDirectory <.work/каталог>
Mode: Verify по умолчанию, сравнивает fixture/provenance; Capture заменяет только candle-period fixture/provenance кандидатами.
Upstream, JavaHome, Logger, RunDirectory обязательны для работы. Нужны готовые production classes, JDK8 и UTC logger.
Результат: полные логи команд, raw streams, class inventories, наблюдения UTC/Pacific-Honolulu и receipt в RunDirectory.
Ограничения: один процесс одновременно, heap128MiB, timeout60 секунд; без сети, загрузок и сборки SDK.
Каталог должен находиться внутри .work текущего checkout; его собственные логи могут заменяться.
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
    New-Item -ItemType Directory -Force -Path $run,(Join-Path $run 'classes') | Out-Null
    $classpath = @('dxfeed-api','dxlib','qd-core') | ForEach-Object { (Resolve-Path -LiteralPath (Join-Path $upstreamPath "$_/target/classes")).Path }
    $sourceRelative = @('dxfeed-api/src/main/java/com/dxfeed/event/candle/CandleType.java','dxfeed-api/src/main/java/com/dxfeed/event/candle/CandlePeriod.java')
    & git -C $upstreamPath diff --quiet HEAD -- $sourceRelative
    if ($LASTEXITCODE -ne 0) { throw 'Pinned upstream candle-period sources differ from HEAD' }
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
    $versionText = [IO.File]::ReadAllText((Join-Path $run 'java-version.stdout')) + [IO.File]::ReadAllText((Join-Path $run 'java-version.stderr'))
    $versionMatch = [regex]::Match($versionText, '(?:openjdk|java) version "1\.8\.[^"]+"')
    if (!$versionMatch.Success) { throw 'Oracle requires JDK8' }
    $version = $versionMatch.Value
    $harness = Join-Path $PSScriptRoot 'CandlePeriodOracle.java'
    $classes = Join-Path $run 'classes'
    Invoke-Logged 'javac' $javac @('-J-Xmx128m','-encoding','UTF-8','-cp',($classpath -join ';'),'-d',$classes,$harness)
    foreach ($zone in @('UTC','Pacific/Honolulu')) {
        $zoneName = $zone.Replace('/','-')
        Invoke-Logged "java-$zoneName" $java @('-Xmx128m',"-Duser.timezone=$zone",'-Duser.language=en','-Duser.country=US','-cp',((@($classes)+$classpath)-join ';'),'qdcxx.oracle.CandlePeriodOracle',(Join-Path $run "$zoneName.tsv"))
    }
    $observed = Join-Path $run 'UTC.tsv'
    $observedHash=(Get-FileHash -LiteralPath $observed).Hash.ToLowerInvariant()
    if($observedHash -cne (Get-FileHash -LiteralPath (Join-Path $run 'Pacific-Honolulu.tsv')).Hash.ToLowerInvariant()) { throw 'Timezone changed candle-period observations' }
    $after = @(ClassInventory)
    $after | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'classes-after.json')
    if(($before|ConvertTo-Json -Depth 5 -Compress) -cne ($after|ConvertTo-Json -Depth 5 -Compress)) { throw 'Production classes changed during capture' }
    $publicProvenance=[ordered]@{revision=$expectedRevision;javaVersion=($version -split "`n")[0];javaSha256=(Get-FileHash $java).Hash.ToLowerInvariant();javacSha256=(Get-FileHash $javac).Hash.ToLowerInvariant();harnessSha256=(Get-FileHash $harness).Hash.ToLowerInvariant();launcherSha256=(Get-FileHash $PSCommandPath).Hash.ToLowerInvariant();upstreamSources=$sources;productionClassInventorySha256=(Get-FileHash (Join-Path $run 'classes-before.json')).Hash.ToLowerInvariant();fixtureSha256=$observedHash;rows=@(Get-Content -LiteralPath $observed).Count;timezoneInvariant=$true}
    $fixture=Join-Path $repo 'tests/fixtures/events/candle/period-oracle.tsv'
    $provenance=Join-Path $repo 'tests/fixtures/events/candle/period-provenance.json'
    if($Mode -eq 'Capture') {
        Copy-Item -LiteralPath $observed -Destination $fixture
        $json = ($publicProvenance | ConvertTo-Json -Depth 7).Replace([string][char]13, '') + [char]10
        [IO.File]::WriteAllText($provenance, $json, [Text.UTF8Encoding]::new($false))
    }
    else {
        if((Get-FileHash -LiteralPath $fixture).Hash.ToLowerInvariant() -cne $observedHash) { throw 'candle-period fixture differs' }
        $recorded=Get-Content -LiteralPath $provenance -Raw|ConvertFrom-Json
        foreach($property in @('revision','harnessSha256','launcherSha256','fixtureSha256','productionClassInventorySha256')) { if($recorded.$property -cne $publicProvenance[$property]) { throw "Provenance differs: $property" } }
        if(($recorded.upstreamSources|ConvertTo-Json -Depth 5 -Compress) -cne ($sources|ConvertTo-Json -Depth 5 -Compress)) { throw 'Upstream source hashes differ' }
    }
    [ordered]@{status='PASS';mode=$Mode;commands=$commands;provenance=$publicProvenance;loggerSha256=(Get-FileHash $loggerPath).Hash.ToLowerInvariant()}|ConvertTo-Json -Depth 9|Set-Content -LiteralPath (Join-Path $run 'receipt.json')
    Write-Output "$Mode PASS: $($publicProvenance.rows) candle-period observations; both timezones match"
} catch { Write-Error $_; exit 1 }
