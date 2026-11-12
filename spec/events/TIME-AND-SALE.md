# TimeAndSale reception {#time-and-sale}

[Русская версия](TIME-AND-SALE.ru.md) · [Public Feed API](../../docs/FEED-API.md)

Include `qd/api/feed.hpp` and link `qdcxx::feed`. A STREAM endpoint accepts
`createSubscription<events::TimeAndSale>()` and delivers individual reports in order while capacity
permits. To select a HISTORY range, create a HISTORY endpoint, call
`createTimeSeriesSubscription<events::TimeAndSale>()`, and use `setFromTime(unixMilliseconds)`.
The lower bound is inclusive and uses the packed seconds and millisecond coordinate.
TICKER rejects the type before registration.

The default STREAM and HISTORY registries append the composite `TimeAndSale` record and the
26 regional records `TimeAndSale&A` through `TimeAndSale&Z`. The logical symbol `AAPL&Q` selects
the regional record and sends physical symbol `AAPL`. The callback reconstructs `AAPL&Q`.
With an explicit registry, the matching regional record takes precedence when present. A missing
regional route falls back to the composite record with the full logical symbol. The trade exchange
code remains the separate source `Exchange` field. The STREAM All selector requires the endpoint's
explicit wildcard permission. HISTORY uses exact symbols.

The callback supplies owned values. The index combines signed business seconds with the unsigned
millisecond and sequence word. FIELD event time is independent. Nano time, trade ID, buyer and seller
use their Java defaults when absent. A validated explicit registry can enable optional fields and select
the actual source representation. Wide and tiny decimal representations preserve their Java business
values, including fractional size. Sale conditions decode the source short string.

The raw trade flag word exposes new, correction and cancel types, aggressor side and other sale
conditions. Indexed event flags separately describe snapshots, transactions and removal. A trade
cancel remains a report unless indexed delivery also carries `REMOVE_EVENT`. When a removal carries
only its indexed coordinate, that coordinate supplies the owning result's index. The indexed source is
`IndexedEventSource::DEFAULT`. Snapshot completeness and later updates follow the existing
[HISTORY observation contract](../subscriptions/LIVE-HISTORY-RANGES.md).

[Pinned Java fixture and generator](../../tools/upstream/time-and-sale-oracle/README.md) capture
the actual STREAM and HISTORY delegates, composite and regional coordinates, and supported schema
representations.
