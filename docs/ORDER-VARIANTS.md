# Use owning SpreadOrder and AnalyticOrder values

[Русская версия](ORDER-VARIANTS.ru.md). English is authoritative.

Include `qd/events/spread_order.hpp` or `qd/events/analytic_order.hpp`. Both values own their strings and
numeric snapshots. A retained copy remains valid after the original cursor, callback batch, subscription
or runtime is destroyed. SpreadOrder's `symbol` is the underlying instrument; `spreadSymbol` is a
nullable description carried by the event. Preserve null and empty as distinct values.

```cpp
namespace q = org::ttldtor::qdcxx;
void handle(const q::events::SpreadOrder &order) {
    consumeUnderlying(order.symbol, order.getSourceId(), order.getIndex());
    if (order.spreadSymbol) {
        consumeSpreadDescription(*order.spreadSymbol);
    }
}
void handle(const q::events::AnalyticOrder &order) {
    consumeIceberg(order.getPrice(), order.getIcebergPeakSize(), order.getIcebergHiddenSize(),
                   order.getIcebergExecutedSize(), order.getIcebergType());
}
```

The `consume...` functions represent application handling. Use an exact AnalyticOrder value when retaining
iceberg fields: assigning to ordinary `Order` copies only its base fields. `flags`, `eventFlags` and
`icebergFlags` are independent. Reserved iceberg type3 reads as UNDEFINED; the original raw flags remain
available. Missing optional iceberg sizes read as NaN. Changing a public wide value makes its numeric
getter use the current value, while other source business-number snapshots remain valid.

Advanced schema-bound callers use `PreparedSpreadOrder::prepare` or `PreparedAnalyticOrder::prepare` with
an explicit immutable local record ID and finite MappingLimits, then `map`/`mapView` with that record's
exact context and complete canonical values. `mapView` copies all borrowed text before returning. Supply
explicit EventValueLimits for the symbol, each nullable string and their total. The mapper validates
all input first and charges complete text before allocating a result. Check the returned `expected`;
resource, context, type and allocation failures publish no partial event and leave the mapper reusable.

Use `defaultSpreadOrderSchema`/`defaultAnalyticOrderSchema` for explicit native DEFAULT projections or
supply a richer validated schema. SpreadSymbol must be declared even when its value is null. Analytic
MMID/MarketMaker and iceberg fields are optional. The immutable record name determines the ordinary
source. `OrderSource::isPublishable` describes direct publication capability; it does not decide whether
a supplied receive record is valid. For example, explicit AnalyticOrder#NTV can be mapped even though
NTV's analytic publication flag is false.

The [mapping contract](../spec/events/ORDER-VARIANTS.md) specifies numeric types, source identity,
independent business/FIELD/removal times and ownership/error guarantees. See the
[production oracle tools](../tools/upstream/order-variants-oracle/README.md) for reference reproduction.