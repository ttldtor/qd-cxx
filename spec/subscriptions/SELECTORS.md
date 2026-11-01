# Explicit subscription selectors and canonical symbols

[Русская версия](SELECTORS.ru.md). English is authoritative.

[`SubscriptionSelectorPolicy`](../../include/qd/core/subscription.hpp) is an immutable local and
controlled-peer profile chosen before activation. It is an explicit caller assertion; descriptors,
record names, input text and remote acknowledgments do not negotiate or enable it. The default
retains legacy Exact semantics and wire bytes. No glob engine, symbol enumeration or global registry is added.

## Profiles and keys

| Profile | Fixed desired/MUX/collector owners | Subscription writer |
| --- | --- | --- |
| `EXACT_ONLY` | Existing Exact keys in STREAM/TICKER/HISTORY; no All | Existing raw FD Exact symbols and message-local FF repeats |
| `CANONICAL_EXACT_V1` | Canonical Exact domain in all three families; no All | Canonical Exact for all six subscription messages |
| `CANONICAL_STREAM_ALL_V1` | STREAM only; canonical Exact plus explicit All; TICKER/HISTORY owner creation rejects this profile | Canonical Exact for all six messages; All only in STREAM_ADD 16 / STREAM_REMOVE 17 |

An Exact key keeps its record provenance and exact UTF-16 text. Null text is invalid; non-null empty
text, embedded NUL and unpaired UTF-16 units retain existing semantics. Text beginning with `*`,
including bare `*`, stays reserved and invalid as Exact. `AB*` and `A?` are ordinary Exact strings,
without pattern interpretation. There is no new escape syntax for a literal leading asterisk.

`core::allSymbols(recordContext)` constructs `SubscriptionSelectorKind::ALL` for one known record.
It uses no desired symbol text and grants no permission by itself. The receiving STREAM owner must
already select `CANONICAL_STREAM_ALL_V1`. All is a distinct key/activation from Exact-empty and
from every concrete DATA symbol. Foreign schema contexts remain invalid. Unknown records are not
selected. All in other profiles/families is a typed `UNSUPPORTED_SELECTOR`; legacy invalid Exact
text remains `INVALID_SELECTOR`. New errors are appended without renumbering existing errors.

All occupies one desired entry and one activation, with zero desired symbol-text units. Incoming
symbol, source payload, cache, queue, per-record arena, new-symbol and work limits still apply.
The profile does not authorize unbounded materialization or reserve storage for every possible symbol.

## Canonical wire and incoming domain

The stateless [`encodePentaSymbol`](../../include/qd/encoding/penta_symbol.hpp) in `qdcxx_encoding`
encodes the pinned Java Penta domain without allocation or retained input. It examines at most seven
UTF-16 units and returns a nonzero signed32 cipher and one-to-five symbol bytes, or no encoding.
Empty text has the canonical FE representation. Non-ASCII/unsupported/unrepresentable text uses
the explicitly chosen raw-string path; exact NUL/unpaired units are preserved there. Calls are
independent and thread-safe; no SDK, reader fork, mutable dictionary or Threads dependency is added.

Canonical writer profiles use Penta for encodable Exact text and raw FD/cipher-zero for unencodable
text, with message-local repeat state. All composes the canonical Penta star under STREAM 16/17.
Legacy `EXACT_ONLY` keeps every previous FD/repeat byte. Tuple order, duplicates, sparse sender IDs,
HISTORY signed bounds, output leases and partial-write rules remain the [writer contract](WRITER.md).

Canonical collectors admit a decoded source symbol only when its actual cipher/text pair belongs
to the same domain: encodable text needs its exact Penta cipher; unencodable text needs cipher zero.
Literal FD for encodable text, including FD star, is outside this incoming profile. This is an explicit
native restriction, not a claim that Java rejects those bytes. Canonical Penta `*` in DATA is a concrete
record: a matching All receives it once. Subscription All and incoming concrete star are separate concepts.

The default legacy FD writer and a real Java collector's canonical Penta keys can inhabit different
mapping domains. Parser-level decoded-text equality alone does not establish collector-key interoperability.
The canonical profile is qualified for the selected controlled-peer path; arbitrary provider adapters
may normalize differently. No blanket provider compatibility or protocol permission response is implied.

Primary source facts are pinned in [upstream.lock](../../upstream.lock):
[PentaCodec](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/PentaCodec.java),
[BinaryQTPComposer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java),
[BinaryQTPParser](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java)
and Matrix [Stream.setEnableWildcards](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/impl/matrix/Stream.java).
These source/production observations do not establish a full network subscription facade.

## Whole-event guard and ownership

The actual STREAM/TICKER/HISTORY collectors preflight canonical-domain visits/text work and arithmetic
for the whole compatible event before ownership transfer, candidate construction, epoch pinning,
stale transitions or maintenance. Direct domain/work/arithmetic refusal leaves input, published
state and existing offers unchanged. `UNSUPPORTED_SYMBOL_DOMAIN` is distinct from capacity overflow.
In HISTORY a guard resource refusal is not converted into shared `STALE`. Other established collector
overflow/reset rules remain unchanged after a successfully guarded event enters their algorithms.

The [MuxQueue](../mux/QUEUES.md) admission barrier is earlier: an accepted UNSTAGED owner already belongs
to the queue. A later collector-domain failure follows existing shared-source failure/disposal rules,
preserves the typed nested cause and records normal gap/discard evidence. It cannot retroactively turn
successful queue admission into caller-owned rejection. No new cache/snapshot/AS-OF algorithm is introduced.

## Captured eligibility and mutation

One concrete STREAM record can match at most the original Exact activation and the original All
activation for that client. It is offered once, preserving source order and identical consecutive
records. Actual collectors capture the matched pair rather than later recomputing current desire.
Removing either original activation preserves eligibility through the other; removing both withdraws
it. Remove/re-add and reset create new scopes and cannot revive old eligibility.

`SubscriptionMembership::anyOf` is a structural OR of same-origin singleton tokens. It is not a
liveness or record-matching oracle. Invalid, foreign and already compound inputs are rejected before
duplicate handling. The actual collector must first match the two entries to that concrete record.
Tokens retain only their established pure identity backing, not source/callback owners. MUX similarly
captures the two original client-key choices. Existing serialized-owner/cross-thread immutable-token
rules and finite retained-backing reservations remain in force.

## Selection points and compatibility

Select the policy in `SubscriptionState::create`, `SubscriptionWriter::create` or `MuxState::create`.
`StreamCollector` takes the policy with its desired-state owner. TICKER/HISTORY choose
`TickerOptions::selectorPolicy` / `HistoryOptions::selectorPolicy`; only legacy or canonical Exact
is supported. `MuxQueue::createStream` takes the explicit STREAM policy; its TICKER/HISTORY factories
carry canonical Exact through those options. Related writer, desired state and incoming source must
use compatible domains selected by their owners; no runtime auto-detection repairs a mismatch.

The standalone dispatcher, `runStream` and existing CLI remain legacy `EXACT_ONLY`. Queued All is
available through `MuxQueue`; selector-kind copies preserve All instead of collapsing it to Exact-empty.
This does not add a complete subscription facade, socket-stop protocol or network demo.

Existing two-field `SubscriptionKey` aggregate initialization still means Exact because the appended
selector defaults to `EXACT`. Schema/wire record layouts remain unchanged. C++ object sizes, private
membership fields and enclosing metadata footprints change: consumers must rebuild and configure
budgets for current headers. This is source compatibility within the described cases, not a stable
binary ABI promise; the general [ABI policy](../../docs/ARCHITECTURE.md#not-decided-yet) remains undecided.

[Validation](../../tests/conformance/subscriptions/WILDCARD-VALIDATION.md) separates actual parser DATA,
selected native/Java projections, complete writer bytes, package checks and their exclusions.
The following examples use actual qualified installed consumers rather than an inferred API sketch.

## Qualified STREAM queue fragment

The code below is extracted verbatim from an installed consumer compiled/run on Windows and Linux.
It is an integration fragment without `main`. The caller supplies a ready matching serialized
`LiveSession`, immutable schema and one complete canonical STREAM frame. The qualified preamble is
one known local `Sample` record, three full type-8 compact-int32 fields `TimeHigh`, `TimeLow`, `Value`,
explicit pair layout 0/1, sender 41 and two identical Penta `ALPHA` rows, raw time 1/value 10.
It is not a standard Quote schema. Source/schema provenance and controlled-peer profile must agree.

For this conditional example prefix, define `WILDCARD_CLIENT=1` and link `qdcxx_mux_queue` in a CLIENT ON
installation. That example macro only includes the queue code; permission still comes from the explicit
factory policy. Keep source access serialized throughout the call. The callback retains an immutable
batch, requests graceful stop and returns its whole borrow. The qualified caller later reads/releases
the returned batch on another thread after the queue/callback owner has died, then finishes its source.
All capacities/work and 4,096-turn bounds are explicit example choices, not defaults or latency promises.

```cpp
#include <qd/collectors/stream.hpp>
#include <qd/encoding/compact.hpp>
#include <qd/encoding/penta_symbol.hpp>
#include <qd/encoding/utf_char.hpp>
#include <qd/encoding/utf_string.hpp>
#include <qd/protocol/live_session.hpp>
#ifdef WILDCARD_CLIENT
#    include <qd/mux/queue.hpp>
#endif

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

namespace e = org::ttldtor::qdcxx::encoding;
#ifdef WILDCARD_CLIENT
namespace m = org::ttldtor::qdcxx::mux;

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
m::OwnedMuxBatch runWildcardQueueExample(p::LiveSession &source, const c::LocalSchema &schema,
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
    auto queue = m::MuxQueue::createStream(schema, {4, 2, 8, 8, 64, 512, 8, 1024 * 1024}, {4, 8, 64, 512, 16, 1024, 8},
                                           {4, 16, 512 * 1024, 2 * 1024 * 1024},
                                           {2, 16, 4, 4, 4, 16, 512 * 1024, 8, 128 * 1024, 8, 16, 4, 4 * 1024 * 1024},
                                           work, c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);

    if (!queue)
        throw std::runtime_error("queue factory failed");

    auto receiver = std::make_shared<Consumer>();
    work = workForTurn();
    auto port = queue->open({8, 256 * 1024, 4, 64 * 1024, 4, 16, 64, 128, 256, 4096, 4, 8, 8, 512}, receiver, work);

    if (!port)
        throw std::runtime_error("registration failed");

    receiver->control = *port;
    const c::SubscriptionRequest requests[]{{c::allSymbols(schema.recordContext(0)), {}},
                                            {{schema.recordContext(0), u"ALPHA"}, {}}};
    m::QueueSubmitWork submit{100, 100000, 100000, 100000};
    auto command = port->add(requests, submit);

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

#endif

```

## Qualified pure collector fragment

This second exact function uses the header/namespace prefix above and no queue helper. Link
`qdcxx_collectors`, `qdcxx_protocol` and `qdcxx_encoding`; CLIENT can be OFF and the conditional queue
block omitted. It receives a whole already admitted canonical STREAM `LiveEvent` with the same
schema/Penta `ALPHA` preamble. The source event and collector remain serialized; a direct staging
refusal retains input ownership. Exact plus All overlap produces two delivered records for two
input records, rather than one copy per selector. Borrowed offers are inspected before `pop`.
The function checks bounded completion explicitly and throws on failure; it supplies no wire/callback ACK.

```cpp
/** Deliver one complete canonical STREAM event using a known-record explicit All plus Exact.
 * Preamble: the caller has qualified the controlled peer canonical profile, and Sample/ALPHA is a known record.
 * The whole LiveEvent and collector stay serialized; front is borrowed only until mutation/pop/destruction.
 * Fixed finite limits and one caller turn bound this example to at most sixteen admitted records.
 * @param schema Immutable actual source schema, preserving record provenance.
 * @param event Whole actual canonical STREAM source owner, consumed only on successful selection.
 * @return Actual consumed record count, or throws before claiming successful delivery.
 * @throws std::runtime_error Explicit desired/staging/offer/ack failure; no callback or wire ACK is inferred.
 */
std::size_t runWildcardCollectorExample(const c::LocalSchema &schema, p::LiveEvent &&event) {
    auto desired = c::SubscriptionState::create(schema, c::SubscriptionFamily::STREAM, {4, 8, 64, 512, 16, 1024, 8},
                                                c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);

    if (!desired)
        throw std::runtime_error("desired factory failed");

    const c::SubscriptionRequest requests[]{{c::allSymbols(schema.recordContext(0)), {}},
                                            {{schema.recordContext(0), u"ALPHA"}, {}}};
    c::SubscriptionWork command{1000, 1000000, 1000000};

    if (!desired->add(requests, command))
        throw std::runtime_error("desired add failed");

    org::ttldtor::qdcxx::collectors::StreamWork work{100000, 1000000000, 1000000000, 1000000000};
    auto collector = org::ttldtor::qdcxx::collectors::StreamCollector::create(
        std::move(*desired), {4, 16, 512 * 1024, 2 * 1024 * 1024}, work);

    if (!collector || !collector->stage(std::move(event), work))
        throw std::runtime_error("staging failed");

    std::size_t consumed = 0;

    for (unsigned index = 0; index <= 16; ++index) {
        auto front = collector->front(work);

        if (!front)
            throw std::runtime_error("offer failed");

        if (!*front)
            return consumed;

        const auto &record = *(**front).record;

        if (record.symbol != u"ALPHA" || record.typedValues.size() != 3 || record.typedValues[2].raw != 10)
            throw std::runtime_error("unexpected materialized input");

        if (!collector->pop(work))
            throw std::runtime_error("acknowledgment failed");

        ++consumed;
    }

    throw std::runtime_error("finite delivery bound exceeded");
}

```
