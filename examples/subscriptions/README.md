# Console subscription examples

[Русская версия](README.ru.md). English is authoritative.

Start with the existing [Quote console application](receiver/quote.cpp). It receives actual binary
QTP data through the public `TcpRecovery` and `SubscriptionConsumer` APIs, projects the complete
built-in eight-field Quote and prints bid/ask raw price/size values. It is an installed consumer
application, with no test peer or test framework in its executable.

## Build and run the Quote receiver

First build/install with CLIENT, RUNTIME and RECOVERY enabled using the exact dependency inputs in
the [build guide](../../docs/CPP-BUILD.md). Then configure its existing standalone CMake project:

```powershell
cmake -S examples/subscriptions/receiver -B build/quote-subscriber '-Dqdcxx_DIR=C:/dev/qdcxx/install/lib/cmake/qdcxx'
cmake --build build/quote-subscriber --config Release
```

Use your actual prefix/package library directory. A TLS-enabled installation also requires its
qualified `ORG_TTLDTOR_QDCXX_OPENSSL_ROOT` at consumer configuration time; it does not turn this
plain application into TLS. Visual Studio puts the executable under `Release`; single-configuration
generators normally put it directly in the build directory. For Visual Studio, run from the checkout root:

```powershell
./build/quote-subscriber/Release/qdcxx-quote-subscription.exe --help
./build/quote-subscriber/Release/qdcxx-quote-subscription.exe --help-ru
./build/quote-subscriber/Release/qdcxx-quote-subscription.exe --host 127.0.0.1 --port 19123 --symbol IBM --duration-ms 5000
```

For a single-configuration Windows build use `./build/quote-subscriber/qdcxx-quote-subscription.exe`;
on Linux normally use `./build/quote-subscriber/qdcxx-quote-subscription`. PowerShell needs
`./qdcxx-quote-subscription.exe` when running from the executable directory. Short executable names
elsewhere assume `PATH`; otherwise use the actual full path.

No arguments give the same English help without schema/bootstrap/network work. A compatible source
must already listen at your selected port and support the requested Quote/symbol. The example starts
no service. For DNS names provide explicit numeric IPv4 `--dns-server` and optional `--dns-port`;
there is no system resolver discovery. Named routes add the documented 80 MiB logical requested-payload
reservation and separate conservative DNS work; numeric routes bypass it. These are not RSS/time limits.

The bounded source/settings are [quote.cpp](receiver/quote.cpp), [settings.hpp](receiver/settings.hpp)
and [arguments.hpp](receiver/arguments.hpp). The application creates a schema and endpoint snapshot,
opens a receiver on a STREAM queue, submits the desired set, then creates/starts/polls recovery on
its owner thread. The same finite work profile is used during progress and shutdown.

The explicit mapping budget allows 128 exact type alternatives, covering the current 72 Quote
alternatives without changing its default eight-field schema. This low-level application keeps
its diagnostic raw-decimal output; the ordinary [Feed example](../feed_quote/README.md) displays
business getters under the [field conversion contract](../../spec/events/FIELD-CONVERSIONS.md).

In the actual `QuoteConsumer::onBatch`, `batch` is a borrowed public `SubscriptionBatch`; its loop
projects each record with explicit finite credits:

```cpp
auto work = projectionWork();
auto projected = batch.project(index, work);
```

This is an excerpt from the complete application: `index` is the current record index, and
`projectionWork()` is the declared helper in `settings.hpp`. The full callback checks projection
failure and the Quote variant before reading it, checks stdout and returns the existing whole-batch
action. The projected Quote owns its values; the batch/projection service is callback-scoped.
The [receiving API guide](../../docs/SUBSCRIPTION-RECEIVER.md) explains how to keep generic batches,
add independent logical clients or select the other delivery families without retaining borrowed facades.

`--maximum-records N` optionally caps printed rows at 1–1000000; duration remains an unconditional
cutoff. This example can stop printing inside a callback batch. In contrast, the separate CLI's
`--max-records` target preserves a whole published batch. At most three fresh attempts use a fixed
250 ms retry delay. Duration/row cutoff requests local drain (`--drain-ms`, default 1000); signals
or failures request abort. Exit is 0 for completed drain/help, 1 for failure, 2 for invalid arguments,
130 for interruption. Local drain proves neither peer ACK nor whole-network flush.

Printed `.raw` values are exact WideDecimal bits; raw zero is NaN. Use `toString()` or `toDouble()`
when displaying business values. The example remains plain TCP/STREAM; for Quote/Profile/HISTORY,
external JSON, verified TLS and optional Basic files use the
[recovery CLI tutorial](../../docs/SUBSCRIPTIONS.md) and [tool reference](../../tools/subscribe/README.md).
Its machine-readable formats and TLS host limits are defined in
[recovery output](../../spec/cli/RECOVERY-OUTPUT.md) and
[closed TLS host profile](../../spec/cli/TLS-HOST-PROFILE.md).

<a id="programmatic-controlled-stream-example"></a>
<a id="example-subscriptions-legacy-stream-en"></a>

## Callable legacy STREAM example

The remaining sections describe the existing `runStream` composition and callable example library,
whose configuration, output and terminal rules are separate from the receiver application above.

[example.cpp](example.cpp) is a small callable example, with no second operational CLI. Its
`runSubscriptionExample` receives an already-created main-thread `RuntimeBootstrap`, a validated
caller schema, an explicit controlled IPv4 loopback port and a synchronous output stream. It requests
exact `ALPHA` on local record zero and stops source admission after the callback target is reached.
The example callback prints local/sender identities and field count. All original generic fields
remain available through `BatchRecord`; prepared standard-event projection is an independent choice.

[stream_client.hpp](stream_client.hpp) / [stream_client.cpp](stream_client.cpp) show the complete
application composition over the actual runtime and dispatcher. They are application/example source,
not a new installed library facade. Exact UTF-16 selectors, including empty/NUL/unpaired units, and
caller-provided ordinary schemas remain separate from the
[CLI's bounded ASCII/built-in profile](../../tools/subscribe/README.md).

## Build and run the real example

Build/install a production package with CLIENT and RUNTIME explicitly enabled and the qualified SDK
roots supplied, following the [runtime contract](../../spec/transport/TCP-RUNTIME.md). A relocated
installation exports `qdcxx_client`, `qdcxx_transport`, `qdcxx_event_mapping`, `qdcxx_quote` and
`qdcxx_profile`; the pure event targets do not acquire runtime dependencies themselves.

The example is independently compilable against that installation:

```text
cmake -S examples/subscriptions -B build/subscription-example -Dqdcxx_DIR=/path/to/prefix/lib/cmake/qdcxx
cmake --build build/subscription-example
```

This builds a callable example library. To execute its actual generic callback and both existing
prepared standard-event paths with a deterministic owned TCP peer, run the independent consumer:

```text
cmake -S tests/conformance/connect_consumer -B build/subscription-consumer -Dqdcxx_DIR=/path/to/relocated/prefix/lib/cmake/qdcxx
cmake --build build/subscription-consumer
ctest --test-dir build/subscription-consumer --output-on-failure
```

Use the configured install library directory instead of `lib` when different. The consumer does not
include the source tree's `include/`, find a test framework, fetch SDKs or connect externally. It compiles
the tracked application/example and controlled peer source against production installed headers/targets.
Its internal runner is not a public command-line utility.

## Ownership, work and completion

Create `RuntimeBootstrap` on the application main thread before other application threads. Retire all
runtime owners and test/application threads before destroying it. All driver/runtime/dispatcher work
and related protocol-owner destruction occurs on the calling owner thread.

`runStream` checks an initially empty `std::optional<ClientHandle>` output slot before driver setup.
After fallible owner setup it publishes a control copy without throwing, before progress/callbacks.
Keep the slot object alive through return. Callbacks can call `requestStop`; cross-thread callers must
publish separate copies with their own synchronization. Clearing the slot does not stop the driver,
which retains its own public control reference. After return a retained handle contains only the
detached command port/terminal, without a dispatcher, callable or socket.

Each whole runtime event is admitted before later ordinary commands. A shared owner-turn budget covers
pre-poll local work and post-admission retry. `ownerOperationsPerTurn` is explicitly configurable in
1..10000, default 10000; exhausted work retries locally before another poll/removal, while cutoff and
interrupt observations still run. Factory/setup credits are separate. The remaining finite example
schema/storage/work bounds are visible in the source; they are not RSS, throughput or hard latency claims.

Duration, data target and interrupt stop new source admission and explicitly abort the TCP run/output.
Graceful local retirement finishes the already-admitted eligible prefix, which can contain more records
than the target. Local and native terminal evidence stay separate in `StreamResult`; pending output,
discarded input and Windows unknown write acceptance are not hidden by a local drained outcome.
Native code 1236 after Windows abort remains a visible failure. Sink/callback failure is not retried.
Arbitrary blocking callback/sink code and OS completion cannot receive a hard elapsed-time guarantee.

This example implements no TLS/authentication, reconnect, wildcard, MUX, TICKER/HISTORY or full
subscription lifecycle. See the [subscription guide](../../docs/SUBSCRIPTIONS.md) for the supported milestone.
