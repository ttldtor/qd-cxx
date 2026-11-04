# Candle-attribute observations

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Oracle.ps1` without arguments, `-Help`, `-HelpRu` and `Get-Help` print help
without operational paths, file creation or Java work. PowerShell7 uses the existing
pinned QD3.355 production CandlePrice/Session/Alignment/PriceLevel/DataType/Exchange
classes and JDK8. No SDK build or download runs; formatting uses a separate JDK21+.

```powershell
pwsh -File tools/upstream/candle-attributes-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/candle-attributes-check
```

Verify is the default. All four paths are required for work; RunDirectory must be
inside this checkout's .work, where its own logs may be replaced. One process runs at a
time, heap128MiB, timeout60 seconds including version/javac. Only the owned timed-out
process tree is stopped. UTC and Pacific/Honolulu bytes must match; production classes
must remain unchanged. Logs, raw streams, class inventories and receipt stay in RunDirectory.
Exit0 means success/help; exit1 means input, provenance/observation, timeout or process failure.

Capture replaces only [attributes-oracle.tsv](../../../tests/fixtures/events/candle/attributes-oracle.tsv)
and [attributes-provenance.json](../../../tests/fixtures/events/candle/attributes-provenance.json)
with review candidates. Verify compares regenerated observations and exact provenance.

The five TSV columns are kind, operation, UTF-16 input hex, UTF-16 operand hex, result.
NULL represents null; four lowercase hex digits encode each exact code unit. Results are
UTF-16 hex or NULL, ERROR for reference argument/range exceptions, or hexadecimal numeric
bits/code followed by a colon and UTF-16 text for level/exchange values. Raw numeric input
is itself hex-encoded ASCII. The fixture contains 265722 rows: all single UTF-16 units for
three parsers/exchange values, null/malformed symbols, defaults, aliases, numeric syntax,
rejected negative zero/infinities, edge bits and 2048 deterministic random binary64 patterns.
These value observations do not exercise Candle receiving, schedules or aggregation.
See [the value contract](../../../spec/events/CANDLE-ATTRIBUTES.md).
