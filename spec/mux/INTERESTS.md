# Bounded aggregate subscription interests

[Русская версия](INTERESTS.ru.md). English is authoritative.

`MuxState` owns one schema/family domain and finite local clients. It publishes the
exact union of their desired interests at every revision. It is a pure state
component linked through `qdcxx_mux` to `qdcxx_core`; protocol output, source
sessions, collectors, callbacks and slow-consumer queues have separate owners.

The [state contract](../subscriptions/STATE.md) defines comparable desired-key
semantics. The [capture profile](../../tests/fixtures/subscriptions/CAPTURES.md)
records selected Java aggregate-bound observations. The
[ownership decision](../../docs/decisions/ADR-0005-live-subscriptions.md) separates
local intent, protocol output and completion. The [implementation validation](../../tests/conformance/subscriptions/MUX-VALIDATION.md) records the native matrix and its evidence limits.

## Identity and aggregate meaning

A key combines immutable record-context provenance and exact UTF-16 symbol.
Identical numeric IDs or equal-shaped independent schemas do not match. The
contract family belongs to the owner. HISTORY additionally carries a signed-64
raw lower bound; it is an attribute of the key, not Unix milliseconds or a
business source identifier.

For STREAM/TICKER, an aggregate key exists while at least one client desires it.
For HISTORY, its aggregate bound is the signed minimum of all interested clients'
bounds. Removing one client cannot remove another's key. Raising the earliest
client bound may change the aggregate without removing the key.

The complete aggregate snapshot describes desired upstream demand. It does not
report a wire delta, written bytes, peer application or acknowledgment. Clients
can change desired state while a source is offline.

## Atomic client mutations

Explicit create/remove client operations use opaque owner/slot/activation tokens.
Destroying a token does not remove a client. Key memberships are distinct between
clients; remove/re-add and a HISTORY-bound change retire the old activation.
Unchanged entries preserve their memberships. Counters never wrap or reuse IDs.

`add/remove/replace/clear` validate complete borrowed inputs before publication.
Equivalent duplicates collapse; contradictory attributes reject atomically.
Replace is an exact set operation, including an effective empty replacement.
No-op/failure preserves revision, memberships and borrowed views.

Every effective client lifecycle or desired-state mutation advances the global
revision once. Creating/removing an empty client can advance it while leaving the
aggregate unchanged. Candidate construction and aggregate computation belong to
one owner transaction; there is no fallible synchronization after separately
publishing a client state.

## Selection and borrowed observations

`clientEntries` exposes only that client's desired entries. `aggregateEntries`
exposes the complete union for the existing writer. Both borrow the published
bank and expire on effective mutation, owner movement or destruction. Aliased
command inputs are read before any published backing is retired.

`select` returns only the selected client's current membership. HISTORY requires
raw time at or above that client's bound, inclusively. This is a raw interest
predicate; HISTORY snapshot/transaction control markers still require collector
semantics. It does not fan out an event or invoke a callback.

Query errors remain distinct from absence/false, including insufficient work.
`clientEntries` validates owner/slot/generation in constant time. Other bounded
searches debit explicit work. Owner methods are serialized; concurrent access and
background progress are not part of this component.

## Resource and lifetime scopes

Two fixed banks reserve all client entries, aggregate slots and symbol storage
from creation. Aggregate symbols borrow the same bank's owned units. The old and
candidate banks remain accounted even when one is scratch. Commands do not
allocate or resize containers.

The owner captures record contexts and indexed-layout metadata, retaining their
fixed core identity without retaining schema declarations. A one-time allowance
of `schema.storageBytes()` conservatively covers that imported identity; this is
intentional overcharging. Local fixed metadata and both banks are also charged.
Requested backing excludes allocator bookkeeping and does not measure RSS.

All limits are finite; zero means zero. Non-null empty symbols remain valid even
with a zero text-arena capacity, using a fixed empty sentinel. A null view is
invalid. Embedded zero and raw UTF-16 surrogates retain their exact units; reserved
leading star selectors follow the separate wildcard contract.

Work covers initialization, input/deduplication/context checks, client and entry
scans, aggregate comparisons, text copies and retirement. It is charged before
scratch mutation and is not refunded on rejection. No lifetime/cumulative message
quota is introduced. Destruction releases bounded owners outside caller-turn
credits and makes no bounded-latency promise.

Moving the owner transfers source tokens, retires old destination tokens and
invalidates borrowed spans; self-move preserves the owner. Retained tokens keep
only fixed local identity, not banks, schema declarations or callbacks.

## Compiled consumer example

The following complete consumer source is the compiled example. It creates two local clients with the same STREAM interest, removes one client, then checks the remaining aggregate and client selection. It demonstrates local desired-state behavior only; it does not connect to a source or deliver events. The checked output is `MUX union=1; remaining client selected; revision=5` and exit code 0.

Build this source with CMake and the installed package (replace `C:/sdk/qdcxx` with the installation prefix):
```cmake
find_package(qdcxx CONFIG REQUIRED)
add_executable(MuxExample MuxExample.cpp)
target_compile_features(MuxExample PRIVATE cxx_std_23)
target_link_libraries(MuxExample PRIVATE qdcxx_mux)
```

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/sdk/qdcxx
cmake --build build --config Release
.\build\Release\MuxExample.exe
```

```cpp
#include "qd/mux/interest.hpp"

#include <array>
#include <iostream>

/** Exercise the installed pure MUX API without client, runtime, JSON or ambient configuration.
 * Link this example with qdcxx_mux; nonzero exit identifies a failed contract check.
 * @return Zero for the exact union after isolated client removal, otherwise a deterministic failure code.
 */
int runMuxExample() {
    namespace c = org::ttldtor::qdcxx::core;
    namespace m = org::ttldtor::qdcxx::mux;
    const std::array records{c::LocalRecord{0, u"Custom", false, {{8}}, {{u"Value", 0}}, {}}};
    auto schema = c::LocalSchema::create(records, {1, 1, 1, 16, 32});

    if (!schema) {
        return 1;
    }

    m::MuxWork work{100, 1000000, 1000000, 1000000};
    auto state = m::MuxState::create(*schema, c::SubscriptionFamily::STREAM, {1, 2, 4, 4, 16, 64, 4, 65536}, work);

    if (!state) {
        return 2;
    }

    const auto a = state->createClient(work);
    const auto b = state->createClient(work);

    if (!a || !b) {
        return 3;
    }

    const std::array requests{c::SubscriptionRequest{{schema->recordContext(0), u"AAPL"}, {}}};

    if (!state->add(*a, requests, work) || !state->add(*b, requests, work)) {
        return 4;
    }

    if (!state->removeClient(*a, work)) {
        return 5;
    }

    if (state->aggregateEntries().size() != 1 || state->aggregateEntries()[0].key.symbol != u"AAPL") {
        return 6;
    }

    const auto selected = state->select(*b, requests[0].key, {}, work);

    if (!selected || !selected->has_value()) {
        return 7;
    }

    std::cout << "MUX union=1; remaining client selected; revision=" << state->revision() << '\n';

    return 0;
}

/** Run the documented installed-consumer example.
 * @return The deterministic example contract result.
 */
int main() {
    return runMuxExample();
}
```

