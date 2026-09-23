# Configured event production oracle

[Русская версия](README.ru.md). English is authoritative.

The oracle measures locked Java production Quote/Profile delegates and FileWriter/FileReader in
21 fresh JVM scenarios. See [coverage](../../../tests/fixtures/events/mapping/COVERAGE.md),
[captures](../../../tests/fixtures/events/mapping/CAPTURES.md), and
[validation](../../../tests/fixtures/events/mapping/VALIDATION.md).
The harness package permits observing package-private Profile flags and packed Quote sequence;
it does not replace upstream methods. No private tape is used.

Each invocation requires a new run directory, JDK8, Maven, and an existing baseline inspector.
Run invocations sequentially against one upstream checkout: clean rebuilds replace its classes.
It first runs the unchanged tape oracle Verify and clean upstream reactor build, retaining locked
source/classpath/provider/dependency provenance. Each new scenario has a 256 MiB heap, a 30-second
process timeout, and a 64-callback-record limit. Timeouts fail; they never count as EOF evidence.
Java formatting uses the separately pinned formatter on JDK21+.

```powershell
$args025 = @{
  Upstream = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/path/to/profile/.m2/wrapper/dists/apache-maven-3.9.16/0daed3be3ebd1c706f0e69e8b07c6b73f5cc4ea3dfce72a8d0ec2e849ca2ddb0/bin/mvn.cmd'
  Inspector = 'C:/qd-cxx/.work/naming-final-msvc/Release/qdcxx-qtp-inspect.exe'
}
& tools/upstream/event-mapping-oracle/Invoke-Oracle.ps1 @args025 -RunDirectory .work/events-a
& tools/upstream/event-mapping-oracle/Invoke-Oracle.ps1 @args025 -RunDirectory .work/events-b
& tools/upstream/event-mapping-oracle/Compare-Events.ps1 -Actual .work/events-a/events -Expected .work/events-b/events
& tools/upstream/event-mapping-oracle/Test-Negatives.ps1 -RunDirectory .work/events-negatives
& tools/upstream/event-mapping-oracle/Invoke-Oracle.ps1 @args025 -Mode Verify -RunDirectory .work/events-verify
```

`Capture` creates observations and checks row shape and put/read event consistency. It does not
rewrite repository expectations. `Verify` additionally compares every artifact byte with the explicit
expected directory. The comparator validates the exact scenario/file inventory, known row arities,
exactly one final completion marker and three put/read events per event/mode. It compares all mapped
properties between actual delegate output and FileReader output, accounting for separate eventTime.
The final exact byte comparison rejects unknown, missing, duplicate and trailing content.

Each scenario has observations.tsv and, except rejected/xml-unsupported, four original production
files plus two separately constructed wire-omission files. Schema rows retain Java slot, full type,
local name and property name (UTF-16 hex). Use phase `variant` for reorder/minimal/numeric-int,
otherwise `initial`; type0 rows are physical tails. Mutation also records singleton-after and
endpoint-after snapshots, but its tapes use initial. Event rows use binary64 hex and exact UTF-16,
with null and empty (`-`) distinct. The `direct-sequence-boundary` phase is direct mapping only;
wire-omitted files use production parsing of separately constructed empty descriptions.

Quote columns: row, phase, index, symbol, bidTime, bidExchange, bidPriceBits, bidSizeBits,
askTime, askExchange, askPriceBits, askSizeBits, time, sequence, timeNanoPart, eventTime, packedSequence.
Profile columns: row, phase, index, symbol, betaBits, earningsPerShareBits, dividendFrequencyBits,
exDividendAmountBits, exDividendDayId, high52WeekPriceBits, low52WeekPriceBits, sharesBits,
freeFloatBits, highLimitPriceBits, lowLimitPriceBits, haltStartTime, haltEndTime, rawFlags,
Description, StatusReason, tradingStatusCode, shortSaleRestrictionCode, eventTime.

