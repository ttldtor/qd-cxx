# Programmatic controlled STREAM example

[Русская версия](README.ru.md). English is authoritative.

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
