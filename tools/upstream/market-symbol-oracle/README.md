# Market-symbol observations

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Oracle.ps1 -Help` or `-HelpRu` prints built-in help without inputs or work.
The PowerShell 7 launcher uses existing pinned QD 3.355 production classes and JDK8;
it performs no SDK build or download. Use the separate JDK21 for Java formatting.

```powershell
pwsh -File tools/upstream/market-symbol-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/market-symbol-check
```

`Mode` defaults to Verify. All four paths are required for work. `RunDirectory`
must be within this checkout's `.work`; its owned logs can be replaced. One bounded
Java process runs at a time, heap 128MiB, timeout 60 seconds. The launcher stops
only its own timed-out process tree. UTC and Pacific/Honolulu outputs must match;
the production class inventory must remain unchanged. Commands, raw streams,
inventories and receipt stay in `RunDirectory`.

Capture replaces only [oracle.tsv](../../../tests/fixtures/market-symbols/oracle.tsv)
and [provenance.json](../../../tests/fixtures/market-symbols/provenance.json) with
candidates; review them together. Verify compares regenerated observations and
source/harness/launcher hashes without replacing fixtures. Exit 0 is success/help;
exit 1 means invalid inputs, provenance/observation mismatch, timeout or process failure.
[Coverage](../../../tests/fixtures/market-symbols/README.md) describes exact scope;
[contract](../../../spec/events/MARKET-SYMBOLS.md) defines C++ behavior.
