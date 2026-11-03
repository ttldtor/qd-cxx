# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

<#
.SYNOPSIS
Capture or verify deterministic local IPF writer bytes and reader projections against pinned Java QD.
.DESCRIPTION
Uses existing production classes and JDK8. Starts one bounded Java process at a time, no network or
SDK build. Capture replaces only the IPF fixture/provenance; Verify compares without replacing them.
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
.PARAMETER NativeDirectory
Optional existing native output directory. Runs actual pinned Java reader over its exact accepted case inventory.
.PARAMETER Help
Print this help without validating inputs, creating directories or launching processes.
.PARAMETER HelpRu
Print Russian help without inputs or work.
.EXAMPLE
pwsh -File tools/upstream/ipf-writer-oracle/Invoke-Oracle.ps1 -Help
.EXAMPLE
pwsh -File tools/upstream/ipf-writer-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/ipf-check
#>
[CmdletBinding()]
param(
    [ValidateSet('Capture','Verify')][string]$Mode = 'Verify',
    [string]$Upstream,
    [string]$JavaHome,
    [string]$Logger,
    [string]$RunDirectory,
    [string]$NativeDirectory,
    [switch]$Help,
    [switch]$HelpRu
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($HelpRu) {
    Write-Output @"
Захват/проверка локального несжатого IPF writer закреплённого Java QD3.355.
Вызов: pwsh -File tools/upstream/ipf-writer-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <QD> -JavaHome <JDK8> -Logger <qdcxx-run-utc.exe> -RunDirectory <.work/каталог>
Mode: Verify по умолчанию, сравнивает fixture/provenance; Capture заменяет только IPF fixture/provenance кандидатами.
Upstream, JavaHome, Logger, RunDirectory обязательны для работы. Нужны готовые production classes, JDK8 и UTC logger.
Результат: полные логи команд, raw streams, class inventories, именованные profiles, writer bytes и reader outcomes и receipt в RunDirectory.
NativeDirectory необязателен: читает actual native writer outputs закреплённым Java reader; точный набор accepted cases.
Ограничения: один процесс одновременно, heap128MiB, timeout60 секунд; без сети, загрузок и сборки SDK.
Каталог должен находиться внутри .work текущего checkout; его собственные логи могут заменяться.
Exit0: успех/справка. Exit1: вход, reference/provenance/наблюдения, timeout или процесс завершился с ошибкой.
-Help/-HelpRu доступны без рабочих входов и не создают каталоги/не начинают Java; Get-Help показывает справку EN.
"@
    exit 0
}
if ($Help) { Get-Help -Full $PSCommandPath; exit 0 }
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
    $version = (& $java -version 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0 -or $version -notmatch 'version "1\.8\.') { throw 'Oracle requires JDK8' }
    $run = [IO.Path]::GetFullPath($RunDirectory,$repo)
    $allowed = Join-Path $repo '.work'
    if (!$run.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'RunDirectory must be inside this checkout .work' }
    New-Item -ItemType Directory -Force -Path $run,(Join-Path $run 'classes') | Out-Null
    $classpath = @('dxfeed-api','dxlib','qd-core') | ForEach-Object { (Resolve-Path -LiteralPath (Join-Path $upstreamPath "$_/target/classes")).Path }
    $sourceRelative = @('dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfile.java','dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfileField.java','dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfileType.java','dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfileReader.java','dxfeed-api/src/main/java/com/dxfeed/ipf/impl/InstrumentProfileParser.java','dxfeed-api/src/main/java/com/dxfeed/ipf/impl/Constants.java','dxlib/src/main/java/com/devexperts/io/CSVReader.java','dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfileWriter.java','dxfeed-api/src/main/java/com/dxfeed/ipf/impl/InstrumentProfileComposer.java','dxlib/src/main/java/com/devexperts/io/CSVWriter.java')
    & git -C $upstreamPath diff --quiet HEAD -- $sourceRelative
    if ($LASTEXITCODE -ne 0) { throw 'Pinned upstream IPF sources differ from HEAD' }
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
    $harness = Join-Path $PSScriptRoot 'IpfWriterOracle.java'
    $classes = Join-Path $run 'classes'
    Invoke-Logged 'javac' $javac @('-J-Xmx128m','-encoding','UTF-8','-cp',($classpath -join ';'),'-d',$classes,$harness)
    Invoke-Logged 'java-default' $java @('-Xmx128m','-Duser.timezone=UTC','-Duser.language=en','-Duser.country=US','-cp',((@($classes)+$classpath)-join ';'),'qdcxx.oracle.IpfWriterOracle',(Join-Path $run 'observed'))
    function ObservationInventory([string]$root) {
        @(Get-ChildItem -LiteralPath $root -File | Sort-Object Name | ForEach-Object { [ordered]@{path=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()} })
    }
    $observed = Join-Path $run 'observed'
    $observations = @(ObservationInventory $observed)
    $observedHash = @($observations | ForEach-Object { $_.path + ' ' + $_.sha256 }) -join "`n"
    $observedHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($observedHash))).ToLowerInvariant()
    $after = @(ClassInventory)
    $after | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'classes-after.json')
    if(($before|ConvertTo-Json -Depth 5 -Compress) -cne ($after|ConvertTo-Json -Depth 5 -Compress)) { throw 'Production classes changed during capture' }
    $caseCount = @(Get-Content -LiteralPath (Join-Path $observed 'cases.tsv')).Count
    $publicProvenance=[ordered]@{revision=$expectedRevision;javaVersion=($version -split "`n")[0];javaSha256=(Get-FileHash $java).Hash.ToLowerInvariant();javacSha256=(Get-FileHash $javac).Hash.ToLowerInvariant();harnessSha256=(Get-FileHash $harness).Hash.ToLowerInvariant();launcherSha256=(Get-FileHash $PSCommandPath).Hash.ToLowerInvariant();upstreamSources=$sources;productionClassInventorySha256=(Get-FileHash (Join-Path $run 'classes-before.json')).Hash.ToLowerInvariant();fixtureInventorySha256=$observedHash;cases=$caseCount;completionStrategy='writer complete marker; canonical field-text fidelity';observations=$observations}
    $fixture=Join-Path $repo 'tests/fixtures/ipf-writer/capture'
    $provenance=Join-Path $repo 'tests/fixtures/ipf-writer/provenance.json'
    if($Mode -eq 'Capture') {
        New-Item -ItemType Directory -Force -Path $fixture | Out-Null
        foreach($entry in $observations) { Copy-Item -LiteralPath (Join-Path $observed $entry.path) -Destination (Join-Path $fixture $entry.path) }
        $publicProvenance|ConvertTo-Json -Depth 7|Set-Content -LiteralPath $provenance
    } else {
        $fixtureInventory=@(ObservationInventory $fixture)
        if(($fixtureInventory|ConvertTo-Json -Depth 5 -Compress) -cne ($observations|ConvertTo-Json -Depth 5 -Compress)) { throw 'IPF writer fixture inventory differs' }
        $recorded=Get-Content -LiteralPath $provenance -Raw|ConvertFrom-Json
        foreach($property in @('revision','harnessSha256','launcherSha256','fixtureInventorySha256','productionClassInventorySha256')) { if($recorded.$property -cne $publicProvenance[$property]) { throw "Provenance differs: $property" } }
        if(($recorded.upstreamSources|ConvertTo-Json -Depth 5 -Compress) -cne ($sources|ConvertTo-Json -Depth 5 -Compress)) { throw 'Upstream source hashes differ' }
    }
    if($NativeDirectory) {
        $nativePath=(Resolve-Path -LiteralPath $NativeDirectory).Path
        Invoke-Logged 'java-native-reader' $java @('-Xmx128m','-Duser.timezone=UTC','-Duser.language=en','-Duser.country=US','-cp',((@($classes)+$classpath)-join ';'),'qdcxx.oracle.IpfWriterOracle','--read-native',$nativePath,(Join-Path $run 'native-read.tsv'))
    }
    [ordered]@{status='PASS';mode=$Mode;commands=$commands;provenance=$publicProvenance;loggerSha256=(Get-FileHash $loggerPath).Hash.ToLowerInvariant()}|ConvertTo-Json -Depth 9|Set-Content -LiteralPath (Join-Path $run 'receipt.json')
    Write-Output "$Mode PASS: $($publicProvenance.cases) IPF writer cases; exact inventory matches"
} catch { Write-Error $_; exit 1 }
