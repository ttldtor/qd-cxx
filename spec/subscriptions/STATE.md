# Bounded desired subscription state

[Русская версия](STATE.ru.md). English is authoritative.

`qd/core/subscription.hpp`, linked through `qdcxx_core`, implements the pure desired-state
part of the [subscription contract](CONTRACTS.md). It does no I/O, parsing, callback dispatch,
collector delivery, clock work or background scheduling and requires no JSON/runtime dependency.
`SubscriptionChange` is a synchronous local application result, not wire emission or a peer
acknowledgement. Upper-layer callback barriers and aggregate runtime credits remain separate work.

## Identity, text and ownership

Create a move-only `SubscriptionState` from one validated `LocalSchema`, one
`SubscriptionFamily` and explicit `SubscriptionLimits`. It captures exact record provenance
and indexed-layout facts, not the schema declarations. The input schema need not outlive it.
A bounded schema copy shares provenance; an independently created equal-shaped schema does not.

`LocalSchema::identity()` returns an owning fixed `SchemaIdentity` without allocation
or copying declarations. It also identifies an empty validated catalog. `valid()`
distinguishes a retained origin from a default/moved-from token; `matches()` compares
exact origins, and invalid tokens never match. The token can outlive the catalog.

Default Exact keys contain `RecordContext` and a borrowed exact UTF-16 `string_view`. The input text must
stay unchanged through the call; it is copied into owned packed storage on effective mutation.
A null data pointer is invalid, including a default-constructed view. A non-null empty view is
valid. Embedded zero and unpaired surrogate units are preserved. There is no normalization or
glob expansion; exact text beginning with `*` is rejected. Explicit All is separately enabled by the selector profiles below.

HISTORY keys must identify records with explicit indexed-time metadata for add, replace **and
remove**, even if removal would otherwise be absent. HISTORY add/replace require a signed64
raw bound; STREAM/TICKER reject a supplied bound. Removal needs no old bound. All representable
signed64 values are raw record time, not milliseconds; the bound is an attribute of a key.

`entries()` exposes borrowed immutable views. Effective mutation, move, assignment or destruction
invalidates them; no-op and rejected commands preserve them. Copy `membership` alone to retain
an owning eligibility token. It keeps a fixed opaque owner identity alive, not text or entries.
`isCurrent()` rejects default, foreign-owner and retired activations. Move transfers identity;
old destination tokens become non-current. Moved-from entry/revision getters return empty/zero;
family retains its previous value and commands reject as described below.

## Commands and failures

Add upserts, identical duplicates collapse, and a changed HISTORY bound creates a new activation.
Remove accepts keys only; absent valid keys are no-ops. Replace publishes exactly the validated
map; empty input and clear empty it. Contradictory duplicate attributes reject the whole command.
Unchanged memberships retain activation IDs. Remove/re-add never makes an old token current.

Each effective command advances revision once. No-op/failed commands preserve the map, revision,
activations and borrowed views. All validation, work/peak reservation and candidate construction
finish before swaps publish state. Inputs may borrow text from the current state. A no-op is
allocation-free but still validates and consumes work. Successful clear allocates no candidate
storage; rejection uses the ordinary error path, including implementation exception bookkeeping.

Results use `std::expected<SubscriptionChange, SubscriptionError>`. Errors distinguish invalid
state/context/selector/history, contradictory input, resource/work limits, natural ID exhaustion
and allocation failure. Command credits are checked/debited first: moved-from with zero command
credits returns WORK_LIMIT; otherwise it returns INVALID_STATE without inspecting inputs.
`create()` separately rejects invalid family, excessive metadata and failed allocation.

Callers serialize all access. There is no thread-safe command port or reentrant callback API here.
Revision/activation counters do not wrap or reuse; practical uint64 exhaustion is guarded but
was checked by source review rather than a runtime exhaustion test.

## Finite accounting

All limit fields are required; zero means zero. schemaRecords bounds fixed captured metadata.
entries/totalSymbolUnits bound resident requested array slots and UTF-16 capacity; symbolUnits
bounds one symbol; commandEntries includes duplicate borrowed inputs. peakEntries/peakSymbolUnits
include old and candidate backing simultaneously. No temporary dynamic maps or key copies exist.
Peak settings can prevent an otherwise shrinking mutation; clear requires no second entry arena.

Fixed owner identity and retained opaque schema identity are distinct from variable buffers;
tokens can extend identity lifetime. This component does not enforce later shared-context
registration/lease credits. Array slots/units bound logical requested storage, not allocator,
exception-runtime bookkeeping or measured process RSS.

Reuse one mutable `SubscriptionWork` across commands in an explicit caller turn. Successfully
charged commands, key/metadata/comparison visits and examined/copied UTF-16 units are not refunded
on rejection/allocation failure. Supply fresh credits only at the chosen next turn; no cumulative
feed lifetime quota exists. Scans are conservatively charged and can be quadratic in inputs or
resident×input; insufficient work rejects before publication rather than secretly continuing.

## Example

Illustrative capacities below are inputs, not production defaults. This complete C++23 program
links only `qdcxx_core`; it demonstrates revision idempotence and retired membership:

```cpp
#include "qd/core/subscription.hpp"

namespace core = org::ttldtor::qdcxx::core;

int main() {
    constexpr std::int32_t COMPACT_INT_FIELD_TYPE = 8;
    const core::LocalRecord record{
        .id = 0, .name = u"Quote", .hasTime = false, .fields = {{COMPACT_INT_FIELD_TYPE}}, .lookups = {{u"x", 0}}};
    const core::SchemaLimits schemaLimits{.records = 1, .fields = 1, .lookups = 1, .nameUnits = 5, .totalNameUnits = 6};
    auto schema = core::LocalSchema::create(std::span(&record, 1), schemaLimits);

    if (!schema) {
        return 1;
    }

    const core::SubscriptionLimits limits{.schemaRecords = 1,
                                          .entries = 4,
                                          .symbolUnits = 16,
                                          .totalSymbolUnits = 64,
                                          .peakEntries = 8,
                                          .peakSymbolUnits = 128,
                                          .commandEntries = 8};
    auto state = core::SubscriptionState::create(*schema, core::SubscriptionFamily::STREAM, limits);

    if (!state) {
        return 2;
    }

    core::SubscriptionWork turn{.commands = 8, .keyVisits = 512, .symbolUnits = 4096};
    const core::SubscriptionRequest request[] = {{{schema->recordContext(0), u"AAPL"}, std::nullopt}};
    const auto added = state->add(request, turn);

    if (!added || !added->changed || added->revision != 1) {
        return 3;
    }

    const auto membership = state->entries()[0].membership;
    const auto repeated = state->add(request, turn);

    if (!repeated || repeated->changed || repeated->revision != 1) {
        return 4;
    }

    const core::SubscriptionKey key[] = {{schema->recordContext(0), u"AAPL"}};
    const auto removed = state->remove(key, turn);

    if (!removed || removed->revision != 2 || state->isCurrent(membership)) {
        return 5;
    }

    return 0;
}
```

The dedicated `core_subscription_state` CTest covers mutation, identity, history, capacities,
work and allocation rollback. See [validation](../../tests/conformance/subscriptions/STATE-VALIDATION.md)
for executed configurations and limits.

<a id="selector-profiles"></a>

## Explicit selector profiles

The default is `EXACT_ONLY`. The immutable [selector contract](SELECTORS.md) adds canonical Exact
and opt-in STREAM All through `core::allSymbols`; All is a separate selector key with one entry
and zero desired symbol text. Captured Exact/All choices retain original activations across mutation;
`SubscriptionMembership::anyOf` is structural OR, not a matching/liveness oracle. Canonical profiles
and appended selector errors do not imply stable binary ABI: rebuild consumers and account current metadata.
