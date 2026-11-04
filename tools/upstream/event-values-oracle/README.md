# Event value production oracle

[Русская версия](README.ru.md). English is authoritative.

`EventValuesOracle.java` observes the production utilities, twelve market enum tables, IndexedEvent
masks and Trade time/sequence methods from [upstream.lock](../../../upstream.lock). It calls actual
EventUtil, TimeNanosUtil, DayUtil and TimeUtil, including negative times, overflow, arbitrary Gregorian
day normalization, invalid inputs and unknown enum codes. It obtains the standard TimeAndSale record
from DXFeedScheme and observes its real ExchangeSaleConditions SHORT_STRING field parse/render methods.
No diagnostic implementation supplies the expected conversions. No endpoint is opened or SDK rebuilt.

```powershell
pwsh -File tools/upstream/event-values-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/qd-reference -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/event-values-verify
```

`Mode` defaults to Verify, comparing fixture bytes and provenance. Capture replaces only
`tests/fixtures/event-values/oracle.tsv` and `provenance.json`. All four operational paths are required:
the exact pinned checkout with existing built production classes, JDK8, the native UTC logger and a
**new** evidence directory inside this checkout's `.work`. Existing directories and redirected ancestors
are refused. Source hashes, the unchanged production-class inventory, Java version/tool hashes, source
revision, harness/launcher hashes and observation digest bind the capture. Verify does not overwrite fixtures.

Each child is sequential, uses at most a 128MiB Java heap and has a 60-second timeout. Only a timed-out
child process tree owned by the launcher is stopped. Full commands, raw streams, UTC phase bounds,
class/source inventories, observations and receipt are retained in RunDirectory. Both UTC and
Pacific/Honolulu executions must produce identical bytes. Exit0 means success/help; exit1 means invalid
inputs, unavailable/mismatched reference, process/timeout, observation/provenance mismatch or I/O failure.

No arguments, `-Help`, `-HelpRu` and `Get-Help ./Invoke-Oracle.ps1 -Full` show help without operational
inputs, directory creation, Java/SDK/network work. No-argument and English help are identical. The CTest
help regression also checks translated help, ignored invalid operational paths in help mode and incomplete
operational input rejection. Format the harness with the pinned JDK21 formatter before Capture and rerun
Verify after the final source freeze. No global Java, Git, toolchain or SDK settings are changed.

See [value contracts](../../../spec/events/EVENT-VALUES.md) and [capture layout](../../../tests/fixtures/event-values/CAPTURES.md).
Native compiler/sanitizer, installed/relocated consumers and independent review are separate checks;
this oracle does not certify future typed event mappers, calendar schedules or receiving-client completeness.
