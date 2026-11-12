# Candle HISTORY oracle

[Русская версия](README.ru.md). English is authoritative.

`CandleLifecycleOracle` records the Java QD Candle HISTORY delegate and collector with the
standard wide-decimal schema. It runs offline. The case subscribes to `AAPL{=m}` from 1000 ms,
completes the initial snapshot, then sends a correction with the same index. The second local
subscriber receives the initial snapshot from the cache. The third receives the corrected
snapshot. The added-record provider reports one physical ADD for all three subscriptions.

The capture mode accepts one path to a new output directory. It writes `oracle.tsv` and nine
binary QTP chunks. With no arguments or `--help`, it prints English help. With `--help=ru`,
it prints Russian help. Help requires no capture path and creates no files. Exit zero
indicates success. A failure exits nonzero and can leave a partial output directory.

## Reproduce the capture

Use JDK 8 and the built production classes of the Java QD revision in
[upstream.lock](../../../upstream.lock). In PowerShell, change to the directory containing
`CandleLifecycleOracle.java`. Set `$upstream` to the built QD checkout, `$jdk` to the JDK
directory, and `$run` to a new directory for compiled classes and both captures.

```powershell
$modules = 'dxlib','mars','dxfeed-promise','dxfeed-api','dxlib-qd-sandbox','auth','proto',
           'qd-core','proto-ssl','qd-rmi','dxfeed-impl','dxfeed-scheme','dxfeed-ipf-filter','qds-file'
$cp = ($modules | ForEach-Object { Join-Path $upstream "$_/target/classes" }) -join [IO.Path]::PathSeparator
$suffix = if ($IsWindows) { '.exe' } else { '' }
$javac = Join-Path $jdk "bin/javac$suffix"
$java = Join-Path $jdk "bin/java$suffix"
if (Test-Path -LiteralPath $run) { throw 'Choose a new run directory' }
New-Item -ItemType Directory -Path "$run/classes" -Force | Out-Null
& $javac '-J-Xmx128m' -encoding UTF-8 -cp $cp -d "$run/classes" ./CandleLifecycleOracle.java
if ($LASTEXITCODE) { throw 'javac failed' }
$class = 'qdcxx.oracle.CandleLifecycleOracle'
$classpath = "$run/classes$([IO.Path]::PathSeparator)$cp"
& $java '-Dfile.encoding=UTF-8' -cp $classpath $class --help
if ($LASTEXITCODE) { throw 'English help failed' }
& $java '-Dfile.encoding=UTF-8' -cp $classpath $class --help=ru
if ($LASTEXITCODE) { throw 'Russian help failed' }
foreach ($zone in 'UTC','Pacific/Honolulu') {
    $label = $zone.Replace('/','-')
    & $java '-Xmx128m' "-Duser.timezone=$zone" '-Duser.language=en' '-Duser.country=US' -cp $classpath $class "$run/$label"
    if ($LASTEXITCODE) { throw "Capture failed for $zone" }
}
```

Compare the SHA-256 of each of the ten files by name in `UTC` and `Pacific-Honolulu`.
They match byte for byte. Concatenate `source-begin.qtp`, `source-end.qtp` and
`source-correction.qtp` in order to get the source stream. The first chunk includes the
binary record description. Each agent's output chunks form a separate stream.

The TSV records the physical record name, symbol, raw time and flags alongside the delegate's
logical symbol, index, Unix milliseconds, sequence, count and raw binary64 close bits. The
oracle exercises local HISTORY delivery and replay within a single QD collector.
