# Production HISTORY receiver oracle

[Русская версия](README.ru.md).

`Invoke-Oracle.ps1` captures or verifies the pinned Java8 production HISTORY collector with the
actual standard TimeAndSale schema and delegate. Its 242 TSV rows include complete physical schema,
raw millisecond-to-record-time conversion, two independent ranges, partial/covered delivery,
END/removal, transaction, SNIP and reattached cached replay. Seven QTP fixtures come directly from
`BinaryQTPComposer`; no handwritten business wire encoder supplies those bytes. Complete non-VOID
raw integer/long/wide-decimal values accompany the business output. Physical VOID tails are not
native logical fields.

This is raw QDAgent/delegate evidence. It does not establish public native typed TimeAndSale,
DXFeed callback completeness, peer acknowledgement or future network freshness. The native test uses
those exact bytes with the common production HISTORY queue/receiver and observation helpers.

```powershell
pwsh -NoProfile -File tools/upstream/history-receiver-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/history-receiver-check-01
```

No arguments, `-Help`, `-HelpRu` and `Get-Help ./Invoke-Oracle.ps1 -Full` need no paths and start no
work. Verify is default. Capture replaces only `tests/fixtures/history-receiver/oracle.tsv`,
`provenance.json` and seven `wire-step-N.qtp` files. Four operational paths are mandatory. The run
directory must be fresh inside this checkout's `.work`, without redirecting ancestors. Existing
evidence is refused. Existing pinned production classes/resources are inventoried before and after;
upstream is neither built nor written. Source, JDK, harness, launcher and fixture hashes bind the
observations. Sequential javac/Java use a 128MiB heap and each subprocess has a 60s deadline; only
owned timed-out children are stopped. UTC and Pacific/Honolulu TSV and every wire file must match
byte for byte. Exit0 means success/help; Exit1 means invalid inputs, process/timeout failure,
reference/provenance or observation mismatch. Complete diagnostics remain in the run directory.
No network or SDK provisioning is performed.

See [the native contract](../../../spec/subscriptions/HISTORY-RECEIVING.md).
