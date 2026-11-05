# Production time-series identity oracle

[Русская версия](README.ru.md).

`Invoke-Oracle.ps1` captures or verifies 123 observations from the pinned Java8 production
`TimeSeriesSubscriptionSymbol`, `DXFeedTimeSeriesSubscription` and event marker classes.
It covers exact UTF-16 and signed time bounds, range-independent hash/equality, nested wrappers,
set/range transitions, the disabled MAX default, raw-generic bypass and inherited/custom markers.
Raw asymmetric equality remains intact; [ADR-0012](../../../docs/decisions/ADR-0012-decorated-subscription-identity.md)
defines the native typed correction. Observing a Java close property does not give the native pure model a lifecycle.

No arguments, `-Help`, `-HelpRu` and `Get-Help ./Invoke-Oracle.ps1 -Full` need no paths and start no Java.

```powershell
pwsh -NoProfile -File tools/upstream/time-series-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/time-series-check-01
```

Verify is default and compares the fixtures. Capture replaces only `tests/fixtures/time-series/oracle.tsv`
and `provenance.json`. Four operational paths are mandatory. The output directory must be fresh,
contained in this checkout's `.work` and free of redirecting ancestors. Existing evidence is refused.
The tool uses existing pinned production classes without building or writing upstream. Complete class/resource
inventories before and after, source/executable/harness hashes and UTC command/exit logs bind each run.
Every subprocess has a 60s deadline; javac/Java use a 128MiB heap and run sequentially. Only owned timed-out
children are stopped. UTC and Pacific/Honolulu output must match byte for byte. Exit0 means success/help;
Exit1 means invalid inputs, reference mismatch, process/timeout failure or changed observations.
Full diagnostics stay in RunDirectory. No network or SDK provisioning is performed.

The [native contract](../../../spec/subscriptions/TIME-SERIES-IDENTITY.md) distinguishes logical capability
from Feed HISTORY availability and preserves the generic wrapper's caller-supplied equality/hash laws.
