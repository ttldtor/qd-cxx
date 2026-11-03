# Public feed API {#feed-api}

[Русская версия](FEED-API.ru.md) · [Practical guides](guides.md) · [API reference](api.md)

Use `qd/api/feed.hpp` and link `qdcxx_feed` to receive typed `events::Quote` and
`events::Profile` through a library-owned worker. Applications create endpoints,
listeners and exact symbol subscriptions; the library progresses connection,
finite recovery, queues and delivery. Subscriptions can be created before or after
`connect()`.

This API supports plain binary QTP over TCP with built-in full Quote/Profile
schemas or explicit validated caller schemas and registered Quote/Profile mappings.
An endpoint chooses TICKER (default, latest values with possible
conflation) or STREAM (every admitted selected source record). TLS, credentials,
HISTORY and wildcard selectors are outside this API. Schema loading is separate. The
[lower-level receiving APIs](SUBSCRIPTIONS.md) expose separate profiles.

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
generic records. Every registered record of the requested type participates;
duplicate local-ID routes reject, and an unregistered type rejects synchronously.
For several types in one logical subscription, use the overload below.

The endpoint copies an immutable owning registry snapshot. Later assignment or
destruction of caller schema/registry/options cannot replace it. Construction has
explicit finite schema, route, allocation-request and work limits; it invokes no
application callback, loader or network. The receiving profile supports at most
16 records, 128 total fields, 256 lookups, 128 UTF-16 units per name and 16384 total
name units. A registry valid under larger bounds is refused at `profile-schema`
before native activation; it is never truncated. See the complete
[registry ownership, errors and bounds contract](../spec/events/EVENT-REGISTRY.md).

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
coalesce; every registered route of each selected type participates. The type set
is copied and remains fixed for that subscription. A runtime array can use the
`std::span<const EventType>` overload. An empty list or unknown enum rejects with
`INVALID_ARGUMENT`; a supported type absent from the endpoint registry rejects
with `UNSUPPORTED_PROFILE`, without publishing a subscription.

`RegisteredEvent` is an owning `std::variant<events::Quote, events::Profile>`.
Copy a value to retain it beyond its callback span. Concrete subscriptions and
mixed subscriptions can overlap independently; closing one withdraws only its
remaining exclusive interests. Callback batches follow queue delivery boundaries
and need not combine all selected types. See the complete
[mixed selection and delivery contract](../spec/events/MIXED-SUBSCRIPTIONS.md).

## Runtime, callbacks and shutdown

Create `ApiRuntime` on the application's main thread **before application threads**.
It acquires the existing transport bootstrap and retains it until its worker has
joined. The worker starts lazily on the first accepted `connect()` request and is
shared by that runtime's endpoints. Coordinate all other users of the same c-ares
library; application threads must retire before the runtime is destroyed. See the
[bootstrap lifetime contract](../spec/transport/TCP-RUNTIME.md).

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

Listeners run serially on the worker. Their span expires when that invocation
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
subscriptions for the worker's latest lifecycle and first asynchronous failure.
There is no application polling requirement for connection or delivery progress.

`connect()` accepts ASCII `host:port` or `[IPv6]:port`. The first accepted address is
fixed; repeating it while open is a no-op. Another address, or reconnecting a closed
or failed endpoint, rejects. Automatic finite recovery uses the endpoint policy.
Selectors are nonempty exact printable ASCII, within the runtime's symbol limit,
without a leading `*` or wildcard/All syntax.

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
| `maximumSymbolUnits` | 128; configurable in 1..128 |
| `dnsServers` | Empty: discover up to 4 system IPv4 resolver addresses |
| `dnsPort` | 53 |

| Endpoint option | Default |
| --- | --- |
| `contract` | `TICKER` |
| `eventRegistry` | Absent: built-in Quote0/Profile1; an explicit value is copied as an immutable snapshot |
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

## Planned statistics

The [layered statistics contract](../spec/subscriptions/CLIENT-STATISTICS.md) defines
future endpoint/subscription counters, epoch and availability rules. These counters
are not exposed by the current facade; lifecycle diagnostics remain available.

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
