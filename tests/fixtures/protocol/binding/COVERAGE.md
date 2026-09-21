# Production binding observations

[Русская версия](COVERAGE.ru.md). English is authoritative.

[Harness](../../../../tools/upstream/binding-oracle/README.md), [inputs](inputs.tsv),
[exact observations](expected/binding.tsv), [validation](VALIDATION.md).
13 fresh-parser cases contain 16 stimulus rows. Twelve described cases produce 15 independent plan
projections; undescribed is a parser-error comparison, not a planner input. Local order is S=0, R=1,
E=2; R fields are y=0, x=1, z=2. Explicit alias xdot targets x and becomes its Java property name.
All fields are ordinary COMPACT_INT, no time/object fields. Metadata/replacers/properties are disabled.

| Case | Production observation |
| --- | --- |
| reorder | Sender 7 delivers R=1, y=202, x=101. |
| alias / full | xdot / R.x resolve x; delivered x=303 / 505, y=404 / 606. |
| middle-skip | Unknown field value 808 between x=707 and y=909 does not disturb alignment. |
| missing-wire | x=111; Java y/z storage is zero, not an application default promise. |
| zero | Described R with zero wire fields still delivers R with zeroed Java slots. |
| empty-local / empty-both | E=2 has no local fields; one skipped value / no values both deliver E. |
| duplicate | x,xdot,y,x=(333,444,555,666) delivers y=555,x=666. |
| sparse | Sender 4096 delivers R=1 with y=888,x=777. |
| alignment | Unknown sender 31 record consumes 123,234; then S=0(v=345), R=1(y=456,x=567) deliver in order. |
| case-sensitive | Lowercase r is skipped; uppercase X is absent from R; y=890 survives. |
| undescribed | Data ID 19 without description causes corrupted-message callback type 15, no records. |

All successful cases end with no pending bytes/errors. Unknown-record logging remains in raw stderr.
Plans are separately designed in [plans.tsv](../../../conformance/protocol/binding/plans.tsv), not
Java callback output. The audit matches raw payloads
and projects input values through plans to actual delivered IDs/order/values. Intermediate overwritten
writes in duplicate are not independently observable: BinaryRecordDesc.field2Desc/readFields in
[SOURCES](../../../../spec/protocol/SOURCES.md) supports preserving all target actions in wire order.
The unknown-record skip operation is evidenced by continued alignment, not a direct skip callback.

Historical P03/P04 are reused as evidence via [CAPTURES](../../../../spec/protocol/CAPTURES.md);
their files are not rewritten or rerun. No XML importer, defaults, conversions, dynamic replacement,
reset, metadata, provider matrix, transport or full protocol parity is established.
