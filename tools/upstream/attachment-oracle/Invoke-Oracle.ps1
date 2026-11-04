# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

<#
.SYNOPSIS
Capture or verify deterministic attachment observations against pinned Java QD.
.DESCRIPTION
Uses existing production classes and JDK8. Starts one bounded Java process at a time, no network or
SDK build. Capture creates attachment fixture candidates; Verify compares without replacing them.
Upstream, JavaHome, Logger and RunDirectory are required for Capture/Verify; Mode defaults to Verify.
RunDirectory must be new and inside this checkout's .work; prior evidence is never overwritten.
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
Fresh owned evidence directory inside this checkout's .work; reparse ancestors are refused.
.PARAMETER Help
Print this help without validating inputs, creating directories or launching processes.
.PARAMETER HelpRu
Print Russian help without inputs or work.
.EXAMPLE
pwsh -File tools/upstream/attachment-oracle/Invoke-Oracle.ps1 -Help
.EXAMPLE
pwsh -File tools/upstream/attachment-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/attachment-check
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
Захват/проверка вложений Message/Configuration закреплённого Java QD3.355.
Вызов: pwsh -File tools/upstream/attachment-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <QD> -JavaHome <JDK8> -Logger <qdcxx-run-utc.exe> -RunDirectory <.work/каталог>
Mode: Verify по умолчанию сравнивает fixture/provenance; Capture создаёт новые attachment fixture/provenance кандидаты.
Upstream, JavaHome, Logger, RunDirectory обязательны для работы. Нужны готовые production classes, JDK8 и UTC logger.
Результат: логи команд, raw streams, source/class/resource inventories, wire fixtures и receipt в новом RunDirectory.
Ограничения: один процесс одновременно, heap128MiB, timeout60 секунд; без сети, загрузок и сборки SDK.
Каталог должен быть новым внутри .work текущего checkout; перенаправляющие ancestors запрещены.
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
    if (Test-Path -LiteralPath $run) { throw 'RunDirectory must be new' }
    New-Item -ItemType Directory -Path $run,(Join-Path $run 'classes') | Out-Null
    $classpath = @('dxlib','mars','dxfeed-promise','dxfeed-api','dxlib-qd-sandbox','auth','proto','qd-core','proto-ssl','qd-rmi','dxfeed-impl','dxfeed-scheme','dxfeed-ipf-filter','qds-file') | ForEach-Object { (Resolve-Path -LiteralPath (Join-Path $upstreamPath "$_/target/classes")).Path }
    $sourceRelative = @('dxlib/src/main/java/com/devexperts/io/Marshalled.java','dxlib/src/main/java/com/devexperts/io/Marshaller.java','qd-core/src/main/java/com/devexperts/qd/kit/MarshalledObjField.java','qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java')
    foreach ($name in @('Message','Configuration','TextMessage','TextConfiguration')) {
        $sourceRelative += "dxfeed-api/src/main/java/com/dxfeed/event/misc/$name.java"
        $sourceRelative += "dxfeed-impl/src/main/java/com/dxfeed/event/misc/${name}Delegate.java"
        $sourceRelative += "dxfeed-impl/src/main/java/com/dxfeed/event/misc/impl/${name}Mapping.java"
    }
    $sourceRelative += 'dxfeed-impl/src/main/java/com/dxfeed/event/misc/MiscFactoryImpl.java'
    & git -C $upstreamPath diff --quiet HEAD -- $sourceRelative
    if ($LASTEXITCODE -ne 0) { throw 'Pinned upstream attachment sources differ from HEAD' }
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
    $harness = Join-Path $PSScriptRoot 'AttachmentOracle.java'
    $classes = Join-Path $run 'classes'
    Invoke-Logged 'java-version' $java @('-Xmx128m','-version')
    $versionOutput = [IO.File]::ReadAllText((Join-Path $run 'java-version.stderr')) + [IO.File]::ReadAllText((Join-Path $run 'java-version.stdout'))
    $versionLines = [regex]::Matches($versionOutput, '(?m)^(?:\d{4}-\d{2}-\d{2}T[0-9:.]+Z )?((?:openjdk|java) version "1\.8\.[^\r\n]+)\r?$')
    if ($versionLines.Count -ne 1) { throw 'Oracle requires one actual JDK8 version line' }
    $version = $versionLines[0].Groups[1].Value.TrimEnd([char]13)
    Invoke-Logged 'javac' $javac @('-J-Xmx128m','-encoding','UTF-8','-cp',($classpath -join ';'),'-d',$classes,$harness)
    foreach ($zone in @('UTC','Pacific/Honolulu')) {
        $zoneName = $zone.Replace('/','-')
        Invoke-Logged "java-$zoneName" $java @('-Xmx128m',"-Duser.timezone=$zone",'-Duser.language=en','-Duser.country=US','-cp',((@($classes)+$classpath)-join ';'),'com.dxfeed.event.misc.AttachmentOracle',(Join-Path $run $zoneName))
    }
    $observed = Join-Path $run 'UTC/oracle.tsv'
    $observedHash=(Get-FileHash -LiteralPath $observed).Hash.ToLowerInvariant()
    $artifacts = @(Get-ChildItem -LiteralPath (Join-Path $run 'UTC') -File | Sort-Object Name | ForEach-Object {
        $hash = (Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()
        if ($hash -cne (Get-FileHash -LiteralPath (Join-Path $run "Pacific-Honolulu/$($_.Name)")).Hash.ToLowerInvariant()) { throw 'Timezone changed attachment observation' }
        [ordered]@{path=$_.Name;size=$_.Length;sha256=$hash}
    })
    $after = @(ClassInventory)
    $after | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'classes-after.json')
    if(($before|ConvertTo-Json -Depth 5 -Compress) -cne ($after|ConvertTo-Json -Depth 5 -Compress)) { throw 'Production classes changed during capture' }
    $sourcesAfter = @($sourceRelative | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $upstreamPath $_)).Hash.ToLowerInvariant()} })
    $sourcesAfter | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'sources-after.json')
    $sources | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'sources-before.json')
    if (($sources | ConvertTo-Json -Depth 5 -Compress) -cne ($sourcesAfter | ConvertTo-Json -Depth 5 -Compress)) { throw 'Production sources changed during capture' }
    $classCount = @($before | Where-Object { $_.path.EndsWith('.class') }).Count
    $publicProvenance=[ordered]@{revision=$expectedRevision;javaVersion=($version -split "`n")[0];javaSha256=(Get-FileHash $java).Hash.ToLowerInvariant();javacSha256=(Get-FileHash $javac).Hash.ToLowerInvariant();harnessSha256=(Get-FileHash $harness).Hash.ToLowerInvariant();launcherSha256=(Get-FileHash $PSCommandPath).Hash.ToLowerInvariant();upstreamSources=$sources;productionClassInventorySha256=(Get-FileHash (Join-Path $run 'classes-before.json')).Hash.ToLowerInvariant();classpathRoots=$classpath.Count;classes=$classCount;resources=$before.Count-$classCount;artifacts=$artifacts;fixtureSha256=$observedHash;rows=@(Get-Content -LiteralPath $observed).Count;timezoneInvariant=$true}
    $fixture=Join-Path $repo 'tests/fixtures/attachments/capture-v1'
    $provenance=Join-Path $repo 'tests/fixtures/attachments/provenance.json'
    if($Mode -eq 'Capture') {
        if (Test-Path -LiteralPath $fixture) { throw 'Capture fixture directory already exists; preserve old evidence' }
        New-Item -ItemType Directory -Path $fixture | Out-Null
        Copy-Item -Path (Join-Path $run 'UTC/*') -Destination $fixture
        $json = ($publicProvenance | ConvertTo-Json -Depth 7).Replace([string][char]13, '') + [char]10
        [IO.File]::WriteAllText($provenance, $json, [Text.UTF8Encoding]::new($false))
    }
    else {
        foreach ($artifact in $artifacts) {
            if ((Get-FileHash -LiteralPath (Join-Path $fixture $artifact.path)).Hash.ToLowerInvariant() -cne $artifact.sha256) { throw "Attachment fixture differs: $($artifact.path)" }
        }
        if (@(Get-ChildItem -LiteralPath $fixture -File).Count -ne $artifacts.Count) { throw 'Attachment artifact count differs' }
        $recorded=Get-Content -LiteralPath $provenance -Raw|ConvertFrom-Json
        foreach($property in @('revision','javaVersion','javaSha256','javacSha256','harnessSha256','launcherSha256','fixtureSha256','productionClassInventorySha256','classpathRoots','classes','resources')) { if($recorded.$property -cne $publicProvenance[$property]) { throw "Provenance differs: $property" } }
        if (($recorded.artifacts | ConvertTo-Json -Depth 5 -Compress) -cne ($artifacts | ConvertTo-Json -Depth 5 -Compress)) { throw 'Artifact provenance differs' }
        if(($recorded.upstreamSources|ConvertTo-Json -Depth 5 -Compress) -cne ($sources|ConvertTo-Json -Depth 5 -Compress)) { throw 'Upstream source hashes differ' }
    }
    [ordered]@{status='PASS';mode=$Mode;commands=$commands;provenance=$publicProvenance;loggerSha256=(Get-FileHash $loggerPath).Hash.ToLowerInvariant()}|ConvertTo-Json -Depth 9|Set-Content -LiteralPath (Join-Path $run 'receipt.json')
    Write-Output "$Mode PASS: $($publicProvenance.rows) attachment observations; both timezones match"
} catch { Write-Error $_; exit 1 }
