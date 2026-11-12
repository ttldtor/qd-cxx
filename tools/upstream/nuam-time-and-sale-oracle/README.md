# NuamTimeAndSale QD fixture generator

[Русская версия](README.ru.md).

`NuamTimeAndSaleOracle.java` uses the production NuamTimeAndSale delegates from the
[pinned QD revision](../../../upstream.lock). It writes STREAM and HISTORY
QTP and observed business values for composite and regional records. The
[fixtures](../../../tests/fixtures/nuam-time-and-sale/provenance.json) record raw and
interpreted trade flags, indexed flags, time, index, decimals and optional text.

Compile with JDK 8. Put the compiled classes of the pinned QD modules on
`$qdClassPath` in this PowerShell example. Run from this generator's directory.

```powershell
javac -encoding UTF-8 -cp $qdClassPath -d classes NuamTimeAndSaleOracle.java
$javaArgs = @('-Duser.timezone=UTC', '-Duser.language=en', '-Duser.country=US',
    '-Ddxscheme.exchanges.NuamTimeAndSale=Q', '-cp', "classes;$qdClassPath",
    'qdcxx.oracle.NuamTimeAndSaleOracle', 'capture-wide')
java @javaArgs
```

Use a fresh output directory for each run. The default decimal profile is
wide. Put `-Ddxscheme.wide=false` before `-cp` for tiny decimal. To capture the
optional fields, add the following JVM arguments before `-cp`.

- `-Ddxscheme.enabled.TimeNanoPart=NuamTimeAndSale`
- `-Ddxscheme.enabled.Buyer=NuamTimeAndSale`
- `-Ddxscheme.enabled.Seller=NuamTimeAndSale`

The checked-in composite files use the default wide profile; the regional files enable the three optional fields above. Both profiles are byte-identical
between `UTC` and `Pacific/Honolulu`. A successful run writes four QTP/TSV pairs.
Use no arguments or `--help` for English help, or `--help=ru` for Russian.
Help starts no capture. Success exits 0. Invalid arguments, an existing output
directory or capture errors exit nonzero. A failed capture can leave a partial
output directory.
