# Candle-period observations

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Oracle.ps1` without arguments, `-Help`, `-HelpRu` and `Get-Help` print help
without operational paths, file creation or Java work. The PowerShell 7 launcher
uses existing pinned QD 3.355 production CandleType/CandlePeriod classes and JDK8;
it performs no SDK build or download. Java formatting uses a separate JDK21+.

```powershell
pwsh -File tools/upstream/candle-period-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/candle-period-check
```

`Mode` defaults to Verify. All four paths are required for work. `RunDirectory`
must be inside this checkout's `.work`; its owned logs can be replaced. One bounded
Java process runs at a time, heap 128MiB, timeout 60 seconds, including the version
check and javac. Only its own timed-out process tree is stopped. UTC and
Pacific/Honolulu output must match and production classes must remain unchanged.
The run directory retains command/exit logs, raw streams, class inventories and a receipt.

Capture replaces only [period-oracle.tsv](../../../tests/fixtures/events/candle/period-oracle.tsv)
and [period-provenance.json](../../../tests/fixtures/events/candle/period-provenance.json)
with candidates for review. Verify compares regenerated observations and exact
source/harness/launcher provenance without replacing fixtures. Exit0 is success/help;
exit1 means invalid input, provenance/observation mismatch, timeout or process failure.

The eight TSV columns are operation, UTF-16 input hex, construction type ordinal,
construction raw binary64 bits, canonical UTF-16 output hex or ERROR, result type
ordinal, result raw binary64 bits, and estimated interval. Unused fields contain
an empty input, -1, zero or a dash according to the operation. Hex text uses four
lowercase digits per exact UTF-16 code unit; output preserves the production Java UTF-8 TSV bytes.

Coverage includes every type, enum prefix and plural case, all single UTF-16 code
units, minute/month and price-name ambiguity, invalid numeric/scanner forms,
subnormals, signed zero, nonfinite values, narrowing saturation, 2^63 rendering,
notation boundaries and 512 deterministic raw double patterns per type. The harness
also checks NaN/signed-zero equality directly. These are value observations;
CandleSymbol, aggregation and receiving Candle events are not exercised.
See the [value contract](../../../spec/events/CANDLE-PERIOD.md).
