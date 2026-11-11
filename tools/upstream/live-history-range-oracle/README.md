# Attached live HISTORY range oracle

[Русская версия](README.ru.md).

`Invoke-Oracle.ps1` observes a real pinned Java8 LOCAL_HUB endpoint, its public
`DXFeedTimeSeriesSubscription<TimeAndSale>`, actual QDHistory distributor demand and public event
callbacks. A bounded controllable executor holds old delivery while fromTime changes. The 57 TSV
rows cover initial MAX, shrink/expansion, disabled data, rapid changes, empty-set property mutation,
two overlapping ranges, symbol removal and retained properties after close. Bound-only demand is
ADD with the latest raw bound, without REMOVE; MAX remains a physical demand sentinel. UTC and
Pacific/Honolulu observations must match byte for byte.

These are attached Java production observations, not a native public TimeAndSale mapper, Java TCP
recovery test or peer acknowledgement. Native receiving uses the common HISTORY path and the
separately captured [standard record wire fixtures](../history-receiver-oracle/README.md).
See the [live range contract](../../../spec/subscriptions/LIVE-HISTORY-RANGES.md).

```powershell
pwsh -NoProfile -File tools/upstream/live-history-range-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/live-history-range-check-01
```

Verify is default. No arguments, `-Help`, `-HelpRu` and `Get-Help ./Invoke-Oracle.ps1 -Full` need
no input paths and start no workload. All four operational paths are mandatory. RunDirectory must
be fresh inside this checkout's `.work`, without redirecting ancestors; prior evidence is refused.
Capture replaces only `tests/fixtures/live-history-range/oracle.tsv` and `provenance.json`.
Existing pinned production classes/resources are inventoried before and after. Source, JDK,
harness, launcher and fixture hashes bind the observations. Each sequential subprocess has a
60-second deadline; javac/Java have a 128MiB heap. Only owned timed-out children are stopped.
Full UTC command/exit logs, raw streams, inventories and receipt remain in the run directory.
No upstream build/write, network or SDK provisioning occurs. Exit0 means success/help; Exit1
means invalid inputs, process/timeout failure or provenance/observation mismatch.

Primary sources at the pinned revision:
[DXFeedTimeSeriesSubscription](https://github.com/dxFeed/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeedTimeSeriesSubscription.java),
[DXFeedImpl](https://github.com/dxFeed/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXFeedImpl.java),
[History](https://github.com/dxFeed/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/impl/matrix/History.java).
