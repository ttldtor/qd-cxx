# Immutable receiving event registry

[Русская версия](EVENT-REGISTRY.ru.md). English is authoritative.

## Boundary and ownership

`api::EventRegistry` owns an immutable complete `core::LocalSchema` and explicit
`EventRegistration` routes. It resolves no process properties, environment, files,
providers or wire descriptions. Construction performs no native, network or
application callback work. The registry belongs to the optional `qdcxx_feed`
component; core, protocol and event adapters gain no transport dependency.

`create` bounded-copies validated declarations and preserves their exact schema
identity. Copying a registry shares immutable storage without allocation. The
original schema and registration containers may retire immediately after success.
An independently recreated equal schema has a different identity; it cannot tag
values for a mapping prepared from this registry. Moving a registry transfers its
ownership. Only `valid()`, `storageBytes()`, destruction and assignment are
permitted on its moved-from value; `storageBytes()` returns zero for that value.

Const registry inspection may run concurrently. Returned schema references and
route spans borrow shared immutable backing and require an owning registry copy to
remain alive. The registry retains no application callable or capture.

## Explicit mappings

`EventType::QUOTE`, `EventType::PROFILE`, `EventType::TRADE`, `EventType::TRADE_ETH`, `EventType::SUMMARY`, `EventType::TEXT_MESSAGE` and `EventType::TEXT_CONFIGURATION`, `EventType::MESSAGE` and `EventType::CONFIGURATION` select the existing prepared adapters.
Record names do not infer a type. Each route supplies an explicit local record ID,
finite `MappingLimits` and an `EventTimePolicy`. IDs retain the `LocalSchema`
requirement: each ID equals its position in the complete record catalog.

Distinct records may project the same event type. Each physical record permits at most one native route and one compatible explicit `EventProjection` derived family. Two native routes, even with different business types, two derived groups, or an incompatible native/group pair reject atomically. Quote pairs with QUOTE_ORDERS; MarketMaker pairs with MARKET_MAKER_ORDERS. Route
inspection preserves input order. A single-type feed subscription selects registered
routes of that type, applying the [regional Quote exchange rules](EVENT-TIME.md);
several selected types use the
[mixed subscription contract](MIXED-SUBSCRIPTIONS.md).

Preparation uses the existing adapters' exact accepted lookup keys, full type
alternatives, required/optional fields and business defaults. Caller lookup aliases
may map a supported business key to any compatible field position. Complete
extra fields and unregistered generic records remain in the retained schema;
projection does not replace the complete generic representation. A custom wire
alias can be the first lookup for a field while a supported business alias selects
that same field. No aliases, conversions or arbitrary C++ event classes are inferred.

The exhaustive [field conversion table](FIELD-CONVERSIONS.md) belongs to each prepared adapter.
Default registration bounds allow 128 exact type alternatives and conservatively charge their
comparison work; Quote requires72, Profile108, Trade110, TradeETH99 and Summary75. Explicit lower bounds still fail atomically.
The complete receiving caps are 42 records and284 fields; the public built-in catalog retains its caller-supplied limits.

`FIELD_METADATA` selects actual original FIELD event time when present;
`UNSPECIFIED` asserts no event timestamp. Neither uses indexed identity as time.
Per-result symbol/text ownership bounds remain the receiving profile's explicit
derived bounds, separate from registration preparation.

## Atomic errors and finite bounds

`EventRegistryLimits` bounds the complete retained schema, number of routes and
retained schema/container/shared allocation-request bytes. `storageBytes()` reports
the same conservative backing, including container capacity and the actual shared
state/control allocation request. The allocator refuses that complete request before
allocating. Its final deallocation does not access construction-local accounting.
Allocator overhead, caller inputs, independently created copies and process RSS
are outside that byte coordinate. Schema/count
bounds independently bound all registry-owned elements and temporary adapter plans.

`EventRegistryWork` supplies visits and conservative mapper preparation credits.
Visits cover attempted schema element/name-unit copies and route/duplicate
inspections. Before each adapter attempt, preparation is charged the checked sum
of its declared `MappingLimits` coordinates. Credits consumed by a failed attempt
are not refunded; construction never adds credits internally. Zero is a real
limit. Checked additions/products reject overflow.

Failure publishes no registry and preserves caller inputs. The result retains
the original `SchemaError` or `MappingError`, or a distinct registry invalid-input,
resource, work or allocation error. A route-specific error includes its input
position. Unknown event/time enums, invalid IDs, duplicate local IDs, moved-from
schema origins, missing required keys and exact type mismatches reject before
publication. Registry construction translates allocation exceptions.

## Defaults and receiving limits

`builtIn` uses the same registration boundary: complete default Quote record0
with eight fields, Profile record1 with sixteen fields, composite Trade record2 with thirteen fields,
composite TradeETH record3 with twelve fields, Summary record4 with ten fields, TextMessage record5 with
three fields, TextConfiguration record6 with four fields, Message record7 with one SERIAL_OBJECT field
and Configuration record8 with Version and SERIAL_OBJECT fields, with their existing
lookup aliases and business behavior. An endpoint without an explicit registry
selects a contract-aware SDK catalog: TICKER has the nine native declarations plus26 real regional Quote records and27 Quote-derived Order groups; HISTORY has native Order, native MarketMaker and its derived Order group. STREAM retains the nine native declarations. Ordinary contract permission remains separate from registration. See [MarketMaker and derived Order](../../docs/MARKET-MAKER-ORDER.md). There is no process-global registration or live mutation.

The high-level plain receiving profile retains its qualified complete-schema
ceiling: 42 records, 284 total fields, 724 lookup entries, 128 UTF-16 units per name
and16384 total name units. A registry can be valid under larger caller-selected
bounds and still be refused by that receiving profile at `profile-schema`, before
native activation. This is an explicit receiving limitation, not truncation or
rejection by the generic schema boundary.

Within that ceiling, preparation preserves the exact schema identity and groups
routes by type. It checks and aggregates complete mapping shape/selection/work
coordinates, callback projection allowances and desired-command capacities.
Each accepted symbol requires one interest per distinct matching physical key. Native/derived overlap
coalesces projection bits before the complete desired command is submitted. Native regional Quote
uses a suffix-free physical symbol while derived Order keeps the literal symbol. One logical selector
can therefore require two physical keys for a compatible regional context. Capacity is the maximum of
registered context count and eligible context count plus the largest same-exchange regional native/derived
bucket, using the same contract eligibility guard. Generic unregistered records do not increase it.
Default 35 coordinate domains reserve 560 physical entries per subscription and 4480 for 8 subscriptions.
Custom 42 paired contexts can require 84 domains, or 1344/10752 entries. These coordinate domains do not
change the 42-record schema ceiling. Actual maximum field count sets the frame allowance, retaining
the minimum 16.
Four physical callback records can produce at most20 owning business outputs. Registry, receiver,
queue and callback preparation include the retained group metadata and complete projection cost.

Regional routing retains84 aligned exchange/source slots and42 optional version gates. The finite
2047 selection plans each hold at most84 route indices in one independently owned vector allocation;
its exact capacity/storage is checked and charged before publication. It is not placed on the native
worker stack. Complete catalog names remain bounded to128 units each and16384 total units.
Each desired revision additionally owns a logical snapshot and at most one suffix-free
physical string per logical symbol. Matching regional routes share that physical string;
Profile or composite fallback may simultaneously use the original logical copy.

For maximum symbol count `N`, maximum symbol units `U` and registered route count `R`,
the separate temporary allowance contains `2*N*U` text units, `N` routing wrappers,
`N*R` borrowed request wrappers and text terminators. Frontend-retained symbols and
queue-owned command copies keep their existing separate bounds. This is logical
backing, excluding unused implementation-selected capacity, allocator overhead and RSS.
The helper reserves the complete final string before copying borrowed prefix/attribute
slices, so no growing-prefix allocation overlaps the final physical buffer. Temporary backing and disposal
are conservatively charged before snapshot allocation. Routing work includes
`N*(2*R+8)+R` visits, `11*N*U` text inspections/copies and twice the temporary backing
plus the32-byte exchange array in byte work. Checked sums/products derive additional
per-endpoint grants for every logical subscription before publication. A fresh valid
grant can always fund this routing attempt plus the existing submission allowance;
overflow refuses at `profile-arithmetic`. Exhausted current-turn credits postpone the
revision without allocation, mutation or internal refill. Failed attempts do not refund.

Request symbol views are created only after the temporary selector vector is fully
populated and remain alive through synchronous `handle.replace`, which owns its
successful command copy. They never escape into listener captures or reconnect state.

Queue-owned command text separately reserves `N*R*(U+1)` units, including one terminator
per maximum-length symbol. Terminators do not reduce the public maximum of128 printable
ASCII units. Checked arithmetic refuses an unrepresentable reservation before allocation.

## Custom schema example

The caller supplies a validated dense catalog with an unrelated generic record0
and Quote-compatible record1 named `ConfiguredQuote`. Its fields can be reordered,
contain extra values and use explicit supported business aliases:

```cpp
const std::array routes{api::EventRegistration{api::EventType::QUOTE, 1}};
api::EventRegistryWork work;
auto registry = api::EventRegistry::create(schema, routes, {}, work);
```

The schema remains loader-independent. Existing native JSON or other source
adapters may supply it; registry construction does not require a loader. See
[schema-bound mapping](EVENT-MAPPING.md) and [ADR-0003](../../docs/decisions/ADR-0003-configurable-schemas-and-event-mappings.md).

## Compatibility evidence boundary

Pinned Java QD's [DXFeed.createSubscription](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeed.java)
single-class and class-array overloads separate typed selection from attachment.
[EventDelegateFactory.createDelegates(DataRecord)](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/EventDelegateFactory.java)
provides a per-record mapping boundary. The native registry reproduces explicit
schema-bound selection using existing measured adapters; it does not claim Java
reflection, arbitrary delegate factories, all event classes or new wire semantics.
The pinned reference and existing adapter provenance are in `upstream.lock` and
[configured event evidence](../../tests/fixtures/events/mapping/COVERAGE.md).

Trade is explicitly registered as EventType::TRADE; see its [complete contract](TRADE.md). The finite84-entry stack binding copy and initialization/read/write work are included in preparation. Quote/Trade/TradeETH/Summary select regional fallback independently; Profile preserves exact logical symbols. Derived Order uses literal physical symbols.

TradeETH is independently registered as EventType::TRADE_ETH; see [TradeETH](TRADE-ETH.md). Its raw ETH bit and lack of Tick fallback are distinct from ordinary Trade.

TextMessage and TextConfiguration preserve exact nonmarket symbols. See [their contract](TEXT-EVENTS.md) for STREAM and TICKER behavior.
