# Candle route oracle

[Русская версия](README.ru.md).

`CandleRoutesOracle` runs the production Java Candle delegate set from
[upstream.lock](../../../upstream.lock) for five physical record catalogs.
It records forward and reverse symbols, schema fields and refused descriptors.

Compile with JDK 8 and the production modules from the
[Series classpath setup](../series-oracle/README.md). Run
`qdcxx.oracle.CandleRoutesOracle <new-directory>` in UTC and Pacific/Honolulu.
The offline run creates the directory and writes `routes.tsv`. Compare the
files byte for byte. No arguments, `--help` and `--help=ru` print help.
Success exits zero. An invalid invocation or capture failure exits nonzero
and can leave a partial output directory.
