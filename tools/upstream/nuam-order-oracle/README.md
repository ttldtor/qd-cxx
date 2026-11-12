# NuamOrder QD fixture generator

[Русская версия](README.ru.md).

`NuamOrderOracle.java` captures HISTORY QTP and Java delegate observations from the [pinned QD revision](../../../upstream.lock) for the default, `NUAM` and `nuam` sources. The checked-in [fixtures](../../../tests/fixtures/nuam-order/provenance.json) cover source/index identity, nullable and empty strings, decimal values and reserved flag codes.

Compile with JDK 8 and the compiled pinned QD modules in `$qdClassPath`:

```powershell
javac -encoding UTF-8 -cp $qdClassPath -d classes NuamOrderOracle.java
java -Duser.timezone=UTC -cp "classes;$qdClassPath" qdcxx.oracle.NuamOrderOracle capture
```

Use a new output directory. No arguments or `--help` print English help without writing files; `--help=ru` prints Russian help. Capture is offline and writes three QTP/TSV pairs. Exit 0 means success; invalid arguments, an existing output directory or capture failure give a nonzero exit. A failed capture can leave partial output. The checked-in files match between UTC and Pacific/Honolulu.
