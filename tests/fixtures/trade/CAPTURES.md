# Trade capture layout

[Русская версия](CAPTURES.ru.md).

The raw dataset is `oracle.tsv` in this directory. `provenance.json` binds its exact bytes to the
locked production source/class inventory and the [reproducible helper](../../../tools/upstream/trade-oracle/README.md).
The helper uses actual generated TradeMapping/TradeDelegate, never a diagnostic converter copy.
The fixed configuration enables nano time and Trade exchange variants Q/z. Each scenario preserves
complete physical record fields while reversing their logical order to exercise lookup identities.
Synthetic VOID tails remain in Java physical storage and are excluded only from native logical fields.

`record` declares scenario, record ID and exact name (Trade or Trade&Q). `field` adds logical position,
full serial type ID, local and property lookup names. `symbol` declares sample physical UTF-16 hex
units and the original FIELD event-time sequence. `value` retains each complete scalar raw value;
nullable text marker is null because these Trade profiles contain no object fields.

`trade` retains, after scenario/record/sample, these25 cells:

| Cells | Actual production result |
|---|---|
|0 | logical event symbol as exact UTF-16 hex units |
|1..6 | eventTime milliseconds, timeSequence, business milliseconds, business nanoseconds, sequence, nano part |
|7..12 | exchange UTF-16 unit, TradeId, dayId, complete post-fallback flags, direction code, extended-hours boolean0/1 |
|13..17 | wide accessor raw price, size, change, day volume, day turnover |
|18..22 | corresponding actual business getter double bits, signed64 spelling |
|23..24 | actual business integer size and day volume |

Flags are read through the actual package-private production getter using bounded harness reflection;
no native fallback formula generates the expected flags. `reject`/`conversion-reject` retain actual
factory/accessor failure. `native-reject` labels the explicit native one-slot TradeId refusal while
preserving the actual successful Java adjacent-slot observation.

Families cover six numeric representations (including all128 tiny low patterns across five mantissa
boundaries, LONG tie/carry/extremes and a1ULP business witness), eleven raw-int representations,
three raw64 TradeId representations, one-slot TradeId with a known physical neighbor, missing optional
fields, missing required price/size, unsupported numeric flags/time representations,128 flags/tick
combinations, and physical symbols with existing suffixes/attributes. Full positive and negative
shapes remain available; unsupported typed behavior is not silently masked.

Native conformance compares owning and borrowed projections. The public standalone Feed consumer
separately proves exact ADD/REMOVE record/symbol tuples, composite/regional binding, all seven mixed
type sets, attributes, wrong-record/unsubscribed drops, explicit reconnect and listener-fault isolation.
Those deterministic local TCP observations are distinct from a public live-service run.
The final mixed demand adds a distinct no-data marker: its wire ADD proves backend membership before
initial delivery. After the first withdrawal, a live marker follows the withdrawn composite in the
same DATA frame; its callback precedes the remaining withdrawal. The final REMOVE sends no new DATA,
so ordinary local drain is checked without racing an artificial late packet at shutdown.
The final REMOVE group requires its exact six record/symbol tuples across complete frames, allowing
their asynchronous order to vary; missing, duplicate and extra tuples remain failures.
