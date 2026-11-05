# Order production oracle

[Русская версия](README.ru.md). English is authoritative.

The launcher captures or verifies the locked standard Order schemas, their original physical VOID
and long-tail slots, actual BinaryQTPComposer/BinaryQTPParser HISTORY data, and production OrderDelegate
receive values. It does not rebuild QD, open a feed or change shared production classes.

```powershell
pwsh -NoProfile -File tools/upstream/order-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/order-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/order-check
```

No arguments and `-Help` show identical English help; `-HelpRu` shows Russian help. These modes need
no operational paths and start no file, Java or SDK work. `Mode` defaults to Verify. Operational modes
require an existing exact [locked revision](../../../upstream.lock), prepared production classes, JDK8,
a native UTC logger and a new run directory inside this checkout's `.work`. Existing evidence is refused.
Each child has a 128 MiB maximum Java heap and a 60-second process timeout; only that launcher's own
timed-out child is stopped. Heap is not a process RSS limit. No network, dependency download or global
configuration change occurs. Exit0 means success/help; exit1 means invalid input, missing/mismatched
reference, timeout, process, comparison or output failure. A failed capture leaves diagnostic files.

The four explicit child scheme profiles are default wide, tiny decimal, configured integer price/size
and FOB. Each is observed in UTC and Pacific/Honolulu. The standard records are Order, Order#NTV and
Order#BZX. Controlled variants retain physical VOID high0/Index low1, complete remaining fields and
source names. Numeric/raw alternatives, optional absence, each missing required field, flags, signed
low-index limits and full-width removals are observed through the real parser/delegate. Required-field
factory refusals remain negative evidence. The composer explicitly enables the `hs` protocol option; the parser must retain every flags value from0 through31, including snapshot begin/end. Source publication capability is not subscription admission.

Capture replaces only [Order TSV](../../../tests/fixtures/order/oracle.tsv), original binary fixtures and
[provenance](../../../tests/fixtures/order/provenance.json). Verify compares all of them without replacing
public files. Logs, raw streams, exact command exits and before/after inventories of all 14 class/resource
roots remain in RunDirectory. Public provenance contains stable hashes and no local run paths.
The [event contract](../../../spec/events/ORDER.md) states native mapping and JSON layout limits.
