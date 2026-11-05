# Time-series subscription values and logical ranges

[Русская версия](TIME-SERIES-SUBSCRIPTIONS.ru.md) · [API reference](api.md)

Include `qd/api/time_series_subscription.hpp` and link `qdcxx_subscription_selectors`.
The header and target are available independently of the optional Feed and transport components.
This component owns business identities, decorated symbol values and mutable logical ranges.
It does not create a live HISTORY subscription. The current nine registered native event types
have no TimeSeriesEvent business marker or Feed HISTORY receiver.

```cpp
#include <qd/api/time_series_subscription.hpp>
#include <array>

namespace api = org::ttldtor::qdcxx::api;

auto type = api::TimeSeriesEventDescriptor::declared(
    u"example:MyTimeSeriesEvent", api::EventSubscriptionCapability::TIME_SERIES);
if (!type) return 1;
auto model = api::TimeSeriesSubscriptionModel::create(std::span(&*type, 1));
if (!model) return 2;
const std::array symbols{api::SubscriptionSelector::plain(u"IBM")};
if (!model->setSymbols(symbols) || !model->setFromTime(0)) return 3;
auto cut = model->snapshot();
if (!cut) return 4;
// cut owns one coherent range, raw set, decorated set and logical revision.
```

The stable identity is an explicit caller-owned business identifier. It is neither a Java class
lookup nor a schema or decoder registration. A caller must not give distinct business types the
same identity. The descriptor accepts custom and inherited time-series families; it is not a
closed list of Candle and market event names. `hasTimeSeriesFeedReceiver` separately returns false.
`isTimeSeriesEventType` queries the existing native registry and returns false for all current types
and unknown enum values. An INDEXED or NON_INDEXED declaration cannot create this model.

`TimeSeriesSubscriptionSymbol<Symbol>` owns its underlying value. Its identity and hash exclude
the outer `fromTime`; `sameDemand` also compares the range and underlying demand attributes.
The default `SubscriptionSelector` domain preserves exact UTF-16, source identities and typed
decorators. Nested wrappers are retained. Caller-defined symbol types must supply symmetric
equivalence and equal, stable hashes for equal values; the concept checks syntax only.
The native typed-domain correction in [ADR-0012](decisions/ADR-0012-decorated-subscription-identity.md)
continues to apply inside nested values.

The model starts with an empty symbol set and `fromTime == INT64_MAX`, the Java disabled sentinel.
Every signed millisecond value is retained exactly, including MIN and MAX. Changing the range
does not change raw symbol identities. A snapshot decorates every retained raw value with that
snapshot's range. `setSymbols` replaces and `addSymbols` merges by typed identity, retaining the
last supplied demand attributes; removal ignores attributes excluded from identity. Reordering an
otherwise equal set is a no-op and retains its previous order. Effective changes receive one
checked owner-local revision; revisions do not identify transport or source epochs.

Finite limits bound input entries, retained entries and UTF-16 units. Invalid capabilities,
conflicting declarations, resource exhaustion and allocation failure return typed errors.
Failed mutations preserve the complete accepted range, set and revision. Snapshots allocate
independent copies and can fail without changing the model. No callback or I/O runs under its
single mutex. Concurrent ordinary methods serialize; moving and destruction require external
exclusion. A moved-from model rejects operations. Detached snapshots survive moves and destruction.
The model has no close lifecycle, event dispatch, callback notices, completeness or delivery promise.

The [normative contract](../spec/subscriptions/TIME-SERIES-IDENTITY.md) explains Java observations
and native distinctions. The [production oracle](../tools/upstream/time-series-oracle/README.md)
captures the actual pinned Java classes, including inherited capabilities and the raw-generic bypass.
