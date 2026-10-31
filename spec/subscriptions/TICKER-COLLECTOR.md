# Bounded TICKER collector

[Русская версия](TICKER-COLLECTOR.ru.md). English is authoritative.

Include `qd/collectors/ticker.hpp` and link `qdcxx_collectors`. The pure C++23
collector keeps one latest value per exact record/symbol key and independent,
finite local delivery views. It consumes whole admitted
[live-session events](LIVE-SESSION.md) and uses [desired state](STATE.md) for
each view. Runtime, callbacks, upstream demand multiplexing and HISTORY are
separate components.

[Qualification](../../tests/conformance/subscriptions/TICKER-VALIDATION.md) records
the executed native/Java profiles and their comparison limits.

## Shared cache and local views

The collector owns an immutable local schema, fixed cache storage and a finite
number of view slots. Each view owns its TICKER desired state and initial/update
queue metadata. View tokens identify their originating collector, slot and
non-reused activation; a foreign or stale token cannot address a replacement view.
Retaining a token retains only a fixed origin marker.

`storeEverything=false` retains a value while at least one local view has that
exact interest. Removing the last interest evicts it. Pre-add input without any
interest is not retained. With `storeEverything=true`, admitted values can remain
without interested views and can supply later initial delivery. Neither mode
promises a cached snapshot on every remove/re-add.

Pending capacity is reserved separately for every view and equals cache-key
capacity. Each cached key has at most one pending value per view, with fixed
initial/update links. A stalled view cannot consume another view's queue storage
or retain superseded payload versions independently of the shared cache.

These local interests control cache retention and delivery. They do not export a
wire subscription union or represent applied peer subscriptions.

## Whole-frame admission and equality

Only complete DATA/TICKER_DATA type-10 events with exact schema provenance are
accepted. The session validates framing before the collector sees an event; an
invalid frame's decoded prefix is not admitted. The first valid event pins its
source epoch even when no key is retained. Binding generations may change within
that epoch; another epoch requires explicit reset.

Stage plans the complete frame before publication. It compares each occurrence
against the candidate's most recent value in source order. Thus a change and
reversal within one frame can still dirty a key even when its final fields equal
the previously delivered value. Present all-zero fields differ from absence.
Equal fields do not dirty a key or move its pending position.

The selected default-field profile compares exact scalar representations and
supported nullable text/byte/opaque content. Flags alone are not a field change.
Raw REMOVE does not instruct this cache to erase a key. The collector uses fields
already materialized by the decoder; it does not decode another payload or infer
business conversions or arbitrary Java object equality.

Delivery distinguishes the original stored `sourceRecord` from `deliveryFlags`.
The source retains raw flags, indexed removal time, field metadata, typed/null/
opaque values and binding provenance. Selected TICKER delivery flags are zero,
matching the measured default-field FLAGGED_DATA projection. An equal-only input
does not replace the last effective value's source metadata.

## Initial and live delivery

New interest in an available cached value schedules initial delivery. Updating a
pending key replaces its visible latest value without moving its queue position.
An update after initial acknowledgment enters live/update order. Remove invalidates
the old membership; re-add creates a new activation and a new pending position.
The explicit positive retrieval quantum controls initial/update balance; its
default 100 follows the locked production source.

An initial acknowledgment can leave that key's update-queue position available
for reuse. Updating the acknowledged key before the quantum boundary must preserve
the measured update ordering; removing both queue links immediately would change it.

`front` prepares one borrowed latest-value offer for one view. `pop` acknowledges
that current offer; it is not a callback or peer acknowledgment. There is no
multi-record batch API in this component.

All access, source-owner copying/release, moves and destruction must be serialized
and nonreentrant. No background thread or application callback runs. Borrowed
schema and desired-state references follow their documented owner/mutation
lifetimes. Cache-changing publication, relevant eviction, offer replacement,
reset and owner movement invalidate affected borrowed delivery/latest values
before backing is released. Callers must finish using an offer before such an
operation.

## Failures, stale evidence and reset

Work preflight rejection preserves input ownership, cache freshness and existing
offers. Failed/no-op desired mutations preserve prior state and offers. Charged
work is not refunded.

Shared cache/resource exhaustion rejects the complete unconsumed input and makes
shared state explicitly stale. It invalidates existing current-value offers even
when their backing remains retained for stale inspection. Every view observes
that shared gap; arbitrary live keys are not silently evicted to make admission
appear successful. Open-slot/core desired-state rejections preserve other views.

Reset preflights disposal, invalidates offers, discards old cache/pending state,
releases epoch owners and retains desired interests. A reserved reset notice
records the local generation, coalesced resets, prior cause and unknown upstream
loss. Exact acknowledgment of the current notice is required before fresh-epoch
delivery. This is one collector-owner barrier; it does not prove acknowledgment
by each view, downstream callback, transport or peer. Stale acknowledgments cannot
unlock a newer notice. Schema replacement requires a new collector.

## Bounds and evidence

Limits are finite and immutable; zero means zero capacity. Requested resident
backing includes fixed metadata/candidate storage, owned schema and unique whole
retained event backing, including unrequested records sharing an event. Per-view
core desired-state scopes and shared session snapshot/epoch scopes remain separate
enforced budgets. This is not allocator overhead or process RSS.

Admission accounts old owners plus the incoming candidate before publication.
A replacement can exceed peak capacity even when its final cache would fit.
Matching, equality, metadata maintenance and conservative possible owner disposal
are charged before destructive commit. Work is caller-delimited per turn, not a
cumulative tape/file quota. Destructor/move-assignment cleanup is outside those
turn credits and has no bounded-latency claim.

The [capture profile](../../tests/fixtures/subscriptions/CAPTURES.md) distinguishes
actual direct-distributor Java observations from native policy. A Java direct
REMOVE input carrying a nonzero ordinary value is not generally representable by
wire REMOVE: the wire decoder supplies default ordinary fields and indexed removal
time. Native comparisons must bind actual decoded inputs and explicitly selected
representable phases; they cannot establish full wire parity for every direct
distributor operand. The profile also executes 104 cached keys across the default
100-record initial/update quantum. After one initial record, an update to that
consumed key and another 99 initial records, the updated key precedes the remaining
four initial records. This larger case complements the small partial-delivery trace.

## Compiled driver fragment

This function ran in the production-only installed consumer on Windows and Linux.
Call it from a serialized driver with a ready `LiveSession`, its matching immutable
schema, and one complete TICKER_DATA envelope for `Sample/ALPHA/Value10`. Local record
0 is an untimed `Sample` with one compact-int field `Value` (type 8); the driver has
already supplied its matching peer description. The function applies local interests;
the driver owns wire subscription output and I/O lifecycle.

The finite limits reserve two views, four cache keys, five event frames, 512 KiB of
cache backing and 1 MiB of collector backing, with separate desired-state budgets.
Work credits cover this small demonstration. Success is zero after first delivery,
late-view initial replay and last-interest eviction. Nonzero identifies a failed
check; copying the schema can propagate an allocation exception. Borrowed records
are read before `pop` invalidates the offer. Build/link guidance is in
[CPP build](../../docs/CPP-BUILD.md).

```cpp
#include <qd/collectors/ticker.hpp>
#include <cstddef>
#include <span>
#include <utility>

namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace s = org::ttldtor::qdcxx::collectors;
/** Exercise one externally driven frame and two independent pure local interests.
 * The caller owns the ready serialized LiveSession and supplies exactly one whole TICKER_DATA
 * envelope with Sample/ALPHA/Value10. This function neither composes upstream interests nor closes I/O.
 * @param source Actual externally owned protocol session, serialized through this call.
 * @param schema Matching complete immutable local schema, copied with preserved provenance.
 * @param bytes One complete controlled data envelope.
 * @return Zero after both deliveries are acknowledged and both views close; nonzero on a failed check.
 */
int runTickerExample(p::LiveSession &source, const c::LocalSchema &schema, std::span<const std::byte> bytes) {
    auto copy = schema;
    s::TickerWork work{1000, 1000000, 1000000, 100000000};
    const s::TickerLimits limits{2, 4, 5, 512 * 1024, 1024 * 1024, {4, 8, 32, 128, 16, 256, 16}};
    auto ticker = s::TickerCollector::create(std::move(copy), limits, {false, 100}, work);

    if (!ticker) {
        return 1;
    }

    auto one = ticker->openView(work);

    if (!one) {
        return 2;
    }

    const c::SubscriptionRequest request{{schema.recordContext(0), u"ALPHA"}, {}};
    c::SubscriptionWork commands{16, 100000, 100000};

    if (!ticker->add(*one, std::span(&request, 1), commands, work)) {
        return 3;
    }

    // Desired application is local; the external driver decides any wire composition and output.
    p::LiveWork inputWork{16, 1000000, 1000000, 1000000};
    auto input = source.feed(bytes, inputWork);

    if (input.status != p::LiveStatus::EVENT || input.accepted != bytes.size() || !input.event) {
        return 4;
    }

    if (!ticker->stage(std::move(*input.event), work)) {
        return 5;
    }

    auto first = ticker->front(*one, work);

    if (!first || !*first) {
        return 6;
    }

    const auto &record = *(**first).value.sourceRecord;
    const auto value = record.typedValues.empty() ? record.values[0] : record.typedValues[0].raw;

    if (!record.context.matches(schema.recordContext(0)) || record.symbol != u"ALPHA" || value != 10) {
        return 7;
    }

    // Copy needed scalars before pop; borrowed record/view storage must not escape its invalidation boundary.
    if (!ticker->pop(*one, work)) {
        return 8;
    }

    auto late = ticker->openView(work);

    if (!late || !ticker->add(*late, std::span(&request, 1), commands, work)) {
        return 9;
    }

    auto initial = ticker->front(*late, work);

    if (!initial || !*initial || (**initial).kind != s::TickerDeliveryKind::INITIAL) {
        return 10;
    }

    if (!ticker->pop(*late, work) || !ticker->closeView(*one, work)) {
        return 11;
    }

    auto shared = ticker->latest(request.key, work);

    if (!shared || !*shared || !(**shared).current) {
        return 12;
    }

    if (!ticker->closeView(*late, work)) {
        return 13;
    }

    auto removed = ticker->latest(request.key, work);

    return removed && !*removed ? 0 : 14;
}
```
