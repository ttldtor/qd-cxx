# Sale HISTORY lifecycle oracle

[Русская версия](README.ru.md). English is authoritative.

`SaleLifecycleOracle` runs the pinned Java QD TimeAndSale and OptionSale HISTORY delegates, distributor and QDAgents offline. For each event type it records partial and complete snapshots, a correction with `TX_PENDING`, a business cancellation, an indexed removal, a truncated snapshot, a second local subscription and a fresh collector. The independent OptionSale Index and business time are recorded separately.

Run with one path to a new output directory. The generator writes two event directories containing `oracle.tsv`, sequential `wire-*.qtp` segments and the `fresh/` stream. With no arguments or `--help` it prints English help. `--help=ru` prints Russian help. Help creates no files. Exit zero means success. Nonzero indicates a usage or capture error and can leave a partial directory.

## Reproduce

Use JDK 8 and the compiled Java QD revision in [upstream.lock](../../../upstream.lock). Run from the directory containing `SaleLifecycleOracle.java`. Set `$upstream` to the checkout, `$jdk` to the JDK directory and `$run` to a new output directory.

```powershell
$modules = 'dxlib','mars','dxfeed-promise','dxfeed-api','dxlib-qd-sandbox','auth','proto',
           'qd-core','proto-ssl','qd-rmi','dxfeed-impl','dxfeed-scheme','dxfeed-ipf-filter','qds-file'
$sep = [IO.Path]::PathSeparator
$cp = ($modules | ForEach-Object { Join-Path $upstream "$_/target/classes" }) -join $sep
$suffix = if ($IsWindows) { '.exe' } else { '' }
if (Test-Path -LiteralPath $run) { throw 'Choose a new output directory' }
New-Item -ItemType Directory -Path "$run/classes" -Force | Out-Null
& "$jdk/bin/javac$suffix" -J-Xmx128m -encoding UTF-8 -cp $cp -d "$run/classes" ./SaleLifecycleOracle.java
if ($LASTEXITCODE) { throw 'Compilation failed' }
foreach ($zone in 'UTC','Pacific/Honolulu') {
    $label = $zone.Replace('/','-')
    & "$jdk/bin/java$suffix" -Xmx128m "-Duser.timezone=$zone" -Duser.language=en -Duser.country=US `
        -cp "$run/classes$sep$cp" qdcxx.oracle.SaleLifecycleOracle "$run/$label"
    if ($LASTEXITCODE) { throw "Capture failed for $zone" }
}
```

Compare SHA-256 by relative path between both zone outputs. Within each event directory the numbered chunks form one stream. `fresh/` uses a new composer and its own record description. The Java replay and fresh collector are local. The C++ conformance test separately covers TCP reconnection and publishes only the range status qualified by its current source epoch.

The oracle reads the agent after the restart and again after SNIP. Those observations preserve the flags visible at each retrieval boundary.
