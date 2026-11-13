# Mixed registered event subscriptions

[Русская версия](MIXED-SUBSCRIPTIONS.ru.md). English is authoritative.

## Selection and publication

`Feed::createSubscription(std::span<const EventType>)` and its initializer-list
overload return one `Subscription<RegisteredEvent>`. `RegisteredEvent` is
`std::variant<events::Quote, events::Profile, events::Trade, events::TradeETH, events::Summary, events::TextMessage, events::TextConfiguration, events::Message, events::Configuration>`. Existing concrete
`createSubscription<events::Quote>()` and `createSubscription<events::Profile>()`
retain their listener types and behavior.

Creation copies and normalizes the complete type set. Duplicate entries coalesce;
input order does not order delivery. The caller can destroy or change its input
after return. The selected set remains fixed until this subscription closes.
Registered local record routes of each selected type participate according to the
[regional Quote exchange selection](EVENT-TIME.md). The
[immutable registry](EVENT-REGISTRY.md) retains complete caller schema identity,
dense local IDs, mapping options and generic records. Record names and wire sender
IDs do not select a C++ type.

Raw arguments are bounded by the positive runtime option
`maximumEventTypeArgumentsPerSubscription`, whose engineering default is 64.
This bounds inspected entries and work, including duplicates; it is independent
of the number of supported event kinds. `{QUOTE, PROFILE, QUOTE}` is accepted
within that bound. The same default remains meaningful if more kinds are added.
QUOTE, PROFILE, TRADE, TRADE_ETH, SUMMARY, TEXT_MESSAGE, TEXT_CONFIGURATION, MESSAGE and CONFIGURATION are supported. Selections containing TEXT_MESSAGE or MESSAGE require STREAM. Unknown enumerators are rejected explicitly.

Validation observes this order:

1. An oversized raw span rejects with `RESOURCE_LIMIT` at
   `subscription-event-type-arguments`, before inspecting any element.
2. Empty input or an unknown enumerator rejects with `INVALID_ARGUMENT` at
   `subscription-event-types`.
3. Closed endpoint/runtime admission rejects with the existing `CLOSED` category.
4. Any selected supported kind missing from the endpoint registry rejects with
   `UNSUPPORTED_PROFILE` at `subscription-unregistered-event`.
5. Exhausted subscription capacity rejects with `RESOURCE_LIMIT`.

Allocation failure can independently reject creation. A refusal publishes no
subscription, reserves no lasting slot and sends no partial desired selection.
One accepted mixed selection consumes one subscription slot and one shared
symbol set, rather than a slot per event kind.

## Execution, delivery and lifetime

The worker opens one existing receiver and one multiplexed registration for the
complete selected route span. Desired additions/removals contain one interest per
symbol and matching selected local record, using a regional physical base symbol where applicable.
Overlapping concrete or mixed subscriptions
share upstream interest through the existing multiplexer. Closing or clearing one
withdraws only interests no longer required by any active subscription.

Each admitted receiver batch is fully projected into owning variant values before
application listeners are called. A projection refusal uses the existing
subscription failure path; no partial projected batch is delivered. Ordering and
batch boundaries follow the selected existing TICKER/STREAM contract. A callback
need not contain all selected kinds, and a network frame does not define a
callback boundary. No extra cross-record ordering is introduced.

The listener's span is valid only during that invocation. Copies of a
`RegisteredEvent` own their event strings and values and remain usable after
registration, endpoint and runtime retirement. Application-retained copies are
outside library reservation accounting.

Worker serialization, selected-listener lifetime, capture retirement outside
control locks, callback exception isolation, creator-thread waits and detached
closed handles are the same as for concrete subscriptions. A listener exception
fails only its subscription with `CALLBACK_FAILURE`; overlapping subscribers can
continue. Awaiting endpoint closure establishes callback and capture quiescence.

## Finite receiving reservations

Profile preparation reserves the total number of registered routes for every
possible subscription. This supports creation after `connect()` without growing
native reservations. With `S` maximum subscriptions, `N` maximum symbols per
subscription and `R` registered routes, per-subscription desired capacity is
`N * max(1, R)` and endpoint desired capacity is `S * N * max(1, R)`, with checked
arithmetic and corresponding symbol-unit bounds. The default built-in registry
therefore reserves nine possible interests per symbol.

Complete mixed mapping limits, duplicate-route preparation visits and callback
projection allowances aggregate all selected routes before activation. Receiving
schema ceilings remain 16 records, 128 total fields, 256 lookups, 128 UTF-16 units
per name and 16384 total name units. Larger generic registries can be valid and
still receive explicit `UNSUPPORTED_PROFILE` before native activation. These
ceilings do not assert that a future full event catalog fits simultaneously.

## Mixed live endpoint

`DeliveryContract::MIXED` selects one live QTP session with three existing
collector families and the combined built-in catalog. `createCatalogSubscription()`
selects all built-in event types under one logical subscription. Regular typed
subscriptions and explicit type lists can share the same Feed.

The worker prepares each participating queue's desired state before committing
the logical revision. No network progress or callback runs between family
commits. Interest messages retain the existing family IDs and are written
sequentially. Recovery replays the committed desired state after reconnect.
A peer must advertise all three families before demand is sent. Public batches
remain per-family and carry no cross-family ordering guarantee.

## Primary compatibility reference

Pinned QD3.355
[DXFeed.createSubscription](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeed.java)
creates and attaches one subscription for a class array.
[DXFeedSubscription](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeedSubscription.java)
rejects an empty array and retains an immutable set with duplicates coalesced.
The native owning variant expresses type-safe delivery for the nine supported
prepared adapters; it does not add arbitrary application-class ABI, reflection,
wire messages, source families or schema loaders.

Trade also participates in RegisteredEvent and typed createSubscription<events::Trade>(). Supported selections have finite plans; fixed511 selection slots cover nonconsecutive native kinds without extra record routes. Trade regional fallback is independent of Quote; see [Trade](TRADE.md). Existing callback/ownership/error guarantees are unchanged.

TradeETH is independently registered as EventType::TRADE_ETH; see [TradeETH](TRADE-ETH.md). Its raw ETH bit and lack of Tick fallback are distinct from ordinary Trade.
