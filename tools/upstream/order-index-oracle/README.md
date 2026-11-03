# Order source/index oracle

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Oracle.ps1` runs actual pinned Java QD mapping, delegate and Order value
methods. It does not rewrite their algorithms, access a public data source or
build/download an SDK. Existing built production classes must match
[upstream.lock](../../../upstream.lock). The formatter JDK is separate from JDK8
used for these observations.

`-Help`, no arguments and `-HelpRu` need no working inputs and create no files.
For Capture or Verify, supply `-Upstream`, `-JavaHome`, `-Logger` and
`-RunDirectory`; `-Mode` defaults to Verify. RunDirectory must be a new directory
inside this checkout's `.work`. Capture replaces only this component's fixture
and provenance. Verify compares them without replacement. Both retain full UTC
logs, per-command outcomes and before/after production class inventories locally.

```powershell
pwsh -NoProfile -File tools/upstream/order-index-oracle/Invoke-Oracle.ps1 -Help
```

Each Java command, including version detection, has a 128MiB maximum heap and
60-second timeout. Only an owned timed-out process tree is stopped. Exit0 means
success/help; exit1 means invalid inputs, missing/mismatched production reference,
process/timeout, provenance or observation failure. No network or dependency work
starts during help or capture.

The [fixture](../../../tests/fixtures/order-index/oracle.tsv) records 321 observations
in two timezones, source classification, raw index extraction/replacement,
production record-source resolution, actual standard field declarations and
OrderDelegate receive packing, invalid suffixes and physical-int behavior under
other full identities. Negative raw-state observations use reflection to set the
private index, then invoke production methods; public `setIndex` rejection is
captured separately. A production mapping can resolve a record for which the
factory offers no typed delegate; these outcomes have distinct row tags.

Java's physical-int acceptance does not broaden the C++ exact type8 profile.
Default-field factory failures are retained as observations, not invented Java
mapper rejections. See the [contract](../../../spec/events/ORDER-INDEX.md).
