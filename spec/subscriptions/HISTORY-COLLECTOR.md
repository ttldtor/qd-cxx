# Bounded HISTORY collector

[Русская версия](HISTORY-COLLECTOR.ru.md). English is authoritative.

The pure C++23 HISTORY boundary uses an immutable local schema, whole admitted
[live-session events](LIVE-SESSION.md) and per-view [desired state](STATE.md).
Its generic partition is an exact record context and UTF-16 symbol; a point is
identified by the full signed-64 raw indexed time. This is not a business source-ID
decoder or an order-book model. Ordinary untimed records are rejected.

The [Java capture profile](../../tests/fixtures/subscriptions/CAPTURES.md) supplies
selected production observations. Source facts, executed comparisons and native
policies retain their separate scopes. Protocol, runtime, upstream demand and
application callbacks remain separate owners. The public C++ API is installed as
[`qd/collectors/history.hpp`](../../include/qd/collectors/history.hpp) in the
existing `qdcxx_collectors` target; the [validation page](../../tests/conformance/subscriptions/HISTORY-VALIDATION.md)
records qualified behavior and limits.

## API and ownership

`HistoryCollector::create` accepts an owning `core::LocalSchema`, explicit
`HistoryLimits`, `HistoryOptions` and `HistoryWork`. It moves the caller's schema
only on success. The collector is movable; owner operations are serialized and
nonreentrant. Factory validation checks requested fixed backing before allocation
and verifies actual capacities before publication. Core desired-state and session
budgets retain their independently enforced scopes.

`openView` creates a local view with explicit snapshot-flag projection options.
`add`, `remove`, `replace` and `clear` use separate core command and collector work
credits. `closeView` retires that view. Tokens identify the exact owner, slot and
activation; stale or foreign handles fail explicitly. Distinct detached token
copies may be copied/released across threads: their fixed scalar marker retains
no protocol lease, desired-state owner or callback. Related collector and protocol
owners remain serialized.

`stage` consumes a successfully admitted whole source event. `front` and `pop`
prepare and acknowledge local projection; `rangeStatus` observes its completeness.
`reset` and `acknowledgeReset` implement the explicit owner reset barrier.
`forgetUnobserved` is the separate empty-metadata reclamation operation described
below. Work exhaustion preserves published state and caller event ownership.

## Raw progress and completeness

HISTORY can deliver raw progress while a transaction or snapshot is incomplete.
Pending records are not silently withheld until transaction end. Source coverage,
source transaction consistency and a particular view's projection consistency are
different facts. Flags zero and an END above the requested lower bound do not
prove a complete requested range.

| Situation | Meaning |
| --- | --- |
| Legacy data without observed BEGIN/MODE | Raw progress can be delivered; snapshot coverage is unknown. |
| Requested inclusive lower bound not covered | The requested range is incomplete. |
| Covered source range with explicit/implicit transaction pending | Progress remains incomplete. |
| Consistent source with initial/local view progress pending | This view is still incomplete. |
| Consistent boundary covers the bound and the view is consistent | Complete only for the exact epoch, membership and observation. |
| SNIP ends above the requested lower bound | Snapshot finished and truncated; full requested coverage is not proved. |
| Bound expands below the known retained frontier | Previous completeness no longer applies. |
| Failed view, shared stale state, pending reset or epoch gap | Fresh completeness is unavailable. |

`rangeStatus` is a detached observation with epoch, membership/revision and
observation serial. Query it again after owner progress; a copied status does not
update itself or certify that the collector remains current.

Completeness applies to the requested range. For example, views may request lower
bounds 0 and 2. After a descending BEGIN at 3 followed by 2, the higher range is
covered. Once its projection is drained and no source transaction is pending,
`completeRequestedRange` is true for that observation even while the separate
source `snapshot` fact remains `RECEIVING` for the lower interest. The range from 0
remains incomplete until covered. A later mutation requires a new observation.

## Shared storage and view delivery

Shared storage maintains bounded indexed points and snapshot frontiers. Views
have independent desired bounds, initial cursors, transaction state and finite
local update FIFOs. Initial history is descending; measured initial/local balance
uses a retrieval quantum of 100 and a separate per-partition snapshot quantum of
10000. Options supply these explicitly, without ambient name-based properties.
Conflation is limited to the measured last pending local record at the same index.

Snapshot replacement can generate removals for swept points. A projected HISTORY
record therefore has its own type: local record provenance, exact symbol, raw
index, normalized delivery flags and typed field views. Synthetic ordinary fields
have exact schema defaults; indexed contributors contain the projected time.
Nullable text and opaque bytes remain distinct and are not business-deserialized.

An optional raw source record refers only to a real admitted record. Generated
boundaries/removals do not invent a sender, cipher, raw flag or `removalTime`.
Actual trigger metadata is labeled separately from the projected output.

`front` prepares one borrowed offer without publishing cursor, queue or retrieval/scheduling
credit changes. It still consumes the caller's explicit `HistoryWork` credits.
`pop` acknowledges that exact offer. Failed preparation preserves every
byte of the previous offer; each view has separate scratch. Successful stage,
effective mutation, pop, close, reset, forget or owner movement invalidates affected
borrowed values before release. Caller use must finish before those operations.
All collector/protocol-owner operations are serialized and nonreentrant.

## Retention and explicit forgetting

In the measured false-retention profile, immediate last-remove/re-add does not
replay data and ordinary live input remains gated until a fresh BEGIN. Small
empty-partition snapshot metadata is therefore distinct from retained points and
payload owners. True retention can replay cached history and project difference
removals on a later snapshot.

The native clock-free profile does not import Java's opportunistic 60-second
empty-metadata expiry. `forgetUnobserved` explicitly reclaims empty metadata with
no live interest or pending reference. Its finite report records forgotten
continuity and a never-reused generation. It does not reset healthy views or
require downstream acknowledgment. A later activation starts a new local
continuity scope. This prevents an unreclaimable cumulative key limit without
adding a background timer, silent LRU or unbounded tombstone map.

## Admission and failure scopes

Limits bound partitions, points, symbols, shared cache frames/bytes, views,
per-view pending items/frames/bytes and requested resident storage. Zero is zero
capacity. Core desired-state allocations and session snapshot/epoch backing are
separate enforced scopes; requested bytes are not RSS or a latency promise.

Whole-frame planning validates context and source semantics before publication.
Shared old-plus-incoming peaks are checked before assuming any failed view's
retirement frees space. Shared exhaustion rejects unconsumed input and marks
shared evidence stale. If shared admission fits, the complete set of individual
view failures is established before one atomic bound-trim/release commit.
Failed views remain queryable until close and stop contributing retention/bounds.
Healthy-view isolation applies within the independently bounded shared scope.

Work rejection leaves prior cache, views, offers and input ownership unchanged.
Candidate failures and caller-input spans cannot escape a rejected plan. Work
includes fixed-array initialization, searches, equality, sweeps, view matching,
queue maintenance, scratch and possible final owner disposal. Replacement limits
cover old plus incoming backing, even when final retained size would fit.

Reset invalidates completeness, discards old cache/pending state and retains
desired interests. A reserved coalesced notice requires exact owner acknowledgment
before fresh source admission/delivery. This is not acknowledgment by callbacks,
transport or peers. Disconnect/overflow in a transaction cannot become a complete
snapshot through reset. Destructor cleanup releases bounded owners outside turn
credits and promises neither delivery nor bounded latency.

The ordinary MAX indexed point is rejected by the locked source; MAX REMOVE is
a virtual transaction boundary. These are source-backed native policies with
separate boundary tests, not an executed Java MAX-record comparison. Wire-bound
MAX values and ordinary data-point MAX are different qualification inputs.

## Compiled caller example

The installed consumer compiles and runs the following function as part of a caller-owned driver. It is not a standalone program: the caller supplies an already-ready serialized `LiveSession`, the matching immutable schema and one complete `HISTORY_DATA` envelope. The sample uses `Sample` with indexed `TimeHigh`/`TimeLow` at indices 0/1 and ordinary `Value` at index 2. Its controlled envelope contains BEGIN at raw time 3 with value 30, then END|REMOVE at raw time 0. The function verifies both projections, complete requested-range evidence from 0 and explicit view close; it returns zero on success. Source readiness/closure, transport, callbacks and upstream demand stay with the caller. This is a generic caller-indexed HISTORY profile, not a Java HISTORY Quote/Profile producer claim.

```cpp
#include <qd/collectors/history.hpp>
#include <qd/encoding/compact.hpp>
#include <qd/encoding/utf_char.hpp>
#include <qd/encoding/utf_string.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace s = org::ttldtor::qdcxx::collectors;
namespace e = org::ttldtor::qdcxx::encoding;

/** Run one bounded local HISTORY example against an already ready serialized source.
 * The caller provides Sample/ALPHA with explicit TimeHigh/TimeLow indexing: BEGIN at3/value30
 * followed by END|REMOVE at0. All callbacks, upstream demand, transport and source closure are external.
 * Borrowed projections are inspected before pop; only detached range evidence survives acknowledgment.
 * @param source Ready matching session, owner-serialized through this call.
 * @param schema Complete matching immutable caller schema.
 * @param bytes Exactly one complete controlled HISTORY_DATA envelope.
 * @return Zero after all two projections, complete range evidence and explicit view close; otherwise failure.
 */
int runHistoryExample(p::LiveSession &source, const c::LocalSchema &schema, std::span<const std::byte> bytes) {
    auto copy = schema;
    s::HistoryWork work{1000, 1000000, 1000000, 100000000};
    const s::HistoryLimits limits{
        2, 4, 16, 4, 512 * 1024, 1, 8, 4, 512 * 1024, 2 * 1024 * 1024, {4, 8, 32, 128, 16, 256, 16}};
    auto history = s::HistoryCollector::create(std::move(copy), limits, {false, 100, 10000}, work);

    if (!history)
        return 1;

    auto view = history->openView({true}, work);

    if (!view)
        return 2;

    const c::SubscriptionRequest request{{schema.recordContext(0), u"ALPHA"}, 0};
    c::SubscriptionWork desiredWork{1000, 100000, 100000};

    if (!history->add(*view, std::span(&request, 1), desiredWork, work))
        return 3;

    p::LiveWork inputWork{10000, 10000000, 10000000, 10000000};
    auto input = source.feed(bytes, inputWork);

    if (input.status != p::LiveStatus::EVENT || input.accepted != bytes.size() || !input.event)
        return 4;

    if (!history->stage(std::move(*input.event), work))
        return 5;

    std::size_t delivered = 0;
    constexpr std::size_t maximumProjections = 4;

    for (std::size_t n = 0; n <= maximumProjections; ++n) {
        auto next = history->front(*view, work);

        if (!next)
            return 6;

        if (!*next) {
            auto status = history->rangeStatus(*view, request.key, work);

            if (!status || !status->completeRequestedRange || delivered != 2)
                return 7;

            return history->closeView(*view, work) ? 0 : 8;
        }

        if (n == maximumProjections)
            return 9; // Explicit partial drain rejection, never a complete result.

        const auto &record = (**next).record;

        if ((delivered == 0 && (record.rawTime != 3 || record.eventFlags != 4 || record.fields[2].raw != 30)) ||
            (delivered == 1 && (record.rawTime != 0 || record.eventFlags != 10)))
            return 10;

        if (!history->pop(*view, work))
            return 11;

        ++delivered;
    }

    return 12;
}
```

<a id="selector-profiles"></a>

## Canonical Exact domain

`HistoryOptions::selectorPolicy` defaults to `EXACT_ONLY`; opt-in `CANONICAL_EXACT_V1` selects the
[canonical incoming domain](SELECTORS.md). HISTORY All and the STREAM-All owner profile remain
unsupported. The whole-event domain/work/arithmetic guard runs before ownership, candidate, epoch,
stale and maintenance mutations; its refusal preserves input/state/offers. A guard resource failure
must not become shared STALE. Existing capacity-overflow, snapshot, AS-OF and reset semantics remain.
Selected parser/projection comparisons do not establish full native Java HISTORY producer equivalence.
