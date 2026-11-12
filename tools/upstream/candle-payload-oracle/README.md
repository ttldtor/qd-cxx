# Candle payload oracle

[Русская версия](README.ru.md).

`CandleOracle` records production HISTORY values from the pinned QD revision in
[upstream.lock](../../../upstream.lock). It uses the real Candle and TradeHistory
delegates and the QTP composer/parser.

Compile with JDK 8 against the production class directories from the
[Series oracle classpath setup](../series-oracle/README.md), using
`tools/upstream/candle-payload-oracle/CandleOracle.java` as the source.
Run `qdcxx.oracle.CandleOracle <new-directory> <profile>`. The profiles are
`standard`, `tiny`, `optional`, `long`, `numeric-long`, `trade` and `period`.
Use `-Ddxscheme.wide=false` for `tiny`. Run in UTC and Pacific/Honolulu to compare
QTP and TSV bytes. Each invocation creates a fresh output directory.

The harness runs offline. Successful capture or help exits zero. Invalid input,
an existing destination, or a capture failure exits nonzero. A failed run can
leave partial output. Run without arguments or with `--help` or `--help=ru` to
see the available profiles.
