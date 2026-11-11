# Series oracle

[Русская версия](README.ru.md). English is authoritative.

`SeriesOracle` captures the pinned Java QD `Series` record and the public values
produced by `SeriesDelegate` after a production binary composer/parser round trip.
[Provenance](../../../tests/fixtures/series/provenance.json) records the revision
and reference source, harness and fixture hashes.

Use production classes from [upstream.lock](../../../upstream.lock) and JDK 8.
From this checkout in PowerShell, set `$upstream` to the reference build root,
`$jdk` to the JDK root and `$run` to a fresh directory under this checkout's `.work`.

```powershell
$modules = 'dxlib','mars','dxfeed-promise','dxfeed-api','dxlib-qd-sandbox','auth','proto',
           'qd-core','proto-ssl','qd-rmi','dxfeed-impl','dxfeed-scheme','dxfeed-ipf-filter','qds-file'
$cp = ($modules | ForEach-Object { Join-Path $upstream "$_/target/classes" }) -join ';'
New-Item -ItemType Directory -Path "$run/classes" -Force
& "$jdk/bin/javac.exe" -encoding UTF-8 -cp $cp -d "$run/classes" tools/upstream/series-oracle/SeriesOracle.java
if ($LASTEXITCODE) { throw 'javac' }

foreach ($profile in 'standard','tiny','optional','field') {
    foreach ($zone in 'UTC','Pacific/Honolulu') {
        $options = if ($profile -eq 'tiny') { @('-Ddxscheme.wide=false') } else { @() }
        $name = $zone.Replace('/','-')
        $settings = @("-Duser.timezone=$zone", '-Duser.language=en', '-Duser.country=US') + $options
        & "$jdk/bin/java.exe" @settings -cp "$run/classes;$cp" qdcxx.oracle.SeriesOracle "$run/$profile-$name" $profile
        if ($LASTEXITCODE) { throw "$profile / $zone" }
    }
}
```

On Linux, use the Java binaries without `.exe` and `:` as the classpath separator.
Only the `tiny` profile uses `-Ddxscheme.wide=false`.

The harness prints help without arguments or with `--help` or `--help=ru`. A capture
creates one new directory with `series.qtp`, `oracle.tsv` and `text.tsv`.
The optional case also writes `optional.tsv`. Success exits zero. Invalid
arguments, an existing output directory and capture errors exit nonzero.
The harness uses no network. A failed capture can leave a partial directory.

- `standard` uses the production wide-decimal schema.
- `tiny` uses the production decimal schema.
- `optional` uses a configured schema and observes missing-field results.
- `field` uses the production schema with explicit FIELD event time.

The non-text files match byte-for-byte between time zones. Compare each
`field` text file with the fixture for that zone.
