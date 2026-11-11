# MarketMaker and derived Order sources

[Русская версия](MARKET-MAKER-ORDER.ru.md). English is authoritative.

Include `qd/api/feed.hpp` and link `qdcxx::feed`. The SDK owns the default schemas, collectors,
source selection and callback retirement. `MarketMaker` is an indexed event. It has no time-series
business capability; an indexed collector layout never grants that capability.

```cpp
namespace q = org::ttldtor::qdcxx;
q::api::ApiRuntime runtime;
q::api::EndpointOptions options;
options.contract = q::api::DeliveryContract::HISTORY;
auto endpoint = runtime.createEndpoint(options);
auto makers = endpoint->getFeed()->createSubscription<q::events::MarketMaker>();
makers->addEventListener([](std::span<const q::events::MarketMaker> batch) {
    for (const auto &maker : batch) {
        // Copy a value when retaining it after this callback.
        const auto bid = maker.getBidPrice();
        const auto count = maker.getBidCount();
        (void) bid;
        (void) count;
    }
});
makers->addSymbols("AAPL");
```

The owning DTO preserves the full indexed flags, independent FIELD `eventTime`, full bid/ask
milliseconds, four wide values and their direct business-double views, and signed counts. Public
index identity is char16 exchange in high32 plus unsigned MMID in low32; its source is DEFAULT0.
The collector retains its original physical raw64 time independently. A removal may therefore have
a different full collector key while producing the same narrowed public identity. Nullable maker
text decodes the current low32 without retaining an input record.

An ordinary TICKER endpoint supports Quote-derived Order from composite Quote and every real
regional record A..Z. An explicit HISTORY endpoint supports ordinary Order and MarketMaker-derived
aggregate Order. STREAM publication/wildcard delegates do not grant ordinary receiving permission.
The existing TICKER endpoint default is preserved.

```cpp
auto endpoint = runtime.createEndpoint();
auto orders = endpoint->getFeed()->createSubscription<q::events::Order>();
const auto composite = q::api::SubscriptionSelector::indexed(
    u"AAPL", q::events::OrderSource::COMPOSITE.indexedSource());
orders->setSelectors(std::array{composite});
```

| Physical record | Separate bid / ask source | Unitary source | Receiving contract |
| --- | --- | --- | --- |
| Quote | COMPOSITE_BID1 / COMPOSITE_ASK2 | COMPOSITE7 | TICKER |
| Quote&A..Z | REGIONAL_BID3 / REGIONAL_ASK4 | REGIONAL8 | TICKER |
| MarketMaker | AGGREGATE_BID5 / AGGREGATE_ASK6 | AGGREGATE9 | HISTORY |

Plain and negative-source selectors choose both separate sides by default. Set immutable
`EndpointOptions::unitaryOrderSources=true` to choose paired unitary sides instead. Explicit source
IDs select their actual delegates independently of that default; source0 selects ordinary native
Order where registered. Raw Quote retains its market source-selection behavior. Raw MarketMaker
accepts plain/negative/default0 and excludes positive source IDs. An All selector still requires the
existing explicit STREAM permission and does not turn these delegates into a STREAM subscription.

Derived Order keeps the literal QD symbol, including an existing suffix. Raw regional Quote instead
uses its existing logical-to-physical suffix conversion. The group keeps bid before ask. Unitary
bid sets TX_PENDING and clears END/SNIP; unitary ask clears BEGIN. Separate Quote-derived Order
has flags0; separate aggregate Order preserves the actual cursor flags. Derived Order starts
sequence and nano remainder at zero, ignoring Quote Sequence/TimeNanoPart. Side time uses the
production Order time packing, including saturated signed32 seconds. Derived exchange must fit
ASCII after the production char16 cast. Negative MMID has the production signed text behavior;
it can produce eight byte-valued units in derived Order and four in raw MarketMaker.

Logical selectors coalesce physical demand. Each physical row has at most one native output plus
four derived outputs. Each captured output retains its original eligibility; withdrawal removes
only its own unselected contributions. Removed and readded sources cannot revive an old queued
contribution. Source cache expansion replays the complete current delegate group. It conflates
only an old source-owned dirty group; records already transferred to the delivery FIFO retain their
original identities. A selected custom-executor packet remains immutable through its retirement.

Delivery statistics count business outputs, including pending, in-flight, delivered, discarded,
conflated and failure outcomes. QueueCounts and transport record observations retain physical-record
units. Unitary pairs are projected atomically. Four physical callback rows produce at most twenty
owning results; complete packet text/body/fixed storage is prepaid before output allocation. No
listener is invoked with a partially projected group. Listener fanout does not multiply event counts.

The SDK TICKER catalog contains 35 physical records, 225 logical fields, 607 lookup keys and 62 native/
derived route declarations. Receiving bounds are 42 records, 284 fields, 724 lookups,128 units per name,
16384 total name units and 84 compatible routes. A physical record admits at most one native route
and one compatible derived family. The public `EventRegistry::builtIn` nine-route catalog and its
caller-supplied default limits remain unchanged. The SDK supplies its larger checked catalog itself.
Default 8 subscriptions with 16 logical symbols reserve at most 4480 physical request slots. A native
regional Quote strips the exchange suffix while derived Order keeps its literal symbol, so one context
can require two physical keys. Checked capacity includes the largest same-exchange regional pair bucket;
a custom 42-context paired profile can reserve 84 coordinate domains, or 10752 aggregate slots.
Registry storage retains its existing 8 MiB ceiling. Callback packet/pool credits remain configurable
through the existing [callback execution](CALLBACK-EXECUTION.md) options.

The [production oracle](../tools/upstream/market-maker-order-oracle/README.md) records original full
types, integer slots, both source-default modes, optional nano fields, all 32 flags and full64 removals.
Native tests compare owning mappings and the real generated QTP bytes. See [ordinary Order](ORDER.md)
for source-qualified native Order and [Feed API](FEED-API.md) for lifecycle and range observations.
The receiving protocol text seed covers the existing64-poll shared worker burst with checked complete descriptor/schema binding scans. It is derived before activation and is never refilled during retry. The existing64KiB snapshot arena is retained; complete35-record prefix and exact/one-under text-credit tests cover that boundary.
