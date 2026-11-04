# Candle-symbol observations

[Русская версия](README.ru.md). English is authoritative.

The Windows PowerShell7 launcher uses existing pinned QD3.355 production classes and JDK8.
No arguments, -Help, -HelpRu and Get-Help print help without paths, file creation, Java or SDK work.
Java formatting uses a separate JDK21+.

```powershell
pwsh -File tools/upstream/candle-symbol-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/candle-symbol-check
```

Verify is the default. All four paths are required for work. RunDirectory must be inside this
checkout's .work; owned observations/logs may be replaced. Each Java subprocess runs serially with
heap128MiB and timeout60s, including version/javac; only its owned timed-out tree is stopped.
UTC/Pacific-Honolulu bytes must match and production class inventories must remain unchanged.
Exit0 means success/help; exit1 means arguments, provenance/observation mismatch, timeout or failure.
Capture replaces only [symbol-oracle.tsv](../../../tests/fixtures/events/candle/symbol-oracle.tsv)
and [symbol-provenance.json](../../../tests/fixtures/events/candle/symbol-provenance.json) with
review candidates. Verify compares without replacing public fixtures. Full logs, raw streams,
source/class inventories and a receipt remain in RunDirectory.

The sixteen TSV columns are operation, input UTF-16 hex, attribute descriptors or right-symbol hex,
status, canonical UTF-16 hex or equals result, base hex, exchange hex, price ordinal, session ordinal,
raw period bits, period-type ordinal, alignment ordinal, raw price-level bits, data-type hex,
signed32 hash or ERROR, and exception class. Unused columns contain a dash. NULL denotes null;
four lowercase digits encode each exact UTF-16 unit. equals output ERROR is its null-domain failure.

Attribute descriptors are semicolon-separated: e + exchange hex; p/s/a + enum ordinal;
l + raw double hex; t + raw double hex / type ordinal; d + exact UTF-16 hex (empty means undefined).
They construct real production values before calling the real CandleSymbol factory; no replacement
normalizer or parser is used. Null attribute references are not represented by the C++ closed variant.

The 9822 rows include combined attributes, explicit getter differences, duplicate/unknown/invalid
keys, null/empty/malformed text, NaN payload/nonfinite/signed-zero periods, equality pairs,4096
deterministic key/value combinations and every seventeenth UTF-16 unit embedded in base symbols.
The native test checks all rows and separately checks the deliberate total C++ operator== null
adaptation. See [the complete contract](../../../spec/events/CANDLE-SYMBOL.md).
