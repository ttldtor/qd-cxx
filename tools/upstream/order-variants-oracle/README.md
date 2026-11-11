# SpreadOrder and AnalyticOrder production oracles

[Русская версия](README.ru.md). English is authoritative.

The two launchers exercise the locked production schemas, BinaryQTPComposer, BinaryQTPParser and
SpreadOrder/AnalyticOrder HISTORY delegates. They use existing compiled Java classes without opening
a feed, rebuilding QD, downloading dependencies or changing global properties.

```powershell
pwsh -NoProfile -File tools/upstream/order-variants-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/order-variants-oracle/Invoke-FieldTimeOracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/order-variants-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/variants-check
```

Both scripts accept the same parameters. `Mode` defaults to `Verify`. `Upstream`, `JavaHome`, `Logger`
and `RunDirectory` are required for operational Capture/Verify. The reference must match
[upstream.lock](../../../upstream.lock), contain built production classes, and use JDK 8. The formatter
uses its separately pinned modern JDK. The run directory must be new and inside this checkout's `.work`;
existing evidence is refused. No arguments and `-Help` show the same complete English help; `-HelpRu`
shows Russian help. Help needs no operational inputs and starts no Java or fixture work.

Each child runs sequentially with a 128 MiB Java heap limit and a 60-second timeout. Heap is not a
process RSS limit. Only the launcher's own timed-out process tree is stopped. Logs include command
arguments/exits, raw streams, and before/after inventories of all 14 production class/resource roots.
Exit 0 means success/help; exit 1 means invalid input, missing/mismatched reference, process/timeout,
output I/O or comparison failure. A failed capture retains its diagnostics. Capture replaces only the
selected tool's fixtures/provenance. Verify reads the public fixtures without replacing them.

`Invoke-Oracle.ps1` observes default-wide, tiny-decimal, configured-integer and FOB profiles separately
in UTC and Pacific/Honolulu: 200862 TSV rows and 200 original QTP files. It includes source publication
capabilities, actual standard-record availability, source-specific optional shapes, required-field
refusals, missing optional defaults, raw/numeric boundaries, flags, full-width removal times and exact
nullable UTF-16 spread strings. Its FIELD values are zero/default. Outputs are
`tests/fixtures/order-variants/oracle.tsv`, `wire/*.qtp` and
[provenance.json](../../../tests/fixtures/order-variants/provenance.json).

`Invoke-FieldTimeOracle.ps1` separately enables/disables the existing FIELD write/read switches. Its
51 rows include 48 typed observations, negative/full-width packed precision and independent business
time. Outputs are [field-time-oracle.tsv](../../../tests/fixtures/order-variants/field-time-oracle.tsv),
`field-wire/*.qtp` and [field-time-provenance.json](../../../tests/fixtures/order-variants/field-time-provenance.json).
Each TSV and wire output must be identical in UTC and Pacific/Honolulu. Stable provenance binds the
harness, launcher, upstream source hashes and unchanged compiled-class inventory without local run paths.

The [variant contract](../../../spec/events/ORDER-VARIANTS.md) distinguishes publication capability,
standard-scheme availability and explicit receive-record admission. These oracles do not exercise
network transport, custom callback executors or the native Feed lifecycle.