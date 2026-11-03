# Bounded local MUX delivery queues

[Русская версия](QUEUES.ru.md). English is authoritative.

Include [`qd/mux/queue.hpp`](../../include/qd/mux/queue.hpp) and link optional `qdcxx_mux_queue`. `MuxQueue` composes one actual
[MuxState](INTERESTS.md) with one actual family collector and finite independent registrations.
The caller supplies an immutable schema, explicit capacities, work credits, consumers and complete
events admitted by the existing [LiveSession](../subscriptions/LIVE-SESSION.md). The queue performs
no decoding, I/O, background pumping, clocks, text glob expansion or wire reconciliation. Local
command/delivery completion does not establish socket writes, peer application or a full subscription facade.

## Composition and atomic boundaries

| Factory | Actual source and routing |
| --- | --- |
| `createStream` | One union [StreamCollector](../subscriptions/STREAM-COLLECTOR.md); each bounded source chunk publishes to all healthy selected clients atomically |
| `createTicker` | One shared [TickerCollector](../subscriptions/TICKER-COLLECTOR.md), with an actual independent source view per client |
| `createHistory` | One shared [HistoryCollector](../subscriptions/HISTORY-COLLECTOR.md), with an actual independent source view per client |

The queue reuses the collectors' cache/snapshot/transaction semantics. It does not fork their
algorithms or interpret business values. Detached records retain exact context, symbol, generic
full field identities, scalar bits, nullable UTF-16/bytes and family-produced flags. `source` is
optional actual raw metadata; synthesized projections do not acquire fabricated source provenance.
HISTORY `trigger` retains actual trigger metadata when present. Raw indexed bounds remain signed
64-bit values, without calendar/unit inference.

`admit` accepts a whole compatible event owner; it transfers ownership only on success. A refusal
leaves the input usable. `DATA` is staged before later desired commands; `RELEASE_ONLY` controls
are disposed in the serialized owner domain and never become consumer records. A pending owner or
mandatory phase can return `BUSY`; progress it explicitly before another admission.

`open` has no initial desired payload. It reserves the complete client maxima and prepares fixed
backing/port/result/command/control/mapping storage before publishing a handle. TICKER/HISTORY
`openView` is the last fallible component action. Failure preserves existing clients and their
desired state. The factory precreates empty internal native MUX clients, so its actual MUX revision
can already be nonzero while public generation count and aggregate demand are empty. Internal
clients are not public registrations.

## Commands, synchronization and callbacks

`MuxHandle::add/remove/replace/clear` copy bounded borrowed input before returning. Immediate
submission errors differ from a pending `MuxCommandTicket`; `result()` is empty until an immutable
result is published. `APPLIED` means authoritative desired mutation; `REJECTED` preserves the actual
`MuxError`; `CANCELLED_CLOSING` marks cancellation at the local cutoff. `APPLIED` does not establish
source synchronization: `SYNC_PENDING` must progress to the applicable active/paused/reset phase.
`aggregateEntries()` borrows the real complete desired union, not sent output, and expires on
effective mutation, retirement, move or destruction.

`progress(QueueWork&)` executes one finite fair owner action. There is no hidden worker or implicit
credit refill. A successful progress result reports its actual `QueueProgressKind`; it need not
complete a command, source synchronization, callback or shutdown.

`MuxConsumer::onBatch` receives a `BorrowedMuxBatch` valid only until return/exception. Copying the
borrow does not extend that lifetime. Call `retain()` to acquire allocation-free `OwnedMuxBatch`
ownership of already constructed immutable backing. Record/field spans remain valid while that
lease survives. Normal `CONTINUE` and `PAUSE` returns acknowledge the entire batch; `PAUSE` gates
later callbacks until `requestResume()`. Neither acknowledges per record nor preempts an executing
callback. A throwing callback is counted and never retried because its side effects may have occurred.

## Thread and lifetime contract

Owner methods, moves and destruction are externally serialized. The owner may migrate between
threads; it is not bound to the factory thread. Independent handles, tickets, control/result queries
and immutable leases support cross-thread use under their normal C++ object-lifetime rules. Moving
or modifying the same handle object concurrently still requires external synchronization.

Callbacks and final library-held consumer releases occur outside port/container locks. A strong
consumer reference and retirement reentrancy guard protect execution and final callable release;
recursive owner mutation/progress is rejected with `REENTRANT`. The execution owner must outlive
its callback: destroying that owner from its own callback is unsupported. Reserved handle controls
and `MuxQueue::requestStop` can be used from a callback as documented by the public header.

Detached ports, tickets, records, batches, epochs and their pure reservation ledger retain no live
protocol/callback execution owner. They can outlive the queue and release from another thread.
Actual backing is deallocated before its charge is removed. Retired generation reservations remain
until all corresponding actual backing is gone. Closing clears the actual native desired map and
closes the real source view before reuse; retained old backing can keep a slot unavailable after
terminal. Port origin/generation checks prevent old commands from selecting a later activation.

Last public handle release records abort without waiting. It closes admission but does not prove
callback quiescence. Observe a real `terminal()` result; an invalid/moved object's `TERMINAL` state
sentinel alone is not a completed result.

## Explicit reservations and work

All maxima are caller supplied; zero is literal, never unlimited. Factory preflight checks complete
maximum backing before queue allocation/component construction. Open preflight includes retained
old generations; candidates and old backing coexist until actual release.

| Scope | Limits |
| --- | --- |
| Aggregate registrations and event/chunk quanta | `QueueLimits::clients`, `sourceRecords`, positive `routeRecords`, `batchRecords`, `cleanupRecords` |
| Aggregate detached delivery | `dataItems`, `recordBytes`, `batchOwners`, `batchBytes` |
| Aggregate commands/results/controls | `commandSlots`, `resultSlots`, `controlSlots` (two reserved control slots per generation) |
| Complete queue request scope | `residentBytes` |
| Per-client delivery | `ClientLimits::dataItems`, `recordBytes`, `batchOwners`, `batchBytes`, positive `batchRecords` |
| Per-record exact payload | `recordFields`, `recordSymbolUnits`, `recordFieldUnits`, `recordByteUnits`, `recordArenaBytes` |
| Per-client command/result storage | `commandSlots`, `resultSlots`, `commandEntries` (including duplicates), `commandSymbolUnits` |

Record bytes include actual STL control/object requests and complete requested arena capacities.
Batch bytes include actual control/object and roots/view-array requests. Fixed queue/generation
metadata also includes actual STL control/object/vector requests. These are qualified requested
storage sizes, not RSS, allocator physical headers, total process heap or arbitrary consumer capture
graphs. Numerical control-block measurement uses the same allocator request shape before constructing
backing; supported single-request STL shapes are checked. C++ ABI exception-transport/runtime
infrastructure can allocate independently; this is not a whole-process allocation-free claim.
Ownership/control blocks are included in admission and are not an excluded overhead allowance.

Per-generation record/batch maxima reserve aggregate capacity once; actual live backing counters
remain separate from those reservations. Retaining leases/results consumes finite slots even after
delivery. `resources()` reports queue, MUX, collector and event scopes separately. Events overlap
the independently enforced session ledger; MUX/core/schema/source budgets and caller-owned copies
remain their own scopes. Adding these counters does not produce an exact total heap figure.
Concurrent detached release can make separately sampled counters describe adjacent instants;
quiescent counters are exact for their stated scopes.

`capacity()` returns the original enforced queue, MUX and collector byte ceilings,
plus the maximum number of source desired owners and their per-owner
`SubscriptionLimits`. These immutable ceilings include future growth; they are
not current usage. Desired entries, UTF-16 peaks and schema counts remain separate
logical axes. Aggregate products must use checked arithmetic in an upper owner;
per-key and per-command maxima remain individual constraints. Component byte
scopes can overlap session events and exclude independently bounded desired storage.
The byte subtotal is neither a whole-run byte bound nor process RSS. A moved-from
queue returns zeroed capacities.

`validateBinding(schema, family, policy, work)` checks exact immutable schema
provenance, including empty schemas, family and selector policy before source
ownership transfer. The queue must be open, without a stop/reset, prior source
identity, pending input or advanced source epoch. Existing registrations and
desired commands are allowed. It performs fixed, charged inspection without
allocation, callbacks or mutation; failure can consume work but preserves the queue.

`QueueSubmitWork` charges operations, visits, text units and bytes. `QueueWork` separately charges
operations, metadata visits, fields, text units, bytes, command actions and callbacks, plus actual
`MuxWork`, `SubscriptionWork` and family collector work. Initialization, rollback, disposal and
record matching are charged before their proportional actions. Batch preparation conservatively
debits twice the client batch-byte ceiling. Credits already spent are not refunded or silently
increased. `WORK_LIMIT` requires an explicit caller-selected new turn; no latency/termination
guarantee follows from repeatedly underfunded work or a callback that never returns.

## HISTORY range evidence

`rangeStatus` returns actual optional AS-OF source diagnostics plus current registration demand,
membership, bound, mapping revision, source epoch and queued/executing counts. `mappingCurrent`
requires agreement in each relevant revision domain: client demand, synchronized mapping, actual
source-view revision, bound, activation and epoch. An unrelated client's global MUX revision need
not equal this client's revision.

`completeDeliveredRange` is true only for synchronized covered clean delivered state. Unstaged DATA,
`SYNC_PENDING`, reset, a known/unclassified gap or outstanding delivery prevents that claim. Old
source diagnostics alone do not establish completeness for a newly applied request. Query work
includes exact key matching and queue visits. This is scoped native evidence, not Java HISTORY
producer equivalence or an upstream continuity guarantee.

## Phased reset and local stop

Every family supports `resetSource` as an accepted/coalesced owner operation.
The operation freezes intake/callbacks, preflights old pending disposal, discards the old pending
owner, disposes detached queues in bounded quanta, invokes actual source reset, publishes reserved
client notices, then performs the actual global ACK. `ResetStatus` exposes the phase, real source
sequence, committed/acknowledged flags and preserved failure. Bookkeeping is prepaid before
irreversible commit. TICKER global ACK may return work exhaustion after source commit: retain its
sequence and `GLOBAL_ACK_PENDING`, retry ACK with genuine new credits, and never repeat reset or
report that accepted reset as rejected. HISTORY's actual ACK has no work argument.

STREAM uses the actual collector reset and its real sequence/discard count while
preserving registrations and desired state. Its source barrier completes
synchronously with that reset, so no collector-global ACK is fabricated. Client
notices report `globalAckPending = false`; each exact client notice ACK still gates
new callbacks. The queue epoch advances once at source commit, including when
later notice publication needs a new work turn. Old caller-retained detached
batches remain valid and charged in the same ledger; they cannot become new-epoch
callbacks. The source owner must retire old protocol intake before admitting a
replacement source. Reset alone is not reconnect or uninterrupted delivery.

Global ACK and each client's exact notice ACK are separate. `acknowledgeReset(sequence)` records
an exact request for owner rechecking; stale sequences cannot acknowledge newer intervals. STOP,
RESUME and exact reset ACK use reserved controls even when data/ordinary commands are full.
Coalesced notices preserve interval evidence without counting the same source notice twice.

Graceful stop fences admission at a finite local cutoff and drains that prefix when possible.
Abort can escalate graceful before terminal publication; later graceful requests cannot downgrade it.
After publication, repeated stop requests are no-ops: the terminal state, outcome and evidence remain unchanged.
A paused/full client that cannot drain must report `FAILED`/`ABORTED`
with gap/discard evidence rather than `DRAINED`. Context/client terminals require actual local
callback, command and source-view cleanup quiescence. Destructor fallback performs finite abort
traversal without starting callbacks; arbitrary consumer-destructor work and wall time are excluded.
This does not close sockets or implement a whole-application stop protocol.

The local prefix starts at successful whole-event `admit`, including events that still require
staging or several routing turns. Decoded or socket-buffered input that was never admitted is
outside that prefix; a closing rejection leaves the input event owned by its caller. STREAM drains
selected records in source order, preserving duplicates. TICKER and HISTORY drain their actual
family projections, not one callback per raw input record. A HISTORY range that remains incomplete
cannot report `DRAINED`, even when every materialized projection was delivered and no counted item
was discarded. One paused or failing registration does not prevent healthy registrations from
draining their eligible prefix; the context still preserves the affected registration's gap or failure.

Terminal publication does not wait for application-retained `OwnedMuxBatch` values. Their records
remain readable and their allocations stay charged in resource observations until the application
releases them. `DRAINED` acknowledges normally returning callbacks, not release of caller-owned
copies, upstream delivery beyond the cutoff, or completion of arbitrary application side effects.

Diagnostic counts distinguish raw `sourceRecordsDiscarded` from per-client delivery projections.
TICKER reset `discarded` counts pending view items; HISTORY `discardedPending` counts materialized
FIFO items. Neither sums cache evictions or retained points. `hadGap` also covers unknown excluded
delivery and is not equivalent to `discarded > 0`. Counters saturate with an explicit flag. The first
non-cancellation failure stays primary through stop/retirement; later causes are bounded secondary evidence.

## Errors and integration

Immediate `QueueError` values distinguish invalid state/configuration/client/event/context/epoch,
reentrancy, busy/closing, command/result/resource/work limits, allocation failure, identity exhaustion,
callback failure and source failure. Owner operations return `QueueFailure` with an optional actual
`MuxError`, core subscription error or family collector error. A typed error does not invent a source
exception message. Failed transactions publish no partial candidate; continuing reset/closing phases
retain already committed effects rather than promising rollback across component boundaries.

[Build instructions](../../docs/CPP-BUILD.md#mux-delivery-queues) describe the CLIENT option and
installed target. [Validation](../../tests/conformance/subscriptions/MUX-VALIDATION.md#mux-queue-validation)
separates queue qualification from the historical pure-interest matrix. Existing lower collector,
desired-state and wire contracts remain authoritative within their scopes.

## Compiled STREAM integration example

This integration function was compiled and run in independent installed/relocated Windows and Linux
applications. It is a fragment for a complete caller application, not a standalone program: the caller
supplies a ready matching `LiveSession`, its immutable `LocalSchema`, and one complete admitted-input
STREAM_DATA envelope. The qualified preamble uses one local `Sample` record with **three** full type-8
compact-int32 fields `TimeHigh`, `TimeLow`, `Value`, explicit `IndexedTimePairInt32{0,1}`, sender ID 41
and exact symbol `ALPHA`. The envelope contains two identical records with raw time 1 and value 10.
This local STREAM layout does not declare a standard Quote adapter or a Java HISTORY producer.

Keep the source serialized and alive throughout the call; construct it using the existing
[LiveSession contract](../subscriptions/LIVE-SESSION.md). The returned owning batch survives this
function's queue and callback owner. The qualified caller subsequently reads/releases it on another
thread, then finishes its source session. `workForTurn` and all capacities below are explicit example
choices, not library defaults. The finite 4,096-turn loops fail explicitly instead of promising that
arbitrary caller work or source state will complete.

The following code is the exact compiled consumer prefix, including its English code comments.

```cpp
#include <qd/encoding/compact.hpp>
#include <qd/encoding/utf_char.hpp>
#include <qd/encoding/utf_string.hpp>
#include <qd/mux/queue.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace m = org::ttldtor::qdcxx::mux;
namespace e = org::ttldtor::qdcxx::encoding;

/** Explicit independent caller turn; allocation/disposal credits are never silently replenished by the library.
 * @return Fresh finite caller-selected credits for one owner action.
 */
m::QueueWork workForTurn() {
    return {100000,
            1000000000,
            1000000000,
            1000000000,
            1000000000,
            10000,
            10000,
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000}};
}

/** Run one local STREAM callback/ownership/close example against a ready serialized actual source.
 * Preamble: Sample/ALPHA has three compact fields (TimeHigh,TimeLow,Value); bytes contains two identical
 * complete STREAM records, time1/value10. No external transport/output completion is implied.
 * The callback retains immutable backing, requests graceful stop, and returns its entire borrowed batch.
 * @param source Ready matching session, serialized through the entire call.
 * @param schema Actual complete immutable matching caller schema.
 * @param bytes One complete STREAM_DATA envelope with two identical requested records.
 * @return Detached immutable batch, valid after this function and its queue/callback owner are gone.
 * @throws std::runtime_error Explicit finite pumping/admission/close failure.
 */
m::OwnedMuxBatch runMuxQueueExample(p::LiveSession &source, const c::LocalSchema &schema,
                                    std::span<const std::byte> bytes) {
    struct Consumer final : m::MuxConsumer {
        m::MuxHandle control;      ///< Pure finite control port, retaining no callback/source owner.
        m::OwnedMuxBatch retained; ///< Immutable owned records only.

        m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
            retained = batch.retain();
            control.requestStop(m::StopMode::GRACEFUL);

            return m::BatchAction::CONTINUE;
        }
    };

    auto work = workForTurn();
    auto queue =
        m::MuxQueue::createStream(schema, {4, 2, 8, 8, 64, 512, 8, 1024 * 1024}, {4, 8, 64, 512, 16, 1024, 8},
                                  {4, 16, 512 * 1024, 2 * 1024 * 1024},
                                  {2, 16, 4, 4, 4, 16, 512 * 1024, 8, 128 * 1024, 8, 16, 4, 4 * 1024 * 1024}, work);

    if (!queue)
        throw std::runtime_error("queue factory failed");

    auto receiver = std::make_shared<Consumer>();
    work = workForTurn();
    auto port = queue->open({8, 256 * 1024, 4, 64 * 1024, 4, 16, 64, 128, 256, 4096, 4, 8, 8, 512}, receiver, work);

    if (!port)
        throw std::runtime_error("registration failed");

    receiver->control = *port;
    const c::SubscriptionRequest request{{schema.recordContext(0), u"ALPHA"}, {}};
    m::QueueSubmitWork submit{100, 100000, 100000, 100000};
    auto command = port->add(std::span(&request, 1), submit);

    if (!command)
        throw std::runtime_error("desired command admission failed");

    for (unsigned turn = 0; turn < 4096 && (!command->result() || port->state() != m::QueueClientState::ACTIVE);
         ++turn) {
        work = workForTurn();

        if (!queue->progress(work))
            throw std::runtime_error("desired progress failed");
    }

    if (!command->result() || command->result()->status != m::CommandStatus::APPLIED ||
        port->state() != m::QueueClientState::ACTIVE)
        throw std::runtime_error("desired synchronization incomplete");

    p::LiveWork sourceWork{10000, 10000000, 10000000, 10000000};
    auto admitted = source.feed(bytes, sourceWork);

    if (admitted.status != p::LiveStatus::EVENT || !admitted.event || admitted.accepted != bytes.size())
        throw std::runtime_error("whole source event failed");

    bool transferred = false;

    for (unsigned turn = 0; turn < 4096 && !transferred; ++turn) {
        work = workForTurn();
        auto result = queue->admit(std::move(*admitted.event), work);

        if (result)
            transferred = true;
        else if (result.error().error != m::QueueError::BUSY)
            throw std::runtime_error("queue source admission failed");
        else if (!queue->progress(work))
            throw std::runtime_error("admission progress failed");
    }

    if (!transferred)
        throw std::runtime_error("source event was not transferred");

    for (unsigned turn = 0; turn < 4096 && !port->terminal(); ++turn) {
        work = workForTurn();

        if (!queue->progress(work))
            throw std::runtime_error("callback close progress failed");
    }

    if (!port->terminal() || port->terminal()->outcome != m::TerminalOutcome::DRAINED ||
        receiver->retained.records().size() != 2)
        throw std::runtime_error("callback cutoff did not drain");

    queue->requestStop(m::StopMode::GRACEFUL);

    for (unsigned turn = 0; turn < 4096 && !queue->terminal(); ++turn) {
        work = workForTurn();

        if (!queue->progress(work))
            throw std::runtime_error("context close progress failed");
    }

    if (!queue->terminal() || queue->terminal()->outcome != m::TerminalOutcome::DRAINED)
        throw std::runtime_error("local context did not quiesce");

    return receiver->retained;
}

```

<a id="selector-profiles"></a>

## Opt-in canonical selectors

`MuxQueue::createStream` accepts the immutable [selector policy](../subscriptions/SELECTORS.md).
The default and earlier compiled example remain `EXACT_ONLY`; explicit All uses
`CANONICAL_STREAM_ALL_V1`. TICKER/HISTORY may select canonical Exact through their options, without All.
Command/mapping copies retain selector kind. A canonical-domain failure after queue admission concerns
an already owned UNSTAGED event: normal shared-source failure/disposal preserves the nested cause,
counts and gaps. It does not undo the accepted ownership barrier. Reservations and local stop/lease
rules above remain; rebuilt consumers must account current metadata sizes.

## Discarding an uncommitted source offer

`requestDiscardPendingSource()` records a serialized allocation-free request,
including from a queue callback. It does not dispose data or change the source
epoch inline. Until subsequent funded `progress()` releases the current uncommitted
source offer, `admit()` rejects with `BUSY`. Progress performs that release before
staging, ordinary commands or further callbacks. `WORK_LIMIT` retains the request
and admission fence; already consumed work is not refunded.

The operation preserves committed collector cache, queued committed projections,
registrations, desired interests, source identity and caller-owned immutable batch
leases with their original quota charges. A source offer already committed before
the cutoff is not retroactively undone. An uncommitted STREAM routing remainder
uses its actual discard path and retains gap/loss evidence. This is neither
`resetSource()` nor final stop. The composing recovery owner keeps native input
fenced separately; see [explicit pause/reconnect](../transport/RECOVERY.md#explicit-plain-pause-and-restart).
`progressRetainedCache(work)` uses the same finite owner scheduler with an immutable
local record-origin gate. It may construct real cached INITIAL projections; only
those newly constructed in this mode may start callbacks. Old queued INITIAL/LIVE
or STREAM records are not retagged. They are discarded using genuine work and
discard/gap counts before callback selection. Work exhaustion preserves the
unconsumed prefix. The composing owner must separately fence source intake; this
method introduces no wire flags, business event source or execution thread.
Ordinary `progress()` remains unchanged. Retained values/epochs and their original
charges remain valid. Explicit final drain after a pause must keep the same origin
gate so close cannot re-enable a previously fenced live FIFO.