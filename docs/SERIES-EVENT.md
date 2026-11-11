# Option series events

[Русская версия](SERIES-EVENT.ru.md). English is authoritative.

[Series](../include/qd/events/series.hpp) owns computed values for one option
series of an underlying symbol. Several series can share an expiration day.
The index distinguishes them. The source is always `IndexedEventSource::DEFAULT` (ID zero).

| Field | Meaning |
| --- | --- |
| Symbol and index | Exact underlying symbol and per-symbol series identity. The public index widens the signed 32-bit wire value to signed 64 bits. |
| Event flags | Snapshot, transaction and removal flags from indexed delivery. |
| Event time | Independent Unix milliseconds when FIELD metadata is selected, otherwise zero. |
| Time and sequence | Business timestamp and low 22 sequence bits from `Time` and `Sequence`. |
| Expiration | Signed day ID, counting days from 1970-01-01. |
| Volatility | Implied volatility index. |
| Call and put volume | Daily traded call and put option volume. `getOptionVolume()` adds the available values and returns NaN when both are NaN. |
| Put/call ratio | Ratio of traded put to call option volume. |
| Forward price | Implied forward price. |
| Dividend and interest | Implied simple dividend and interest returns. |

The owning value retains the wide-decimal projection and the business double.
An accessor uses the current decimal after a change to its public wide field.

The native default record is `Series`. The logical schema represents the physical
VOID/Index prefix by its `Index` field. The collector key keeps the unsigned low
word. A removal can also carry a nonzero high word. The public index reflects the
signed low word on either path. Expiration and business time are independent
of this indexing.

`PreparedSeries` accepts an explicitly selected record from a validated caller
schema. It requires `Expiration`, `Volatility`, `PutCallRatio` and `ForwardPrice`.
Absent `Index`, `Time` and `Sequence` are zero. Absent `CallVolume`, `PutVolume`,
`Dividend` and `Interest` are NaN. An indexed subscription also needs a validated
indexed layout whose low-word contributor is `Index`.

The mapper validates every field and its schema/record identity before projection.
The returned event owns its symbol and values independently of the schema,
mapper and source cursor.

[Reproduce the pinned Java observations](../tools/upstream/series-oracle/README.md).
