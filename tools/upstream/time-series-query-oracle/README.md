# Time-series query boundary oracle

[Русская версия](README.ru.md).

`TimeSeriesQueryOracle.java` uses `TimeAndSale`, `TimeSequenceUtil` and `AbstractIndexedList` from the [pinned QD revision](../../../upstream.lock). It records raw indices, decoded business times, sequences, Java list ordering and the documented-inclusive upper-bound discrepancy in the [fixture](../../../tests/fixtures/time-series-query/provenance.json).

Compile with JDK 8 using the pinned QD production classpath in `$qdClassPath`:

```powershell
javac -encoding UTF-8 -cp $qdClassPath -d classes TimeSeriesQueryOracle.java
java -Duser.timezone=UTC -cp "classes;$qdClassPath" qdcxx.oracle.TimeSeriesQueryOracle capture.tsv
```

Provide a new output file. The tool works offline, exits 0 on success and nonzero for invalid arguments, an existing file or a write error; a failed capture may leave a partial file. No arguments or `--help` prints English help without writing files; `--help=ru` prints Russian help. Checked observations match in UTC and Pacific/Honolulu.
