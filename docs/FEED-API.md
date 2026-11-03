# Public feed API {#feed-api}

[Русская версия](FEED-API.ru.md) · [Practical guides](guides.md) · [API reference](api.md)

Use `qd/api/feed.hpp` and link `qdcxx_feed` to receive typed `events::Quote` and
`events::Profile` through a library-owned worker. Applications create endpoints,
listeners and exact symbol subscriptions; the library progresses connection,
finite recovery, queues and delivery. Subscriptions can be created before or after
`connect()`.

This API supports plain binary QTP over TCP with built-in full Quote/Profile
schemas. An endpoint chooses TICKER (default, latest values with possible
conflation) or STREAM (every admitted selected source record). TLS, credentials,
HISTORY, wildcard selectors and external schemas are outside this API. The
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
#include <thread>

int main() {
    namespace q = org::ttldtor::qdcxx;

    try {
        q::api::ApiRuntime runtime;
        auto endpoint = runtime.createEndpoint();
        auto quotes = endpoint->getFeed()->createSubscription<q::events::Quote>();

        quotes->addEventListener([](std::span<const q::events::Quote> batch) {
            for (const auto &quote : batch) {
                std::cout << quote.bidPrice.toString() << '\n';
            }
        });
        quotes->addSymbols({"AAPL", "IBM"});
        endpoint->connect("demo.dxfeed.com:7300");

        std::this_thread::sleep_for(std::chrono::seconds(10));

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

A listener exception fails that subscription with `CALLBACK_FAILURE`; it is not
retried and does not fail other subscriptions. Removing a listener prevents future
selection; a callback already selected may finish. Listener captures retire outside
control locks. Awaiting endpoint closure establishes callback/capture quiescence.

`CloseResult::drained` describes the actual admitted local prefix. Inspect
`deadlineExpired`, `hadGap` and `diagnostic` as well. It does not establish peer
acknowledgement, unread-wire completeness or an uninterrupted upstream history.

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
| `maximumSymbolUnits` | 128; configurable in 1..128 |
| `dnsServers` | Empty: discover up to 4 system IPv4 resolver addresses |
| `dnsPort` | 53 |

| Endpoint option | Default |
| --- | --- |
| `contract` | `TICKER` |
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
