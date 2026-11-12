# TheoPrice QD oracle

[Русская версия](README.ru.md).

TheoPriceOracle uses the pinned QD revision in [upstream.lock](../../../upstream.lock).
It records the production TheoPrice descriptor and delegate values after binary QTP
round trips in TICKER and HISTORY. Each run creates a new output directory with
two QTP files and two TSV files. Exit 0 indicates success. Invalid arguments,
existing output or a capture error produce a nonzero exit. A failed run can leave
a partial directory. The harness works offline and shows help with no arguments.

Compile with JDK 8 against the production module class directories using the
[Series oracle classpath setup](../series-oracle/README.md). Replace the Java
source with `tools/upstream/theo-price-oracle/TheoPriceOracle.java` and invoke the class
`qdcxx.oracle.TheoPriceOracle` with a single new output directory. Run once with
default wide decimals and once with `-Ddxscheme.wide=false` for tiny decimals.
The QTP and TSV files match byte for byte in UTC and Pacific/Honolulu.
