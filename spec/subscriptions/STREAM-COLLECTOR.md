# Bounded STREAM collector

[Русская версия](STREAM-COLLECTOR.ru.md). English is authoritative.

Include `qd/collectors/stream.hpp` and link `qdcxx_collectors`. This pure C++23
component defaults to exact STREAM interests over [desired state](STATE.md) and whole
admitted [live-session events](LIVE-SESSION.md). It implements every-record order,
including identical consecutive values, without a latest-value cache. Callback
dispatch, shared upstream multiplexing, TICKER and HISTORY collectors
are separate components. No runtime, socket or JSON dependency is introduced.

## Ownership and input

`StreamCollector::create` accepts a usable STREAM `SubscriptionState`, explicit
`StreamLimits` and `StreamWork`. It validates without changing the desired revision
and allocates all fixed ring storage before taking ownership. Failure preserves
the input state's identity, entries and ownership; charged work is not refunded.
The collector is move-only. `desired()` is a borrowed const pointer, null after
move; successful ownership transfer invalidates prior borrowed collector views.

All related session/event/snapshot/collector access, moves and release must be
serialized and nonreentrant. No background progress or application callbacks
occur. Destruction and move assignment release bounded owners outside caller-turn
credits; they are not bounded-latency or dispatch operations.

`stage` accepts only a complete `LiveEvent` with DATA kind and message type 15.
The session has already validated the whole frame: a malformed frame's completed
decoder prefix never becomes collector input. Exact local record provenance and
UTF-16 symbol select current desired entries; sender IDs are not subscription
identity. Unrequested records and pre-add input are not cached for later replay.

Stage preflights matching, complete selected-record capacity and all retained
whole-event backing before publishing anything. Successful nonempty admission
moves the input event and captures each selected membership/revision. The first
selected frame pins one opaque epoch even after drain. Later binding generations
within that epoch are allowed; queued values retain their original snapshot.
Another epoch is rejected until an explicit successful source reset. A zero selection leaves
the input owner and previous offer unchanged. Rejected admission never consumes
the input or publishes a partial selected frame.

## Revision and delivery barriers

`add/remove/replace/clear` forward the existing atomic desired-state commands with
`SubscriptionWork`. An effective mutation invalidates a prior delivery offer;
failed/no-op mutations preserve it. Unchanged memberships remain eligible.
Remove/re-add creates a new activation and cannot revive old queued records.

`front(work)` preflights withdrawal scanning and potential owner disposal, removes
retired activations, and returns the first eligible borrowed `StreamDelivery`.
Its admission revision is local evidence, not peer acknowledgement. The record
pointer borrows the retained frame; successful nonempty stage/pop/discard or an
effective desired change invalidates it. Failed preflight preserves queue and
offer. Repeated front does not deliver or acknowledge another record.

`pop(work)` requires the current successful nonempty offer and acknowledges exactly
that record. It is not a user callback. Client dispatch must implement the
[callable/barrier contract](CONTRACTS.md) separately. `discard(work)` removes all
pending records atomically without reporting delivery; it preserves the epoch pin
and terminal cause. Counts distinguish staged, delivered, withdrawn, discarded
and ignored records, with explicit saturation.

`resetSource(work)` atomically discards pending records and releases the fixed epoch
pin while preserving desired entries, membership activations, revision, fixed rings
and cumulative counts. It prepays every item/frame visit, event release and identity
release before mutation. Insufficient work preserves owners and offers. Success
invalidates all offers, advances a non-reused reset sequence and reports actual
discarded records with `unknownLoss = true`; upstream continuity cannot be inferred
even when the local queue was empty. A new admitted epoch may then be pinned.
Terminal overflow, a moved-from owner or exhausted reset sequence returns
`INVALID_STATE`; reset never revives a terminal collector. Source ownership must
exclude further old-epoch input before switching; the collector does not maintain
a history of rejected epochs. This operation does not implement transport retry.

Each offer also carries its owning core membership token and original scalar
binding generation, scoped to the pinned epoch. These metadata copies do not
extend the borrowed record's lifetime or retain a protocol snapshot.

For a multi-record consumer, `prepareBatch(storage, work)` writes an eligible
source prefix into caller-provided fixed metadata slots without consuming it.
The caller keeps those slots stable through acknowledgement. Matching and
metadata work are preflighted; no scratch allocation or second filter is added.
`acknowledgeBatch(work)` preflights every item and possible frame/snapshot release
before atomically consuming that exact prefix. It returns separate eligible and
withdrawn counts, without claiming a user callback ran. A client can copy a full
detached candidate before this commit; failure starts no callback and loses no
eligible prefix.

Single and batch offers are exclusive. A successful front or replacement prepare
cannot leave an old batch acknowledgement valid. Effective changes, nonempty
stage, pop/discard, move or destruction invalidate prepared views; failed/no-op
commands and failed work preflight preserve them. Empty storage offers prefix
zero; positive storage may prepare a wholly withdrawn prefix with no eligible
records, acknowledged without invoking a consumer.

## Resources and failure

All limits are finite, including zero: frame slots, selected record slots, retained
whole-event bytes and collector resident bytes. Fixed frame/item arrays never
grow. A retained event counts its full State and arena, including unrequested
records, unused capacity and abandoned monotonic slots. `residentBytes()` adds the
fixed collector backing; `stagedBytes()` counts only retained event backing.

Desired storage and shared snapshot/epoch reservations have their existing
separate bounds. These scopes must be aggregated by an upper owner; the collector
counter is not a total transitive heap or process RSS measurement. Borrowed views
do not create independent owning deliveries or move their quota to the caller.

`StreamWork` covers operations, metadata/membership visits, examined UTF-16 units
and byte work. Matching, publication and destructive work are preflighted before
queue mutation; stage/front/pop allocate no storage. Shared snapshot destruction
is covered by sampling/debiting `LiveEvent::releaseBytes()` immediately before
release, conservatively including unrelated reservations in that epoch. Work
failure leaves input/queue/offer usable and is not terminal. Callers choose new
turns; a continuing feed has no cumulative tape quota.

Capacity overflow is a visible terminal `RESOURCE_LIMIT`, with reserved inline
pending/incoming counts. The incoming frame is rejected whole, existing bounded
owners remain retained, and delivery stops. Removal/clear and work-bounded discard
remain available; adding/replacing demand does not restart a terminal collector.
Library destruction never claims that retained records were delivered.

## Compatibility and validation

The selected [production observations](../../tests/fixtures/subscriptions/CAPTURES.md)
cover per-event/interleaved order, two-client interest removal and identical-value
filtering without pre-add replay. Native tests compare actual input, subscription
and returned output semantic coordinates/counts against those observations.
Wire cipher/raw-symbol representation and pull-loop `more` markers are outside
this collector-level comparison. Two independent local collectors are not a
shared upstream MUX proof.

The stronger local queued-withdrawal/re-add barrier follows the reviewed local
contract; it is not inferred from Java's remove-before-next-input capture. Checks
also cover whole-frame overflow, work retry, old snapshot release, changed binding
generations, moved owners, failed frames, exact unusual text, independent provenance,
continuing ring reuse and full standard Quote values through prepared mapping.

See [validation profiles and evidence boundaries](../../tests/conformance/subscriptions/STREAM-VALIDATION.md).

## Checked integration example

This function is compiled and run by the installed consumer. The caller supplies a
ready schema-compatible session and exactly one full STREAM_DATA envelope with one
selected ALPHA record at local record zero. Limits are example values, not defaults.
It returns zero only after the selected record was explicitly popped; nonzero reports
a local failure. It performs no callbacks or network I/O.

```cpp
#include "qd/collectors/stream.hpp"
#include <cstddef>
#include <span>
#include <utility>

namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace s = org::ttldtor::qdcxx::collectors;

int runStreamExample(p::LiveSession &session, const c::LocalSchema &schema, std::span<const std::byte> bytes) {
    auto desired = c::SubscriptionState::create(schema, c::SubscriptionFamily::STREAM, {1, 2, 16, 32, 4, 64, 4});

    if (!desired) {
        return 1;
    }

    s::StreamWork work{16, 1024, 1024, 16 * 1024 * 1024};
    auto collector = s::StreamCollector::create(std::move(*desired), {2, 4, 131072, 262144}, work);

    if (!collector) {
        return 2;
    }

    const c::SubscriptionRequest request{{schema.recordContext(0), u"ALPHA"}, {}};
    c::SubscriptionWork command{4, 1024, 1024};

    if (!collector->add(std::span(&request, 1), command)) {
        return 3;
    }

    p::LiveWork inputWork{16, 1000000, 1000000, 1000000};
    auto admitted = session.feed(bytes, inputWork);

    if (admitted.status != p::LiveStatus::EVENT || admitted.accepted != bytes.size() || !admitted.event) {
        return 4;
    }

    auto staged = collector->stage(std::move(*admitted.event), work);

    if (!staged) {
        return 5;
    }

    std::size_t delivered = 0;

    for (;;) {
        auto next = collector->front(work);

        if (!next) {
            return 6;
        }

        if (!*next) {
            break;
        }

        const auto &record = *(**next).record;

        // Read borrowed record fields only before the acknowledgement invalidates the view.
        if (record.symbol != u"ALPHA") {
            return 7;
        }

        if (!collector->pop(work)) {
            return 8;
        }

        ++delivered;
    }

    return delivered == staged->selected && delivered == 1 ? 0 : 9;
}
```

<a id="selector-profiles"></a>

## Canonical Exact and explicit All

The desired owner carries an immutable [selector profile](SELECTORS.md). Under
`CANONICAL_STREAM_ALL_V1`, known-record All and matching Exact capture at most two original
activations and offer the concrete record once. Canonical DATA star is ordinary concrete input.
Whole-event domain/work preflight precedes moves, epoch pinning and state/offer changes;
`UNSUPPORTED_SYMBOL_DOMAIN` preserves input. Removing/re-adding a selector never revives old eligibility.
The default and existing example above remain legacy Exact; no wildcard text expansion is inferred.
