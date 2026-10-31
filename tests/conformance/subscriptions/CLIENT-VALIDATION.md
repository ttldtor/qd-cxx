# Local STREAM dispatch validation

[Русская версия](CLIENT-VALIDATION.ru.md). English is authoritative.

The `client_dispatch` native test exercises the [local dispatcher contract](../../../spec/subscriptions/CLIENT-DISPATCH.md)
through real `LiveSession` events and the STREAM collector. Enable
`ORG_TTLDTOR_QDCXX_BUILD_CLIENT` to build the test. Runtime and JSON may remain OFF.

The final-source CLIENT ON / RUNTIME OFF / JSON OFF profile passes **42/42 CTests**
on Windows x64 MSVC 19.42 Release and Linux x86-64 GCC 13.3 Debug ASan/UBSan.
Main-suite LeakSanitizer is OFF. Both platforms pass 27 client cases / 2949 assertions
and 21 collector cases / 21724 assertions. Complete captured Linux test output has
no sanitizer diagnostics; this is execution evidence for these inputs, not a proof
that every possible interleaving is safe.

A separate GCC 13.3 Debug ThreadSanitizer build passes nine focused ownership/
cross-thread cases with 1367 assertions and no diagnostics. Strict halt-on-error
UBSan checks additionally pass four client cases / 559 assertions and the collector
move case / 42 assertions. These focused runs are separate from the full matrix.

Production-only builds and real relocated installed consumers pass on both platforms.
Each 29-file package preserves every file hash after relocation, including a custom
`lib64` and `public include` layout. The consumer retains a batch, requests graceful
stop, observes local terminal and callable retirement, then releases detached data
and a completed ticket on another thread. It requires no runtime/JSON SDK.

## Behavioral coverage

- Multi-record self-remove, remove/re-add and replacement preserve the executing batch
  while retiring later old activations. Recursive owner progress is rejected.
- Graceful stop drains the admitted eligible prefix; abort reports discarded and
  unclassified input. Callback exceptions are not retried or replaced by later stop.
- Last public release on another thread cannot complete an executing callback.
  Fresh handle acquisition and final release have one linearized lifetime outcome;
  copying an existing closing handle remains available for terminal observation.
- Accepted events survive staging/release work exhaustion. Pending staging precedes
  ordinary changes; a zero-selection release retry does not replay pre-add records.
- Ordinary queue/result saturation cannot prevent reserved stop. Completed tickets
  survive dispatcher destruction without slot reuse or retained live operations.
- Zero ordinary capacities remain finite and preserve reserved stop; checked size
  products accept zero without division by zero. Failed live frames never expose
  their partially decoded record prefix as a callback.
- Detached batches preserve exact contexts, membership, generation, nullable UTF-16,
  opaque bytes and raw metadata after all protocol owners disappear. Their final
  release on another thread returns the atomic reservation.
- Retained-batch capacity, candidate allocation, arena/work exhaustion and factory
  allocation failures are explicit. No earlier prefix is acknowledged by a failed copy.
- Producer command text is copied before return, including empty, NUL and unpaired
  UTF-16 units. Concurrent producers never access mutable protocol/collector state.
- Independent schema contexts reject interchange; a drained queue does not admit
  a foreign epoch. Final library-held callable destruction can inspect the closed
  port outside mutation locks.
- The full standard Quote Java fixture reaches a callback and the existing prepared
  Quote mapping. This also checks the explicit external subscription-output route.

The collector's separate `collectors_stream` test covers atomic batch preparation
and acknowledgement, stale single/batch interleavings, zero/withdrawn prefixes and
move invalidation. Existing single-record move behavior remains unchanged.

## Qualification limits

The dispatcher owns no socket or source session. Local terminal does not establish
transport/output/partial-frame quiescence. A controlled TCP client, TLS, reconnect,
shared MUX and full subscriptions require their separate integration checks.
Caller-created Consumer allocations/captured graphs and independent deep copies
are explicit application scopes. Requested backing/work counts do not measure RSS,
allocator overhead or callback execution time.

## Compiled ownership example

This conformance excerpt is compiled and executed by the relocated consumers on both
platforms. It assumes an externally owned ready session, matching schema/local record
0, and one complete STREAM_DATA frame containing ALPHA with value 10. The explicit
capacities and finite loop bounds are example configuration, not library defaults.
It demonstrates local callbacks/retention/stop; it does not create a network client.
The source session and its wire output remain the caller's responsibility. Return 0
means the checked local lifecycle completed; other return values identify a failed step.

```cpp
#include <qd/client/stream_dispatcher.hpp>
#include <atomic>
#include <cstddef>
#include <memory>
#include <span>
#include <thread>
#include <utility>

namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace d = org::ttldtor::qdcxx::client;

/** Supply explicit finite example work, never library defaults.
 * @return Owner turn shared by all example progress calls.
 */
d::DispatchWork ownerTurn() {
    return {128,
            64,
            16,
            100000,
            100000,
            100000,
            16 * 1024 * 1024,
            {64, 100000, 100000},
            {128, 100000, 100000, 16 * 1024 * 1024}};
}

/** Application-created callback state; library-held ownership ends before terminal publication. */
struct SampleConsumer final : d::Consumer {
    d::ClientHandle handle;
    d::OwnedBatch retained;
    std::size_t calls = 0;
    bool valid = true;
    bool requested = false;

    /** Retain independent immutable data and request graceful stop without waiting.
     * @param batch Callback-only borrowed values, retained explicitly for later thread-safe use.
     */
    void onBatch(const d::BorrowedBatch &batch) override {
        ++calls;
        retained = batch.retain();

        for (const auto &record : batch.records()) {
            const auto value = record.typedValues.empty() ? record.values[0] : record.typedValues[0].raw;
            valid = valid && record.symbol == u"ALPHA" && value == 10;
        }

        requested = handle.requestStop(d::StopMode::GRACEFUL).has_value();
    }
};

/** Run one local consumer over an actual externally owned ready protocol session.
 * The caller supplies exactly one complete STREAM_DATA envelope containing one ALPHA record.
 * Explicit example capacities are not library defaults. No callback/owner waits for itself.
 * @param session Actual serialized protocol driver; this function does not duplicate it.
 * @param schema Matching immutable local provenance.
 * @param bytes One whole controlled source frame.
 * @return Zero only after local terminal, callable retirement and foreign-thread detached release; nonzero on failure.
 */
int runClientExample(p::LiveSession &session, const c::LocalSchema &schema, std::span<const std::byte> bytes) {
    d::OwnedBatch retained;
    d::CommandTicket completed;
    std::weak_ptr<SampleConsumer> callable;
    const auto context = schema.recordContext(0);
    {
        auto consumer = std::make_shared<SampleConsumer>();
        callable = consumer;
        auto work = ownerTurn();
        const d::ClientLimits limits{2, 4, 4, 32, 4, 8, 64, 128, 128, 8192, 2, 32768, 131072};
        auto dispatcher = d::StreamDispatcher::create(schema, {1, 2, 16, 32, 4, 64, 4}, {4, 8, 131072, 262144}, limits,
                                                      consumer, work);

        if (!dispatcher) {
            return 1;
        }

        auto handle = dispatcher->handle();

        if (!handle) {
            return 2;
        }

        consumer->handle = *handle;
        const c::SubscriptionRequest request{{context, u"ALPHA"}, {}};
        d::SubmitWork submit{8, 128, 128};
        auto command = handle->add(std::span(&request, 1), submit);

        if (!command) {
            return 3;
        }

        for (unsigned step = 0; step < 16 && !command->result(); ++step) {
            if (!dispatcher->progress(work)) {
                return 4;
            }
        }

        if (!command->result() || command->result()->status != d::CommandStatus::APPLIED) {
            return 5;
        }

        // Applied is local desired state, never automatic wire emission or peer acknowledgement.
        p::LiveWork inputWork{16, 1000000, 1000000, 1000000};
        auto event = session.feed(bytes, inputWork);

        if (event.status != p::LiveStatus::EVENT || event.accepted != bytes.size() || !event.event) {
            return 6;
        }

        // Hand the source event over before any later ordinary command progress.
        if (!dispatcher->admit(std::move(*event.event))) {
            return 7;
        }

        for (unsigned step = 0; step < 32 && !handle->terminal(); ++step) {
            if (!dispatcher->progress(work)) {
                return 8;
            }
        }

        const auto terminal = handle->terminal();

        if (!terminal || terminal->outcome != d::DispatchOutcome::DRAINED || terminal->counts.completedRecords != 1 ||
            !consumer->valid || !consumer->requested || consumer->calls != 1) {
            return 9;
        }

        retained = std::move(consumer->retained);
        completed = *command;
    }

    // Dispatcher and callable are gone. Completed tickets/batches retain no live operation or callback.
    if (!callable.expired()) {
        return 10;
    }

    std::atomic<bool> valid{false};
    std::jthread producer([batch = std::move(retained), ticket = std::move(completed), context, &valid]() mutable {
        valid = batch.records().size() == 1 && batch.records()[0].context.matches(context) &&
                batch.records()[0].symbol == u"ALPHA" && ticket.result().has_value() &&
                ticket.result()->status == d::CommandStatus::APPLIED;
        batch = {};
        ticket = {};
    });
    producer.join();

    return valid.load() ? 0 : 11;
}
```
