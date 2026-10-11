# Production outgoing tape witnesses

[Русская версия](README.ru.md). English is authoritative.

This finite additive corpus observes the unchanged locked QD through `FileWriter` followed by
`FileReader`. It adds outgoing property names and IDs, physical versus wire layout, exact raw values,
nullable type-10 `String`/`char[]`, symbol representations, and explicit timing/mode witnesses.
The unchanged [event oracle](../event-mapping-oracle/EventMappingOracle.java) supplies its `default`
Quote/Profile delegate witnesses. Old sources, captures and expectations remain unchanged.

Use JDK 8 for the oracle, JDK 21+ for formatting, CMake 3.25+ and PowerShell 7.4+. Supply an explicit
clean checkout matching [upstream.lock](../../../upstream.lock), Maven executable and dependency cache.
The preparation command performs a clean locked reactor build; each capture then verifies the
prepared classpath receipt against actual provider bytes before and after execution. Every run
directory must be new. No endpoint, private tape or network transport is used by capture.

```powershell
$upstream = 'C:/path/to/locked-QD'
$java8 = 'C:/path/to/jdk8'
$formatter = 'C:/path/to/jdk21-or-newer'
$maven = 'C:/path/to/mvn.cmd'
$cache = 'C:/path/to/explicit-maven-cache'
$prepare = 'C:/path/to/new-preparation'
$a = 'C:/path/to/new-capture-a'
$b = 'C:/path/to/new-capture-b'
$verify = 'C:/path/to/new-verify'
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome $formatter
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome $formatter
cmake "-DJAVA_HOME=$java8" "-DUPSTREAM=$upstream" "-DMAVEN=$maven" "-DCACHE=$cache" "-DRUN=$prepare" -P tools/schema-export/Prepare.cmake
cmake "-DJAVA_HOME=$java8" "-DUPSTREAM=$upstream" "-DRECEIPT=$prepare/classpath-receipt.json" "-DRUN=$a" -P tools/upstream/tape-write-oracle/Invoke-Oracle.cmake
cmake "-DJAVA_HOME=$java8" "-DUPSTREAM=$upstream" "-DRECEIPT=$prepare/classpath-receipt.json" "-DRUN=$b" -P tools/upstream/tape-write-oracle/Invoke-Oracle.cmake
pwsh -NoProfile -File tools/upstream/tape-write-oracle/Compare-TapeWrite.ps1 -Actual "$a/artifacts" -Expected "$b/artifacts"
cmake -DMODE=Verify "-DJAVA_HOME=$java8" "-DUPSTREAM=$upstream" "-DRECEIPT=$prepare/classpath-receipt.json" "-DRUN=$verify" -P tools/upstream/tape-write-oracle/Invoke-Oracle.cmake
pwsh -NoProfile -File tools/upstream/tape-write-oracle/Test-Negatives.ps1 -RunDirectory C:/path/to/new-negatives -Expected tests/fixtures/protocol/tape-write/expected
```

Capture validates its structure; the second capture establishes deterministic bytes. Verify compares
fresh production output to the retained 43-artifact corpus and never writes expectations. The
comparator enforces the exact file/directory set, rejects symbolic paths, bounds artifact sizes,
checks typed numeric/UTF-16 rows, ordered raw values and symbols, zero flags, production counts,
completion and current tool/input provenance. It compares every new artifact byte for byte.

For the historical `default/observations.tsv` only, it compares complete lines because the existing
checkout's automatic text conversion uses CRLF while fresh Java output uses LF. The six historical
`.qtp` files remain byte comparisons. This exception does not normalize the new corpus.

Each Java execution has a 30-second limit and 256 MiB heap; compilation/comparison also have finite
timeouts. Negative comparison children have 30-second limits, close their process handles and kill
only their own timed-out process trees. File readers close in `finally`; writer resources close
before replay; callback cursors are inspected immediately and are never retained.

`extras/*-wire.tsv` are diagnostic envelope/descriptor scans, not parser acceptance. The old event
oracle's `*-wire-omitted.qtp` are constructed diagnostic omissions. Production acceptance comes from
FileReader callbacks and the explicit zero-error/count checks. Java acceptance of full identities
24/137, FIELD/NONE timing or STREAM output does not admit these to a future RAW/MESSAGE writer.
Java close may log I/O failures; this harness does not establish checked publication or durability.

See [captures](../../../tests/fixtures/protocol/tape-write/CAPTURES.md),
[coverage](../../../tests/fixtures/protocol/tape-write/COVERAGE.md) and
[validation](../../../tests/conformance/protocol/tape-write/VALIDATION.md).
