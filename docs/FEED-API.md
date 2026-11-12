# Public feed API {#feed-api}

[Русская версия](FEED-API.ru.md) · [Practical guides](guides.md) · [API reference](api.md)

Use `qd/api/feed.hpp` and link `qdcxx_feed` to receive typed `events::Quote`, `events::Profile`, `events::Trade`, `events::TradeETH`, `events::Summary`, `events::TextMessage`, `events::TextConfiguration`, `events::Message`, `events::Configuration`, `events::Order`, `events::MarketMaker`, `events::SpreadOrder`, `events::AnalyticOrder`, `events::OtcMarketsOrder`, `events::OrderImbalance` and `events::Series` through a library-owned worker. Applications create endpoints,
listeners and exact symbol subscriptions; the library progresses connection,
finite recovery, queues and delivery. Subscriptions can be created before or after
`connect()`.

This API supports plain binary QTP over TCP with built-in full Quote/Profile/Trade/TradeETH/Summary
and text/serialized-event schemas or explicit validated caller schemas and registered mappings.
An endpoint chooses TICKER (default, latest values with possible
conflation), STREAM (every admitted selected source record), or explicit HISTORY (indexed shared storage
and independent views). Ordinary Order, MarketMaker, SpreadOrder, AnalyticOrder, OtcMarketsOrder and Series
use HISTORY. Quote-derived Order and OrderImbalance use TICKER. The SDK owns both standard
catalogs, including regional A..Z sources. See [MarketMaker and derived Order](MARKET-MAKER-ORDER.md),
[order variants](ORDER-VARIANTS.md) and [OTC orders and auction imbalance](AUCTION-ORDER-EVENTS.md).
The default HISTORY registry supplies both DEFAULT variants. Use an explicit validated registry for
record variants such as `SpreadOrder#ISE` or `AnalyticOrder#NTV`, and select the exact source
with `SubscriptionSelector::indexed`. SpreadOrder selectors name the underlying instrument.
The same selector chooses a named OtcMarketsOrder or OrderImbalance source. A plain symbol
selects all available sources for that event type.
The existing nine nonindexed types retain their existing TICKER/STREAM profiles and reject HISTORY before native
registration. TLS and credentials
remain separate receiving profiles. Wildcard selectors require the existing explicit STREAM permission.
Schema loading is separate. The
[lower-level receiving APIs](SUBSCRIPTIONS.md) expose separate profiles.

Receive typed `events::TimeAndSale` with `createSubscription<events::TimeAndSale>()` on STREAM or
`createTimeSeriesSubscription<events::TimeAndSale>()` on HISTORY. Both accept composite symbols
such as `AAPL` and regional symbols such as `AAPL&Q`. See the [TimeAndSale contract](../spec/events/TIME-AND-SALE.md)
for route selection, time and correction flags.

## Receive Quote

First [build the enabled package](CPP-BUILD.md#public-feed-api-build). The
[complete console example](../examples/feed_quote/main.cpp) includes argument
validation, interruption, output errors, a finite duration and a no-data exit.
Its [build and usage guide](../examples/feed_quote/README.md) uses an installed package.

The central application sequence is:

```cpp
#include "qd/api/feed.hpp"

#include <chrono>
#include <iostream>
#include <stop_token>

int main() {
    namespace q = org::ttldtor::qdcxx;

    try {
        std::stop_source stop;
        q::api::ApiRuntime runtime;
        auto endpoint = runtime.createEndpoint();
        auto quotes = endpoint->getFeed()->createSubscription<q::events::Quote>();

        quotes->addEventListener([](std::span<const q::events::Quote> batch) {
            for (const auto &quote : batch) {
                std::cout << quote.bidPrice.toString() << '\n';
            }
        });
        quotes->addStateChangeListener([source = stop](auto, auto current) mutable {
            if (current == q::api::SubscriptionState::FAILED) {
                source.request_stop();
            }
        });
        quotes->addSymbols({"AAPL", "IBM"});
        endpoint->connect("demo.dxfeed.com:7300");

        const auto cutoff = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        static_cast<void>(endpoint->awaitState(q::api::EndpointState::CLOSED, cutoff, stop.get_token()));

        const auto closed = endpoint->closeAndAwaitTermination();
        runtime.closeAndAwaitTermination();

        if (closed.diagnostic.code != q::api::ErrorCode::NONE) {
            std::cerr << "Endpoint diagnostic=" << static_cast<int>(closed.diagnostic.code)
                      << " stage=" << closed.diagnostic.stage << " native=" << closed.diagnostic.nativeCode
                      << " detail=" << closed.diagnostic.detailCode << " drained=" << closed.drained
                      << " expired=" << closed.deadlineExpired << " gap=" << closed.hadGap << '\n';
        }

        return closed.drained && !closed.deadlineExpired && !closed.hadGap &&
                       quotes->getDiagnostic().code == q::api::ErrorCode::NONE
                   ? 0 : 1;
    } catch (const q::api::Error &error) {
        std::cerr << error.diagnostic().stage << '\n';
        return 1;
    }
}
```

The wait limits this application's receiving interval; it does not progress the
feed. This short sequence does not assert that any Quote arrived. Use the complete
example when a no-data result must fail. The demo address is an example input;
availability, subscriptions and delayed data depend on the external service.

For Profile, create `createSubscription<q::events::Profile>()` and use a listener
taking `std::span<const q::events::Profile>`. For STREAM, set
`EndpointOptions::contract = DeliveryContract::STREAM` before `createEndpoint(options)`.
The delivery contract applies to every subscription on that endpoint.

Set the typed `EndpointOptions::batchRecords` to cap whole physical records per callback selection
(default and maximum 4). One physical record can yield several business events. Set
`EndpointOptions::pendingRecords` to bound detached physical records per subscription (default and
maximum 128), including records selected for a callback. Both limits must be positive and
`batchRecords` cannot exceed `pendingRecords`. HISTORY source storage has its own limits.

STREAM preserves order and duplicates while capacity is available. If staging the next prefix
exhausts a subscription's reservation, the SDK rejects that prefix for the affected subscription and
reports the overflow through its state and statistics. TICKER can conflate pending values for the
same key before detached delivery. Use `getStatistics()` to distinguish conflation from overflow.

## Receive Greeks

`events::Greeks` provides option price, volatility, delta, gamma, theta, rho and vega.
Use `createSubscription<events::Greeks>()` on a TICKER endpoint for latest values.
On a HISTORY endpoint, use `createTimeSeriesSubscription<events::Greeks>()` and
set an inclusive `setFromTime()` bound in Unix milliseconds before adding symbols.

The indexed source is DEFAULT. The packed index retains the signed seconds and
millisecond/sequence word, and the event flags carry the HISTORY snapshot and
transaction state. Independent FIELD event time is available when sent by the
peer. Use `getHistorySnapshot()` for the last observed range coverage.
The built-in registry uses wide decimals. Select an explicit registry with the
peer's field types for a configured tiny-decimal schema.

## Receive Underlying

`events::Underlying` provides underlying, front and back volatility, call and put volume,
and the put-to-call ratio. Use `createSubscription<events::Underlying>()` on TICKER or
`createTimeSeriesSubscription<events::Underlying>()` with an inclusive `setFromTime()`
bound on HISTORY. The indexed source is DEFAULT.

Missing numerical fields return NaN. `getOptionVolume()` sums both available volumes
or returns the available side when one is missing. It returns NaN when both are missing.

## Receive TheoPrice

`events::TheoPrice` provides theoretical and underlying price, delta, gamma,
dividend and interest. Use `createSubscription<events::TheoPrice>()` on TICKER,
or `createTimeSeriesSubscription<events::TheoPrice>()` on HISTORY. Set the
inclusive `setFromTime()` bound in milliseconds on the HISTORY subscription.

Absent optional dividend and interest values return NaN. A missing optional
Sequence maps to zero in a non-indexed configured schema. HISTORY requires a
physical Time and Sequence pair for the raw range coordinate. After reconnect,
the endpoint replays the accepted range on the fresh connection.

## Receive Trade

The built-in registry appends composite Trade2 while retaining Quote0/Profile1. Use owning
`events::Trade` and its Java business getters: LONG can differ from wide-to-double by one ULP.
For a real regional Trade&Q, supply an exported registry with that `EventType::TRADE` route.
Logical AAPL&Q then submits physical(Trade&Q,AAPL); renaming an already decoded generic symbol
does not establish regional delivery. Complete fields/types/defaults, flags, packed time and the
explicit one-slot TradeId boundary are specified in [Trade](../spec/events/TRADE.md).

```cpp
auto trades = endpoint->getFeed()->createSubscription<q::events::Trade>();
trades->addEventListener([](std::span<const q::events::Trade> batch) {
    for (const auto &trade : batch) {
        std::cout << trade.getTime() << ' ' << trade.getPrice() << ' '
                  << trade.getSizeAsDouble() << '\n';
    }
});
trades->addSymbols({"AAPL"});
```

The runtime/endpoint provide SDK progress; the application does not poll queues or work budgets.
A Trade copy owns its symbol and inline snapshot; the callback span lasts only during invocation.
Mixed subscriptions accept nonempty sets of registered types, subject to the selected delivery contract; the complete receiving-schema ceiling remains16 records.

## Explicit event registry

Defaults need no registry setup. For a validated caller `schema`, register explicit
record routes before creating an endpoint:

```cpp
const std::array routes{q::api::EventRegistration{q::api::EventType::QUOTE, 1},
                        q::api::EventRegistration{q::api::EventType::QUOTE, 2}};
q::api::EventRegistryWork work;
auto registry = q::api::EventRegistry::create(schema, routes, {}, work);

if (!registry) {
    // Inspect the typed cause and optional registrationIndex before continuing.
    return 1;
}

q::api::EndpointOptions options;
options.eventRegistry = std::move(*registry);
auto endpoint = runtime.createEndpoint(options);
auto quotes = endpoint->getFeed()->createSubscription<q::events::Quote>();
```

The advanced snippet assumes an existing runtime and includes `<array>` and
`<utility>` in addition to `qd/api/feed.hpp`. Local record IDs remain dense catalog
positions; the example requires compatible records 1/2 after record 0. Record names
do not select an event class. Existing adapters resolve supported explicit business
lookup aliases and exact types, preserving reordered fields, extra fields and
generic records. Registered composite records of the requested type participate;
regional Quote routes use the exchange selection below.
Duplicate local-ID routes reject, and an unregistered type rejects synchronously.
For several types in one logical subscription, use the overload below.

The endpoint copies an immutable owning registry snapshot. Later assignment or
destruction of caller schema/registry/options cannot replace it. Construction has
explicit finite schema, route, allocation-request and work limits; it invokes no
application callback, loader or network. The receiving profile supports at most
42 records, 284 total fields, 724 lookups, 128 UTF-16 units per name and 16384 total
name units. A registry valid under larger bounds is refused at `profile-schema`
before native activation; it is never truncated. See the complete
[registry ownership, errors and bounds contract](../spec/events/EVENT-REGISTRY.md).

Prepared Quote/Profile/Trade/TradeETH/Summary/Message/Configuration use the exhaustive [field conversion rules](../spec/events/FIELD-CONVERSIONS.md):
supported numbers retain WideDecimal fields and expose direct Java business getters such as
`quote.getBidPrice()`/`profile.getBeta()` without LONG-to-wide rounding. Raw int
properties preserve Java physical-slot behavior, and UTF_CHAR_ARRAY/STRING preserve nullable
UTF-16 text. Wrong present types are refused during registry preparation. Default registrations
allow 128 type alternatives; custom lower bounds remain explicit resource limits.

For regional Quote delivery, supply the actual validated regional record declaration
and register it as `EventType::QUOTE`. For example, when the selected schema contains
`Quote&Q`, subscribing with ordinary `quotes->addSymbols("AAPL&Q")` sends the physical
`Quote&Q`/`AAPL` interest. The received owning Quote has symbol `AAPL&Q`; absent exchange
fields use the record's suffix. Any registered exchange follows the same rule.
Composite and regional interests stay distinct in a mixed subscription. A matching
regional route is selected per event kind; otherwise composite routes keep the exact
logical symbol. Trailing attributes survive conversion. Remove and explicit reconnect
use the same selected record and physical symbol.

A selected regional route must restore the exact logical spelling. Noninvertible forms
such as `AAPL&N&Q` or `AAPL{x=y}&Q` throw `INVALID_ARGUMENT` at
`subscription-symbol-representation` from `addSymbols`/`setSymbols`; the complete mutation
is refused and the previous desired set remains. Composite fallback keeps these literals
unchanged. The ordinary attributed form `AAPL&Q{x=y}` is representable.

The built-in registry includes composite Quote/Profile/Trade/TradeETH/Summary and both text events. This explicit bounded registry
path does not load the complete regional catalog or implement broader selectors.
The [time and variant contract](../spec/events/EVENT-TIME.md) specifies optional
Sequence/nanoseconds, Quote seconds/milliseconds, Profile halt seconds, independent
event metadata and the deterministic local-wire evidence boundary. It makes no new
public-service availability claim.

## Receive several event types

The following fragment assumes an existing endpoint and includes `<type_traits>`:

```cpp
auto mixed = endpoint->getFeed()->createSubscription(
    {q::api::EventType::QUOTE, q::api::EventType::PROFILE, q::api::EventType::QUOTE});
mixed->addEventListener([](std::span<const q::api::RegisteredEvent> batch) {
    for (const auto &event : batch) {
        std::visit([](const auto &value) {
            using Event = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Event, q::events::Quote>) {
                // Read the owning Quote value.
            } else {
                // Read the owning Profile value.
            }
            (void)value;
        }, event);
    }
});
mixed->addSymbols({"AAPL", "IBM"});
```

This creates one `Subscription<RegisteredEvent>` and one symbol set. Repeated types
coalesce; registered routes of each selected type use the exchange selection above. The type set
is copied and remains fixed for that subscription. A runtime array can use the
`std::span<const EventType>` overload. An empty list or unknown enum rejects with
`INVALID_ARGUMENT`; a supported type absent from the endpoint registry rejects
with `UNSUPPORTED_PROFILE`, without publishing a subscription.

`RegisteredEvent` is an owning variant of all supported registered event types, including TextMessage and TextConfiguration.
Copy a value to retain it beyond its callback span. Concrete subscriptions and
mixed subscriptions can overlap independently; closing one withdraws only its
remaining exclusive interests. Callback batches follow queue delivery boundaries
and need not combine all selected types. See the complete
[mixed selection and delivery contract](../spec/events/MIXED-SUBSCRIPTIONS.md).

## Runtime, callbacks and shutdown

Create the owning `ApiRuntime(RuntimeOptions)` on main **before application threads**;
those threads must retire before its destruction. A host that already created
`RuntimeBootstrap` on main before starting threads may later use the explicit
borrowing constructor on that same creator thread, while retaining the host owner.
Both forms retain their bootstrap lease until the runtime object is destroyed,
including after close. The worker starts lazily on the first accepted `connect()`
and is shared by that runtime's endpoints. Coordinate every user of the same
c-ares state. See the [embedding and ownership contract](../spec/transport/RUNTIME-EMBEDDING.md).

Destroy the runtime on its creating thread. It is neither copyable nor movable.
`Endpoint::closeAndAwaitTermination()` and `ApiRuntime::closeAndAwaitTermination()`
are restricted to that creating thread; another thread, including a listener,
receives `ErrorCode::WRONG_THREAD` before waiting. Violating the runtime destructor's
thread precondition terminates the process.

Await reentered from a retiring listener capture's destructor (including cycles
between endpoints), or runtime await during capture retirement or recursive shutdown,
rejects with `ErrorCode::INVALID_STATE`.

Endpoint and subscription producer methods are safe to call from listeners.
`close()` requests shutdown without waiting and cannot be blocked by ordinary
capacity exhaustion. Releasing endpoint/subscription handles requests close;
retirement remains with the runtime. Handles may survive the runtime as closed,
detached ports and do not keep a native worker alive.

After quiescence, state/diagnostic queries and final release of a terminal handle
are permitted on another thread. `awaitState` remains creator-affine even when its
terminal target is already reached; a closed subscription refuses new admission
with `CLOSED`. These handles retain no SDK lease and cannot resume native work
after runtime destruction. A retained native failure stays `FAILED` with its
original diagnostic rather than becoming a successful `CLOSED`.

Event listeners run serially per subscription, on the SDK worker by default. An explicit
[event executor policy](CALLBACK-EXECUTION.md) can borrow application workers while preserving serial
selection, actual completion accounting and source-epoch fences. Their span expires when that invocation
returns; copy an event to retain its owning strings and values. Such application
copies do not retain a queue or runtime, and application retention is outside the
library's finite reservations. Keep callbacks short: a blocked callback delays
other endpoints and shutdown. Neither drain deadlines nor destructors can preempt
application code or promise a hard OS-retirement deadline.

An event-listener exception fails that subscription with `CALLBACK_FAILURE`; it is not
retried and does not fail other subscriptions. Removing a listener prevents future
selection; a callback already selected may finish. Listener captures retire outside
control locks. Awaiting endpoint closure establishes callback/capture quiescence.

`CloseResult::drained` describes the actual admitted local prefix. Inspect
`deadlineExpired`, `hadGap` and `diagnostic` as well. It does not establish peer
acknowledgement, unread-wire completeness or an uninterrupted upstream history.

## State observers

Register endpoint and subscription state observers before the transitions you need:

```cpp
const auto listener = endpoint->addStateChangeListener(
    [](q::api::EndpointState previous, q::api::EndpointState current) {
        // Observe the transition; keep this callback short.
        (void)previous;
        (void)current;
    });
endpoint->removeStateChangeListener(listener);
```

The same methods on `Subscription<Event>` use `SubscriptionState`. Registration
makes **no initial callback**; `getState()` is a separate current-state snapshot.
Observers receive distinct actual old/new transitions in publication order, including
failure and terminal states. One complete selection is made atomically with each
transition and invoked in registration order outside control locks. Removing an
observer prevents later selections; an already selected observer can still run.
Removal is idempotent and remains available after close. Adding after the close
admission fence rejects with `CLOSED`; ordinary capacity cannot block close.

Calls run on the SDK worker after that runtime's first `connect()`. If the runtime
has never started its worker, creator-thread endpoint/runtime retirement invokes
state observers without starting one. A callback can inspect state, close, remove
observers or operate on another endpoint. Worker awaits reject with `WRONG_THREAD`;
creator-retirement reentry into that endpoint's await or runtime await rejects with
`INVALID_STATE`. Different inactive endpoints can be retired recursively; an
inactive creator retirement and another active endpoint's worker can overlap.

State observers are advisory. If one throws, the complete selected snapshot still
runs. `getStateChangeDiagnostic()` retains the first static `CALLBACK_FAILURE`
separately; owner state, `getDiagnostic()` and `CloseResult` do not change. `CLOSED`
and `FAILED` remain absorbing. As before, `CLOSED` proves native/registration and
all **event-listener** captures have retired. Only these new opt-in state observers
can still be executing or releasing captures; endpoint await includes both event
and state-observer retirement before returning or releasing a runtime slot.

Separate positive `maximumStateListenersPerEndpoint` and
`maximumStateListenersPerSubscription` limits count accepted, selected and retiring
observer boxes. Removal releases capacity only after the final selected invocation
and application capture destruction. No state queue grows with transitions: the
first producer-published `CONNECTING` selection has one deferred slot, and later
observations are dispatched directly by the owning execution domain. A synchronous
worker-start refusal publishes no transition and invokes no observer on the producer.
See the [complete state-observer contract](../spec/subscriptions/FEED-STATES.md).

## Accepted changes and errors

`addSymbols`, `removeSymbols`, `setSymbols` and `clearSymbols` accept bounded desired
state asynchronously. Duplicates coalesce; `setSymbols({})` withdraws all interests.
Return from a producer call does not mean wire ADD/REMOVE or peer acknowledgement.
Each subscription has independent interests and delivery, including overlapping symbols.

Synchronous rejection throws `api::Error`, whose `diagnostic()` is detached static
and numeric evidence. Use `getState()` and `getDiagnostic()` on endpoints and
subscriptions for the latest committed lifecycle and first asynchronous failure.
There is no application polling requirement for connection or delivery progress.

`connect()` accepts ASCII `host:port` or `[IPv6]:port`. The first accepted address is
fixed; repeating it while logically online is a no-op. After `disconnect()`, the
same address resumes with a fresh source epoch. Another address, or connecting a closed
or failed endpoint, rejects. Automatic finite recovery uses the endpoint policy.
Selectors are nonempty exact printable ASCII, within the runtime's symbol limit,
without a leading `*` or wildcard/All syntax.

`disconnect()` immediately reports `NOT_CONNECTED`, retaining desired interests and
projected cache. After physical pause quiescence, new/changed subscriptions may
receive that retained cache. `reconnect()` requests a fresh epoch while online;
after explicit disconnect it does nothing. `disconnectAndClear()` immediately
fences new stale selections and requests asynchronous real cache clear, including
while already disconnected. `NOT_CONNECTED` is not a clear-completion barrier.
An already selected listener snapshot can finish; owning event copies remain valid.
These controls preserve finite lifetime attempts/work. See the
[complete lifecycle and admission contract](../spec/subscriptions/FEED-LIFECYCLE.md).
## Finite defaults

Configure `RuntimeOptions` before constructing the runtime and `EndpointOptions`
before creating an endpoint. Counts include pending, active and retiring entries;
they are logical reservations, not process RSS limits. Increasing counts can require
substantial backing storage, and unsupported capacity arithmetic rejects explicitly.

| Runtime option | Default |
| --- | --- |
| `maximumEndpoints` | 4 |
| `maximumSubscriptionsPerEndpoint` | 8 |
| `maximumSymbolsPerSubscription` | 16 |
| `maximumListenersPerSubscription` | 4 |
| `maximumStateListenersPerEndpoint` | 4 |
| `maximumStateListenersPerSubscription` | 4 |
| `maximumEventTypeArgumentsPerSubscription` | 64 raw type arguments, including duplicates; positive |
| `maximumPendingStateChangesPerEndpoint` | 8; pending/executing/retiring selections |
| `maximumSymbolUnits` | 128; configurable in 1..128 |
| `dnsServers` | Empty: discover up to 4 system IPv4 resolver addresses |
| `dnsPort` | 53 |

| Endpoint option | Default |
| --- | --- |
| `contract` | `TICKER` |
| `eventRegistry` | Absent: built-in Quote0/Profile1/Trade2/TradeETH3/Summary4/TextMessage5/TextConfiguration6; an explicit value is copied as an immutable snapshot |
| `connectTimeout`, `dnsTimeout`, `writeTimeout` | 5000 ms each |
| `readTimeout` | 30000 ms |
| `pollInterval` | 1 ms |
| `maximumAttempts` | 3 total, including the initial attempt |
| `retryDelay` | 250 ms |
| `drainTimeout` | 1000 ms |

Counts must be positive. Endpoint deadlines/cadence are positive milliseconds up
to `INT_MAX`; retry delay must be positive when attempts exceed one. A finite drain
allowance can expire and escalate to abort; it is not a hard blocking-await timeout.

## DNS configuration

An empty `RuntimeOptions::dnsServers` reads a bounded system snapshot at runtime
construction: Windows `GetNetworkParams`, Linux `/etc/resolv.conf`. Up to four unique
usable IPv4 resolver addresses are retained in platform order. Discovery failure
or no usable IPv4 server raises `DNS_CONFIGURATION`; no public resolver is substituted.
This snapshot is taken even when a later endpoint uses a numeric route. Numeric
endpoints bypass lookup after construction.

For explicit resolvers, populate `dnsServers` with one to four nonzero numeric
`transport::DnsServer` IPv4 addresses and optionally set `dnsPort` (1..65535).
For example, with a resolver address supplied by your network configuration:

```cpp
q::api::RuntimeOptions options;
options.dnsServers.push_back(q::transport::DnsServer{{192, 0, 2, 53}});
q::api::ApiRuntime runtime(options);
```

`192.0.2.53` is a documentation placeholder; replace it with your actual resolver.
This option chooses the DNS server transport; it does not enable TLS or change the
endpoint's exact symbol/delivery policy.

## Statistics

The [layered statistics contract](../spec/subscriptions/CLIENT-STATISTICS.md) defines
the endpoint/subscription counters below, epoch and availability rules. Rate and
baseline helpers remain planned; lifecycle diagnostics stay independent.

## Wait for a current state

`endpoint->awaitState(target, deadline, stopToken)` and
`subscription->awaitState(target, deadline, stopToken)` block only the runtime's
creator, without polling or native progress. `StateWaitResult<State>` returns the
current state and first lifecycle diagnostic. Conditions have explicit precedence:
`REACHED` target, a different absorbing `TERMINAL` (`CLOSED`/`FAILED`), `CANCELLED`,
then `DEADLINE_EXPIRED`. The default deadline is steady-clock `max()` and the
default stop token is empty. A past transition is not remembered.

Foreign/SDK callback calls reject with `api::Error` / `WRONG_THREAD`; any creator
callback/capture-retirement context rejects `INVALID_STATE`, even for a different
owner or already matching state. Invalid enum values reject `INVALID_ARGUMENT`.
Checks happen before the ready-result shortcut. State waits have no lifecycle side
effects and do not consume observer capacity. `awaitState(CLOSED)` can return while
opt-in state observers still retire; use `closeAndAwaitTermination()` for full
quiescence. See [wait results, cancellation and signal lifetime](../spec/subscriptions/FEED-WAITS.md).

## Receive TradeETH

Create `createSubscription<q::events::TradeETH>()` and use getPrice()/getSizeAsDouble() as in the Trade
example. Builtin TradeETH3 is composite; explicit TRADE_ETH registrations enable actual TradeETH&Q.
The ETH bit may be false during an RTH transition. Nano uses Last.TimeNanoPart and there is no Tick
fallback. See [TradeETH](../spec/events/TRADE-ETH.md) for complete fields, defaults and finite limits.

## Receive Summary

Create `createSubscription<q::events::Summary>()` and use getDayOpenPrice()/getPrevDayVolume() for
source business doubles and getOpenInterest() for full mapping-long OI, correcting the pinned
delegate narrowing loss under [ADR-0010](decisions/ADR-0010-summary-open-interest-width.md). Builtin Summary4 is composite; an explicit SUMMARY registration enables actual Summary&Q.
Day IDs are signed days, not business timestamps. PriceType comes from complete flags.
See [Summary](../spec/events/SUMMARY.md) for all fields, defaults and regional OpenInterest absence.

## Explicit decorated selectors

Owning `SubscriptionSelector` and `addSelectors`/`setSelectors`/`removeSelector` preserve typed kind/source identity separately from physical demand. Quote/Profile/Trade/TradeETH/Summary ignore IES source ID as production Java does. Order routes explicit source IDs to registered Order records; plain and negative-source selectors select the available source union. See [indexed Order reception](ORDER.md). Typed Feed TSS is atomically unsupported. `EndpointOptions::enableWildcard=true` permits explicit All only for STREAM and is disabled by default. Literal leading-star text does not enable wildcard; existing string/Candle APIs remain plain exact. [Complete contract, example, finite storage and known-binding source adapter and time value conversion](../spec/subscriptions/DECORATED-SELECTORS.md).

TextMessage requires a STREAM endpoint. TextConfiguration uses STREAM or TICKER; its nullable owning text and version policy are specified in [Text events](../spec/events/TEXT-EVENTS.md).

## Accepted subscription observations and query classifiers

`getSnapshot()` copies one complete accepted desired revision, exact typed logical selectors and the immutable normalized event-type set. The result survives mutation and runtime retirement. `getEventTypes()` and `containsEventType()` inspect that immutable set. An accepted revision is separate from worker application, physical demand and peer acknowledgement.

`addChangeListener()` reports initial nonempty state and committed REMOVED/ADDED phases. Replacement removes before adding at one revision. Callbacks borrow their arguments; copy `change.snapshot` or its selectors to retain independent values. They run outside control locks. Reentry and concurrent calls use finite after-return continuation, including between runtimes and from a complete selected data callback. No caller polling, queue management or network progress is required.

Observer exceptions are advisory through `getChangeDiagnostic()`; event-listener failure policy is unchanged. `close()` remains a nonblocking fence. Actual worker retirement or creator `closeAndAwaitTermination()` delivers terminal CLOSED and includes selected callbacks and capture destruction. Read the exact [observation/lifetime/budget contract](../spec/subscriptions/OBSERVABLE-SUBSCRIPTIONS.md) before depending on native timing differences from Java.

`Endpoint::createFilter(orderedCategories, context)` builds an independent classifier using the endpoint's actual registry and delivery contract; an advanced builder also accepts an explicit registry and finite limits. It changes no subscription demand. The optional context is supplied once for explicit resource capabilities. [Filter queries, categories, trackers and IPF source ownership](FEED-FILTERS.md) describe their separate lifetimes and optional backend.

The installed/relocated [public consumer](../tests/conformance/subscription_observer_consumer/CMakeLists.txt) builds only against `find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)` and `qdcxx::feed`. It includes each public header independently and exercises detached snapshots, callback phases, classifier queries, refusal and offline retirement without private headers or source paths.

## Endpoint statistics

`endpoint->getStatistics()` copies the last coherent publication into an owning
`EndpointStatistics`. It can be read concurrently from producer threads and after
runtime shutdown. `ownerIdentity` retains only an opaque bounded token; compare it
for equality, without interpreting its address. Distinct owners cannot alias while
an old observation survives. `generation` stays one for this owner lifetime.
`observedAt` is the steady-clock publication time, rather than the time of the read.
Reads allocate no storage, perform no IO/DNS, dispatch no callbacks and reset no work.
A blocked worker callback delays publication; readers keep seeing the preceding cut.

The `connection` fields separate physical native IO from protocol completion:
`readBytes`/`writtenBytes` count positive OS receive/send completion octets, including
partial transfers and subsequently discarded input. Plain Feed TCP carries QTP stream
octets. A complete structurally accepted incoming frame counts before payload
processing, so a rejected payload still contributes one `readMessages`. DATA counts
only after whole-payload validation and event construction, including wire records
skipped by local schema binding, before projection or subscription fan-out. Outgoing
messages and ADD/REMOVE records count only when the last frame byte is accepted by
the socket. Queued output and desired selectors contribute nothing; real replay counts
again. Socket acceptance does not prove peer acknowledgement. If an interrupted
Windows write has an unreported prefix, sticky `writeAcceptanceUnknown` identifies
the reported byte count as a lower bound; complete-frame counts cover reported progress.

`connectionAttempts` counts actual recovery factory entries, including DNS/connect
failures. `attemptIdentity` is that owner's current factory ordinal, absent without
an attempt. `sourceEpoch` is an established source ordinal, absent for an unestablished
attempt and after retirement. Within unsaturated totals, `establishedConnections =
closedConnections + activeConnections`; protocol readiness is a separate state.
Reconnect changes the source, preserving lifetime totals and generation. Baselines
are independent consumer copies; there is no destructive reset or rate helper.

Each `StatisticCounter` carries its own `availability`. Available idle counts are
zero; overflow saturates at `UINT64_MAX` with sticky `SATURATED`, disabling exact
deltas for that field. Incoming subscriptions, outgoing DATA and QD lag/RTT currently
report `UNSUPPORTED`. No event timestamp or callback duration substitutes for wire
timing. This accessor instruments the existing plain Feed path; a future Feed TLS
path must separately identify encrypted transport and plaintext QTP boundaries.

Native totals are preserved after all owned completions return. `final` becomes true
only after native, listener and callable-capture quiescence, including subscription-change
and state observers; totals and timestamp then freeze. CLOSED alone is not that barrier. A saved
snapshot remains detached from sockets, schemas, callbacks and runtime owners.
See the complete [layered contract](../spec/subscriptions/CLIENT-STATISTICS.md).

[Endpoint configuration](ENDPOINT-CONFIGURATION.md): explicit addresses/properties, immutable snapshots, effective local values and Java compatibility boundaries.

## Subscription statistics

`subscription->getStatistics()` returns a detached `SubscriptionStatistics` under
one subscription-local accounting mutex. Actual collector/FIFO admission, selection,
cleanup and user invocation transitions update this same owner. Reads allocate no
storage and perform no IO, dispatch, reset or callback wait. User callbacks run outside
the accounting and control locks. `ownerIdentity`, `generation`, `observedAt` and `final`
have the endpoint observation rules above; endpoint and subscription reads are separate
cuts. `sourceEpoch` is the latest observed established endpoint source, absent when
inactive; lifetime totals and old pending work survive source changes without retagging
their internal origin. Final publication waits for event, state and change-observer
captures to retire, including their destructors. Values and timestamp then freeze.

`desiredSelectors` counts accepted logical selectors once, independently of selected
event types, qualified by `desiredRevision`. `activeSelectors` is published only after
the complete actual source mapping commits, qualified by `activeDesiredRevision` and
the observed `sourceEpoch`. A pending/failed mapping does not claim the next command
active; a subsequent submission cannot relabel an older mapping candidate. Unchanged
commands preserve the physical mapping and publish their own logical qualification.
An actually cleared/reset mapping and retirement have zero active selectors and no
active revision. This is local application, not peer acknowledgement.
`queueCapacityEvents` is the actual reserved FIFO capacity plus TICKER view pending slots,
zero before native registration. It describes slots, not free capacity or process memory.
`pendingEvents` includes source-view pending values and detached FIFO events;
`inFlightEvents` covers the complete selected adapter call, including blocked listeners.

Before saturation, the lifetime conservation rule is:

```text
admittedEvents = deliveredEvents + conflatedEvents + discardedEvents +
                 callbackFailedEvents + evictedOverflowEvents + pendingEvents + inFlightEvents
droppedEvents = rejectedOverflowEvents + evictedOverflowEvents
```

One occurrence matching two subscriptions is admitted once by each. Listener fanout
does not multiply admission or delivered events. `deliveredEvents` requires normal
completion of the whole selected listener set. No listeners, projection refusal,
source/clear fencing and cancellation retire admitted events as `discardedEvents`.
A throwing listener charges the whole selected batch once to `callbackFailedEvents`,
including events seen by an earlier listener; remaining queued events are discarded.
Close may suppress not-yet-entered listeners without claiming completed delivery.

`droppedEvents` counts known overflow only. STREAM rejects incoming per-registration
FIFO candidates; TICKER can retire already admitted source-view candidates as overflow
victims. Superseded pending TICKER values count as `conflatedEvents`, never as dropped.
Equal/filtered input and unclassified shared STREAM source loss are not fabricated as
subscription admissions or drops; the existing gap/unknown-eligibility diagnostics
remain authoritative for such unmaterialized loss.

`callbackCalls`/`callbackFailures` count actual event-listener invocations/throws.
`callbackDurationSamples`, `callbackDurationTotal` and `callbackDurationMaximum` measure
completed invocations with the steady clock, in nanoseconds, including throws and
blocking; queue wait and state/change observers are excluded. Timing has `NO_SAMPLE`
until the first call finishes. Count and sum overflow saturate per field with the
existing sticky availability marker; reads and reconnect never reset totals.

[Server aggregation](SERVER-AGGREGATION.md): separate local requests and current-peer reports, bounded descriptor updates and explicit unknown states.

## Indexed option series

Use `Feed::createSubscription<events::Series>()` on a HISTORY endpoint. The default
HISTORY registry includes `Series`. A plain symbol and the indexed DEFAULT selector
share one exact physical demand. A positive source ID selects no Series record.
Series retains the original symbol, including exchange-looking suffixes. Its business
time does not set a subscription range. See [Series fields and schema](SERIES-EVENT.md).

## HISTORY range observations

`Feed::createTimeSeriesSubscription<Event>()` uses the same owning subscription facade with
`getFromTime()`/`setFromTime(ms)` and an initial signed64 MAX. A verified native time-series mapper
and an explicit HISTORY endpoint are required. The current event types reject this operation before registration.
Ordinary subscriptions reject range operations. Accepted range/set/revision and observer snapshots
change together; the SDK applies demand and withdraws obsolete not-started HISTORY delivery.
See [live HISTORY ranges](../spec/subscriptions/LIVE-HISTORY-RANGES.md) for capability, callback
BEGIN, finite work, overlap, output refusal and recovery boundaries.

Select `DeliveryContract::HISTORY` explicitly. The worker owns the collector, independent views, replay and reset acknowledgements. `Subscription<Event>::getHistorySnapshot()` returns a passive owning AS-OF cut; reads perform no I/O or callback waiting. Current completion requires both actual delivered range evidence and matching accepted logical/source/clear/callback fences. The default SDK HISTORY registry includes the indexed event families documented above. Types without a HISTORY route are rejected before native registration. See [Order fields, source selectors and ownership](ORDER.md) and [MarketMaker and derived Order](MARKET-MAKER-ORDER.md). See the [HISTORY contract](../spec/subscriptions/HISTORY-RECEIVING.md) for exact states, finite storage limits and raw indexed range units.

## Receive Candle

Use `createSubscription<Candle>()` on a TICKER endpoint for the latest received value, or `createTimeSeriesSubscription<Candle>()` on a HISTORY endpoint for a ranged snapshot and subsequent changes. Pass a `CandleSymbol` to `addSymbols()` and set the HISTORY range with `setFromTime()`. The callback owns a copyable Candle value whose event symbol uses the logical `CandleSymbol` requested by the application.

The default receiving registry supports the standard `Candle` and `TradeHistory` wire records. A custom registry can select other supported physical variants. Symbol attributes select a compatible record before the subscription changes. Receiving Candle does not compute candles from trades or fill missing periods. See [Candle routing](../spec/events/CANDLE-ROUTES.md) for the physical record rules and [Candle payload](../spec/events/CANDLE-PAYLOAD.md) for field semantics.

## Receive OptionSale

Use `Feed::createSubscription<events::OptionSale>()` on a HISTORY endpoint. See [OptionSale fields and indexed identity](OPTION-SALE-EVENT.md).

Receive NuamTrade using `createSubscription<events::NuamTrade>()` on a TICKER or STREAM endpoint.
The event extends Trade with independently updated exchange statistics. See the
[NuamTrade fields and routing](../spec/events/NUAM-TRADE.md).

Receive `NuamTimeAndSale` on STREAM or HISTORY using the typed Feed API. It extends `TimeAndSale` with the matching engine `matchId`; HISTORY retains its indexed transaction and removal flags. The default route is composite; regional routes require an explicit registry.

## Read the latest retained event

`Feed::getLastEvent(quote)` returns an independent copy of the most recent committed
TICKER value for the event type and symbol. The supplied event is returned unchanged
when the cache has no current value. Use `getLastEventIfSubscribed<events::Quote>("IBM")`
to distinguish absence of an exact subscription from a subscribed symbol awaiting
data. In the latter case the result has the requested symbol and default-initialized
C++ fields. For example, the regional Quote's bid and ask exchange fields are zero
until a source value is retained. A wildcard does not count as an exact subscription.
STREAM and HISTORY endpoints have no result for these reads.

The read inspects the actual collector, even when there are no listeners or when
aggregation or a custom executor delays delivery. A plain disconnect keeps the
cache readable once the source has quiesced. Accepted clear, reconnect, removal
or close immediately fence affected values. A returned copy remains usable after
the cache or endpoint closes.

Reads use the existing local runtime owner and never start a worker, connect,
change demand or send a request to the source. A call from that runtime's worker
listener reads directly without waiting. An external call reserves one bounded
request per endpoint and waits at most one second for completion. Concurrent
occupation of that slot reports `ErrorCode::RESOURCE_LIMIT`; an expired wait reports
`ErrorCode::WORK_LIMIT`. A timed-out request stays charged until the owner retires it.

The typed read accepts supported LastingEvent values, including regional symbols
and Candle's canonical symbol. Other event types report `UNSUPPORTED_PROFILE`.
For an exact Candle subscription, pass its parsed `CandleSymbol` to the same
`getLastEventIfSubscribed<events::Candle>` method.

## Local aggregation

Set `EndpointOptions::aggregationPeriod` or the `dxfeed.aggregationPeriod` configuration property for a local endpoint floor. `Subscription::setAggregationPeriod` sets an optional per-subscription period. The effective local period is their maximum and takes effect after the running interval. `Subscription::getAggregationPeriodInfo` reports the local period selected for the last actual listener invocation, or no value before one. `Endpoint::getAggregationPeriodInfo` separately reports an upstream descriptor.

A TICKER subscription retains waiting updates in its collector so repeated values of one key may conflate. STREAM and HISTORY use bounded ordered queues and can report their existing overflow outcome. During a running interval, TICKER initial collector values remain eligible ahead of pending live updates. Already detached queue entries retain their original order. Closing drains admitted events without waiting for the local interval. Zero preserves immediate delivery.
