# MarketMaker and derived Order production oracle

[Русская версия](README.ru.md). English is authoritative.

The launcher captures actual MarketFactory delegates, original logical/full types and integer slots,
MarketMaker/derived Order results, and BinaryQTPComposer/BinaryQTPParser bytes at the [locked revision](../../../upstream.lock).
It uses existing production classes without rebuilding QD or opening a feed.

```powershell
pwsh -NoProfile -File tools/upstream/market-maker-order-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/market-maker-order-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/market-maker-check
```

`Mode` defaults to Verify. Capture replaces only the dedicated TSV, QTP and provenance candidates
under `tests/fixtures/market-maker-order`; Verify compares without changing them. Both require an
existing pinned checkout, its14 built production class/resource roots, JDK8, a UTC logger and a new
run directory inside this checkout's `.work`. Logs, raw streams, command exits and before/after class
inventories stay there. Existing evidence is refused. Each child has a128MiB Java heap bound and a
60-second timeout; only that launcher's own timed-out child tree is stopped. No network, SDK build,
global configuration or shared class write occurs. Exit0 means success/help; exit1 means argument,
reference/provenance, comparison, process, timeout or output failure. Partial diagnostics remain on failure.

No arguments and `-Help` show identical English help. `-HelpRu` shows Russian help; `Get-Help -Full`
reads the standard comment help. These paths need no operational inputs and start no Java/file work.

Separate and unitary default source modes run in fresh JVMs. A third profile enables the real
`dxscheme.nanoTime` fields. Each runs in UTC and Pacific/Honolulu; observations and binary bytes must
match across timezones. Actual ordinary delegate contract/SUB/PUB/WILDCARD/time-series/source facts
are emitted separately. The corpus covers Quote, Quote&Q and MarketMaker, complete numeric/raw type
alternatives, optional absence, missing required fields, all32 flags and five full64 removal words.
The composer explicitly enables `hs`, preserving snapshot flags. Native tests compare both original/
parsed delegate results and feed the actual generated QTP through the production C++ parser/mappers.
NaNs compare by classification; other doubles compare exact bits. A nonindexed cursor reports getTime0
and its REMOVE compact-long is zero, independently of its ordinary first data fields.

See the [public receiving contract](../../../docs/MARKET-MAKER-ORDER.md). Public provenance contains
stable source/class/harness/launcher/fixture hashes without local evidence paths.