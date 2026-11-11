# Receive indexed Order

[Русская версия](ORDER.ru.md). English is authoritative.

Include `qd/api/feed.hpp` and link `qdcxx::feed` (or the compatible `qdcxx_feed`). Select HISTORY explicitly:

```cpp
namespace q = org::ttldtor::qdcxx;
q::api::ApiRuntime runtime;
q::api::EndpointOptions options;
options.contract = q::api::DeliveryContract::HISTORY;
auto endpoint = runtime.createEndpoint(options);
auto orders = endpoint->getFeed()->createSubscription<q::events::Order>();
orders->addEventListener([](std::span<const q::events::Order> batch) {
    for (const auto &order : batch) {
        consume(order.symbol, order.getSourceId(), order.getIndex(), order.getPrice(), order.eventFlags);
    }
});
orders->addSymbols("AAPL");
```

This sequence configures local demand; connect the endpoint and perform checked shutdown as in the
[complete Feed example](../examples/feed_quote/README.md). `consume` stands for the application's own
synchronous handling. A listener span expires when its callback returns. Copy an Order to retain its
symbol, fields and nullable MarketMaker independently of the subscription/runtime lifetime.

HISTORY without an explicit registry uses SDK-owned standard Order and MarketMaker schemas. This page describes native Order; [derived Order sources](MARKET-MAKER-ORDER.md) additionally project Quote on TICKER and MarketMaker on HISTORY. Source-specific
schemas are supplied through the existing immutable EventRegistry; register every selected record as
EventType::ORDER. Each native indexed layout must explicitly select its exact COMPACT_INT Index with
IndexedTimeLowInt32. Source names are resolved from the record; schema names and extra fields remain
caller-owned configuration. Applications do not construct collectors, queues or per-turn work objects.
TICKER remains the endpoint default. Typed or mixed Order requests reject incompatible contracts before
subscription publication. Ordinary STREAM Order reception is not inferred from publication delegates.

A plain selector demands every registered Order source. An explicit indexed selector demands its source;
negative source IDs select the available source set, retaining their distinct logical identity. For example:

```cpp
const auto source = q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::NTV.indexedSource());
const std::array selectors{q::api::SubscriptionSelector::plain(u"AAPL"), source};
orders->setSelectors(selectors);
orders->removeSelector(selectors[0]);
```

Removing the plain selector retains NTV demand. Overlapping selectors do not duplicate a physical record
within one subscription. Separate subscriptions remain independent. Source records with equal low indexes
retain independent HISTORY partitions and transaction/snapshot progress. Public eventFlags are the actual
collector delivery flags; business flags remain separate. The full field/index contract and explicit native
versus JSON layout boundary are in [Order](../spec/events/ORDER.md).
