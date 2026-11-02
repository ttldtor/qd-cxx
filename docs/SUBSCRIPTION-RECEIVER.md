# Receive generic records and prepared events

[Русская версия](SUBSCRIPTION-RECEIVER.ru.md). English is authoritative.

Use `qd/client/subscription_receiver.hpp` when an application needs independent logical
subscriptions on an existing STREAM, TICKER or HISTORY queue. Generic records remain
complete; explicitly selected records can also be projected through the existing
prepared Quote/Profile mappings. The application supplies its schema, queue and
serialization loop. [Recovery](../spec/transport/RECOVERY.md) supplies the existing
TCP/TLS connection lifecycle and replay; the receiver supplies the callback boundary.

For a complete console application, see the
[Quote subscriber source](../examples/subscriptions/receiver/quote.cpp), its explicit
[bounded settings](../examples/subscriptions/receiver/settings.hpp) and
[argument/help handling](../examples/subscriptions/receiver/arguments.hpp), with the
[standalone CMake project](../examples/subscriptions/receiver/CMakeLists.txt).
This application uses plain TCP and the full built-in eight-field Quote schema.
It uses an explicit canonical exact-symbol profile; it changes no library default.
TLS and custom indexed schemas use their existing separate configuration contracts.

## Build and run the console application

First build and install the library with `ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON` and
`ORG_TTLDTOR_QDCXX_BUILD_RUNTIME=ON`, following [the build guide](CPP-BUILD.md).
Then configure the example as an independent installed consumer. For example, in PowerShell
from a source checkout, replace the installation directory with your own:

```powershell
cmake -S examples/subscriptions/receiver -B build/quote-subscriber -Dqdcxx_DIR=C:/dev/qdcxx/install/lib/cmake/qdcxx
cmake --build build/quote-subscriber --config Release
```

Use the selected build generator's executable directory. A single-configuration generator
places the executable directly under `build/quote-subscriber`; Visual Studio normally uses
the `Release` subdirectory. An installed package built with TLS enabled also needs its
qualified `ORG_TTLDTOR_QDCXX_OPENSSL_ROOT`, even for this plain-linked application;
see the [package/TLS instructions](CPP-BUILD.md).

`qdcxx-quote-subscription --help` and no arguments show identical English help without
schema loading, process bootstrap or networking. `--help-ru` shows Russian help.
To receive from an already running controlled QTP source:

```text
qdcxx-quote-subscription --host 127.0.0.1 --port 19123 --symbol IBM --duration-ms 5000
```

The port is an example, not a bundled service. The source must advertise the supported
binary QTP contract and a compatible Quote description. A named host requires an explicit
numeric `--dns-server`; the application does not discover a resolver from the environment
or system configuration. Runtime limits and timeout/retry values are explicit in the
example settings and built-in help. No account, provider coverage or live public-feed
availability is implied by this local setup example.

For named hosts, the example adds the documented 80 MiB DNS SDK envelope to its
initial logical requested-payload reservation. Its DNS work credits use the separate
conservative allowance in [DNS resource bounds](../spec/transport/DNS-RESOURCES.md).
These values are not measured RSS, CPU instructions or a latency guarantee. Numeric
addresses bypass this DNS profile. Both settings are selected before endpoint creation;
shutdown uses the same per-turn work profile.

The printed `bid.raw`, `bidSize.raw`, `ask.raw` and `askSize.raw` values are exact
`WideDecimal` representation bits. Raw zero represents NaN, not a zero price or size.
Use `WideDecimal::toDouble()` for an approximate floating-point value or `toString()`
for its bounded text representation when presenting business values.

## Register an application callback

1. Construct one validated immutable `LocalSchema` for the endpoint. Keep copies of that
   same schema origin when creating queues, prepared bindings and subscription keys.
2. Create the existing queue for the selected delivery family and selector policy.
   Choose its finite aggregate and per-client capacities for the expected workload.
3. Implement `SubscriptionConsumer::onBatch`. Read `batch.records()` for generic values;
   use `batch.project(index, remainingWork)` for an explicitly prepared Quote/Profile.
   Use `std::get_if` on the owning result variant. Unlisted records return `monostate`.
4. Call `openSubscriptionReceiver` with explicit bindings, limits and preparation work.
   Invalid typed mappings fail before any registration is opened. A generic-only receiver
   uses an empty binding list and needs no business mapper.
5. Submit the initial complete set with the returned handle's `replace`. Observe the
   actual command ticket; then use ordinary `add`, `remove`, `replace` or `clear` for later
   changes. Each logical subscription keeps its own handle.
6. Advance the existing queue or composing recovery owner on its serialized owner thread.
   The receiver starts no thread, endpoint or scheduling loop.

The existing `MuxHandle` also provides pause/resume and stop. Typed mapping does not hide
extra fields or replace generic metadata. For HISTORY, inspect the record's signed
`rawTime`, produced flags and optional trigger/source metadata directly; raw index bits
are independent of business event time. Only explicit `FIELD_METADATA` can supply the
optional projection timestamp from actual original FIELD metadata.

## Keep results and stop safely

The batch facade and prepared projection service are valid only during the callback.
`batch.retain()` keeps an existing independent generic batch lease. A returned Quote/Profile
already owns its projected values and can survive callback, queue and recovery destruction.
Retaining generic backing does not extend the lifetime of the batch facade or mapping plan.
For later projection, explicitly keep an existing prepared mapper and call its `mapView`.

One callback acknowledges one whole batch with the existing `BatchAction`; the application
controls any iteration inside it. Stop and public recovery-owner destruction/replacement
inside that callback preserve the existing callback barrier. Exceptions retain per-client
queue isolation. No extra callback is inserted by the projection service.

`requestDrain(deadline)` on recovery fences native admission/output and attempts to drain
the already queue-admitted local prefix during later explicit polls. Inspect the actual
terminal outcome after owned work retires. The deadline can escalate to abort; it does not
prove peer receipt, wire flush or a complete upstream history snapshot. `requestStop()`
and destruction retain their immediate abort semantics.

For a recovery-owned HISTORY queue, call the owner's `historyRangeStatus(handle, key,
work)` on its creating thread after `poll()` returns. A successful detached observation
reports the existing queue's `completeDeliveredRange` for that exact bound and epoch.
Held input, reset/reconnect/cutoff or unavailable authentication make the current query
ineligible; report unknown instead of reusing an older complete result. The
[recovery observation contract](../spec/transport/RECOVERY.md) defines eligibility and
work. Raw progress, event flags and local drain alone do not prove a completed range.

The [receiver contract](../spec/subscriptions/RECEIVER.md) documents exact errors, work
charges and compatibility. The [queue contract](../spec/mux/QUEUES.md),
[prepared event mappings](../spec/events/EVENT-MAPPING.md) and
[recovery contract](../spec/transport/RECOVERY.md) define the underlying boundaries.
