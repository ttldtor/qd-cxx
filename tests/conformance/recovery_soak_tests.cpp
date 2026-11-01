// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest workload: finite real recovery churn; ordinary allocation requests are not RSS or SDK heap.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace m = root::mux;
namespace t = root::transport;
namespace r = root::test::recovery;
using namespace std::chrono_literals;
constexpr std::size_t EPOCHS = 3;
constexpr std::size_t RECORDS_PER_EPOCH = 192;
constexpr std::size_t RECORD_COUNT = EPOCHS * RECORDS_PER_EPOCH;
constexpr std::size_t REQUEST_CEILING = 128 * 1024 * 1024;
constexpr auto RETRY_DELAY = 1s;
constexpr auto OBSERVATION_LIMIT = 8s;
thread_local bool measuring = false;
std::atomic<std::size_t> outstanding{};
std::atomic<std::size_t> peak{};
std::atomic<std::size_t> measuredCalls{};

/** Matching metadata for ordinary allocation requests, including allocations outside the measured interval. */
struct alignas(std::max_align_t) AllocationHeader {
    std::size_t bytes; ///< Original request, excluding this test-only metadata.
    bool measured;     ///< Whether this ordinary request belongs to the measured main-thread interval.
};

/** Count only creating-thread ordinary new/new[]; SDK C allocation and aligned allocation are excluded.
 * @param bytes Requested ordinary allocation size.
 * @return Correctly aligned user block, or bad_alloc on overflow, cap or malloc failure.
 */
void *allocate(std::size_t bytes) {
    if (bytes > std::numeric_limits<std::size_t>::max() - sizeof(AllocationHeader) ||
        (measuring && bytes > REQUEST_CEILING))
        throw std::bad_alloc();

    auto *header = static_cast<AllocationHeader *>(std::malloc(sizeof(AllocationHeader) + bytes));

    if (!header)
        throw std::bad_alloc();

    header->bytes = bytes;
    header->measured = measuring;

    if (measuring) {
        const auto current = outstanding.fetch_add(bytes) + bytes;

        if (current > REQUEST_CEILING) {
            outstanding.fetch_sub(bytes);
            std::free(header);
            throw std::bad_alloc();
        }

        measuredCalls.fetch_add(1);
        auto previous = peak.load();

        while (previous < current && !peak.compare_exchange_weak(previous, current)) {
        }
    }

    return header + 1;
}

/** Release exactly the matching header, even if destruction occurs after measurement or on a peer thread.
 * @param block Ordinary allocation result or null.
 */
void release(void *block) noexcept {
    if (!block)
        return;

    auto *header = static_cast<AllocationHeader *>(block) - 1;

    if (header->measured)
        outstanding.fetch_sub(header->bytes);

    std::free(header);
}

/** Scope main-thread allocation measurement; assertions and application bookkeeping may also be included. */
struct Measurement {
    /** Enter the one externally serialized measurement interval. */
    Measurement() {
        if (measuring)
            std::terminate();

        measuring = true;
    }

    /** Leave new requests unmeasured; existing measured blocks still debit when deleted. */
    ~Measurement() {
        measuring = false;
    }
};

/** Fixed application observations, with only the first immutable batch deliberately retained. */
struct Recorder final : m::MuxConsumer {
    std::array<std::int64_t, RECORD_COUNT> values{}; ///< Fixed application memory, allocated before measurement.
    std::size_t records{};                           ///< Actual callback record count, including overflow diagnostics.
    std::size_t callbacks{};                         ///< Actual callback invocations.
    bool pause{};                                    ///< Selected deliberately slow registration.
    std::optional<m::OwnedMuxBatch> retained;        ///< At most one application-owned detached batch.

    /** Observe exact raw data, retaining only one batch rather than every delivered record.
     * @param batch Callback-only view, never saved as a borrow.
     * @return PAUSE for the designated slow consumer, CONTINUE for both healthy consumers.
     */
    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        ++callbacks;

        if (!retained)
            retained = batch.retain();

        for (const auto &record : batch.records()) {
            if (records < values.size() && record.typedValues.size() == 3)
                values[records] = record.typedValues[2].raw;

            ++records;
        }

        return pause ? m::BatchAction::PAUSE : m::BatchAction::CONTINUE;
    }
};

/** Give each operation fresh finite credits, keeping actual callback/decode quotas small.
 * @return A funded turn with at most sixteen LiveSession operations and two STREAM callbacks.
 */
t::RecoveryTurnWork work() {
    auto result = r::turn();
    result.session.operations = 16;
    result.queues[1].callbacks = 2;

    return result;
}

/** Pump actual owner turns; no wall-clock throughput target is inferred.
 * @tparam Predicate Monotonic test observation.
 * @param owner Public owner.
 * @param predicate Required bounded observation.
 * @param turns Actual poll counter.
 * @param limit Finite observation deadline.
 */
template <class Predicate>
void until(t::TcpRecovery &owner, Predicate predicate, std::size_t &turns,
           std::chrono::milliseconds limit = OBSERVATION_LIMIT) {
    const auto deadline = std::chrono::steady_clock::now() + limit;

    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        auto credits = work();
        const auto result = owner.poll(credits);
        REQUIRE(result);
        ++turns;

        if (result->phase == t::RecoveryPhase::FAILED || result->phase == t::RecoveryPhase::EXHAUSTED)
            break;

        std::this_thread::yield();
    }

    const auto status = owner.status();
    INFO("phase=", static_cast<int>(status.phase), " attempts=", status.attempts, " stage=", status.first.stage,
         " error=", static_cast<int>(status.first.error));
    REQUIRE(predicate());
}

/** Submit one exact key; the command ticket is released before another command is submitted.
 * @param handle Actual registration.
 * @param local Exact schema provenance.
 * @param symbol Explicit desired key.
 * @return Accepted detached command.
 */
m::MuxCommandTicket replace(m::MuxHandle &handle, const c::LocalSchema &local, std::u16string_view symbol) {
    const c::SubscriptionRequest input{{local.recordContext(0), symbol}, {}};
    m::QueueSubmitWork submit{10000, 1000000, 1000000, 1000000};
    auto result = handle.replace(std::span(&input, 1), submit);
    REQUIRE(result);

    return std::move(*result);
}
} // namespace

void *operator new(std::size_t bytes) {
    return allocate(bytes);
}

void *operator new[](std::size_t bytes) {
    return allocate(bytes);
}

void operator delete(void *block) noexcept {
    release(block);
}

void operator delete[](void *block) noexcept {
    release(block);
}

void operator delete(void *block, std::size_t) noexcept {
    release(block);
}

void operator delete[](void *block, std::size_t) noexcept {
    release(block);
}

/** Run and completely tear down one finite native workload, before repeating it with new owners and sockets. */
void runScenario() {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto local = r::schema();
    root::test::TcpPeerConfig script;
    script.lifetime = 30s;
    script.sessions.resize(EPOCHS);
    constexpr std::array<std::u16string_view, EPOCHS> SYMBOLS{u"A", u"B", u"C"};

    for (std::size_t epoch = 0; epoch != EPOCHS; ++epoch) {
        auto &session = script.sessions[epoch];
        session.initialBytes = r::ready();
        r::append(session.initialBytes, r::description(static_cast<std::int32_t>(41 + epoch)));
        session.clientBytesToRead = 1;
        session.waitForControllerClose = true;

        for (std::size_t ordinal = 0; ordinal != RECORDS_PER_EPOCH; ++ordinal)
            r::append(session.replyBytes, r::data(static_cast<std::int32_t>(41 + epoch), SYMBOLS[epoch],
                                                  static_cast<std::int32_t>(epoch * RECORDS_PER_EPOCH + ordinal)));
    }

    root::test::OwnedTcpPeer peer(std::move(script));
    REQUIRE(peer.port() != 0);
    auto fastA = std::make_shared<Recorder>();
    auto fastB = std::make_shared<Recorder>();
    auto slow = std::make_shared<Recorder>();
    slow->pause = true;
    std::weak_ptr<Recorder> weakA = fastA, weakB = fastB, weakSlow = slow;
    // Warm the test framework's retained INFO-context stack outside the production interval.
    {
        INFO("outer allocation-control context");
        {
            INFO("nested allocation-control context");
            REQUIRE(outstanding.load() == 0);
        }
    }
    std::size_t turns = 0, churnCommands = 0, pausedTurns = 0;
    const auto started = std::chrono::steady_clock::now();
    {
        Measurement scope;
        auto *control = ::operator new(97);
        const auto calibration = outstanding.load();
        ::operator delete(control);
        REQUIRE(calibration == 97);
        REQUIRE(outstanding.load() == 0);
        bool refused = false;
        try {
            auto *excess = ::operator new(REQUEST_CEILING + 1);
            ::operator delete(excess);
        } catch (const std::bad_alloc &) {
            refused = true;
        }
        REQUIRE(refused);
        REQUIRE(outstanding.load() == 0);
        peak.store(0);
        measuredCalls.store(0);
        {
            auto creation = r::queueWork();
            const m::QueueLimits bounds{
                4, 4, 2, 1, 4, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024};
            auto queue = m::MuxQueue::createStream(local, {16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024},
                                                   {16, 32, 128, 4096, 64, 8192, 64},
                                                   {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024}, bounds, creation);
            REQUIRE(queue);
            const m::ClientLimits client{32, 512 * 1024, 8, 128 * 1024, 1, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096};
            auto opening = r::queueWork();
            auto handleA = queue->open(client, fastA, opening);
            opening = r::queueWork();
            auto handleB = queue->open(client, fastB, opening);
            auto stalledLimits = client;
            stalledLimits.dataItems = 2;
            opening = r::queueWork();
            auto stalled = queue->open(stalledLimits, slow, opening);
            REQUIRE(handleA);
            REQUIRE(handleB);
            REQUIRE(stalled);
            auto initialA = replace(*handleA, local, SYMBOLS[0]);
            auto initialB = replace(*handleB, local, SYMBOLS[0]);
            auto initialSlow = replace(*stalled, local, SYMBOLS[0]);

            // A peer may publish after the first union ADD. Require every tested registration's own
            // authoritative command barrier before network activation, rather than assuming atomic multi-client ADD.
            for (unsigned count = 0;
                 count != 200 && (!initialA.result() || !initialB.result() || !initialSlow.result()); ++count) {
                auto applying = r::queueWork();
                REQUIRE(queue->progress(applying));
            }

            REQUIRE(initialA.result());
            REQUIRE(initialB.result());
            REQUIRE(initialSlow.result());
            REQUIRE(initialA.result()->status == m::CommandStatus::APPLIED);
            REQUIRE(initialB.result()->status == m::CommandStatus::APPLIED);
            REQUIRE(initialSlow.result()->status == m::CommandStatus::APPLIED);
            auto settings = r::settings(peer.port(), EPOCHS, RETRY_DELAY);
            settings.timeouts.read = 10s;
            auto endpoint = r::endpoint(local, settings);
            t::RecoveryQueues queues;
            queues.stream = std::move(*queue);
            auto preparing = r::preparation();
            auto owner = t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), r::options(),
                                                preparing);
            REQUIRE(owner);
            const auto capacity = owner->capacity();
            CHECK(capacity.queues[1].queueBytes == bounds.residentBytes);
            REQUIRE(owner->start());

            for (std::size_t epoch = 0; epoch != EPOCHS; ++epoch) {
                if (epoch) {
                    until(
                        *owner,
                        [&] {
                            return owner->status().phase == t::RecoveryPhase::BACKOFF;
                        },
                        turns);
                    REQUIRE(owner->status().attempts == epoch);

                    for (unsigned change = 0; change != 12; ++change) {
                        const auto symbol = change % 2 == 0 ? u"REMOVED" : SYMBOLS[epoch];
                        auto changedA = replace(*handleA, local, symbol);
                        auto changedB = replace(*handleB, local, symbol);
                        until(
                            *owner,
                            [&] {
                                return changedA.result() && changedB.result();
                            },
                            turns, 750ms);
                        REQUIRE(changedA.result()->status == m::CommandStatus::APPLIED);
                        REQUIRE(changedB.result()->status == m::CommandStatus::APPLIED);
                        REQUIRE(owner->status().phase == t::RecoveryPhase::BACKOFF);
                        churnCommands += 2;
                    }

                    const auto noticeA = handleA->resetNotice();
                    const auto noticeB = handleB->resetNotice();
                    REQUIRE(noticeA);
                    REQUIRE(noticeB);
                    CHECK(handleA->acknowledgeReset(noticeA->sequence) == m::ControlAdmission::RECORDED);
                    CHECK(handleB->acknowledgeReset(noticeB->sequence) == m::ControlAdmission::RECORDED);
                    // Authoritative offline changes are complete. Wait for the actual finite retry hint
                    // without burning millions of caller turns while no native attempt exists.
                    const auto retry = owner->nextWake();
                    REQUIRE(retry);
                    std::this_thread::sleep_until(*retry);
                }

                const auto expected = (epoch + 1) * RECORDS_PER_EPOCH;
                INFO("A-records=", fastA->records, " B-records=", fastB->records, " slow-records=", slow->records,
                     " A-state=", static_cast<int>(handleA->state()), " B-state=", static_cast<int>(handleB->state()));
                until(
                    *owner,
                    [&] {
                        return fastA->records == expected && fastB->records == expected;
                    },
                    turns);
                CHECK(owner->status().attempts == epoch + 1);
                CHECK(owner->status().phase == t::RecoveryPhase::ACTIVE);
                CHECK(owner->capacity().queueBytes == capacity.queueBytes);
                CHECK(owner->capacity().replayBytes == capacity.replayBytes);

                if (!epoch) {
                    REQUIRE(stalled->terminal());
                    REQUIRE(stalled->terminal()->primary);
                    CHECK(stalled->terminal()->primary->error == m::QueueError::RESOURCE_LIMIT);
                    CHECK(slow->callbacks == 1);
                    const auto before = slow->records;

                    for (unsigned count = 0; count != 64; ++count) {
                        auto credits = work();
                        REQUIRE(owner->poll(credits));
                        ++turns;
                        ++pausedTurns;
                    }

                    CHECK(slow->records == before);
                }

                if (epoch + 1 < EPOCHS)
                    peer.allowClose(epoch);
            }

            for (std::size_t ordinal = 0; ordinal != RECORD_COUNT; ++ordinal) {
                CHECK(fastA->values[ordinal] == static_cast<std::int64_t>(ordinal));
                CHECK(fastB->values[ordinal] == fastA->values[ordinal]);
            }

            REQUIRE(handleA->requestStop(m::StopMode::GRACEFUL) == m::ControlAdmission::RECORDED);
            REQUIRE(handleB->requestStop(m::StopMode::GRACEFUL) == m::ControlAdmission::RECORDED);
            until(
                *owner,
                [&] {
                    return handleA->terminal() && handleB->terminal();
                },
                turns);
            CHECK(handleA->terminal()->outcome == m::TerminalOutcome::ABORTED);
            CHECK(handleB->terminal()->outcome == m::TerminalOutcome::ABORTED);
            CHECK(handleA->terminal()->hadGap);
            CHECK(handleB->terminal()->hadGap);
            CHECK_FALSE(handleA->terminal()->primary);
            CHECK_FALSE(handleB->terminal()->primary);
            REQUIRE(owner->requestStop());
            until(
                *owner,
                [&] {
                    return owner->status().phase == t::RecoveryPhase::CLOSED;
                },
                turns);
            const auto callbacks = fastA->callbacks + fastB->callbacks + slow->callbacks;

            for (unsigned count = 0; count != 64; ++count) {
                auto credits = work();
                REQUIRE(owner->poll(credits));
                ++turns;
            }

            CHECK(fastA->callbacks + fastB->callbacks + slow->callbacks == callbacks);
            CHECK_FALSE(owner->nextWake());
            REQUIRE(fastA->retained);
            REQUIRE(fastB->retained);
            REQUIRE(slow->retained);
            CHECK(fastA->retained->records().size() == 1);
            CHECK(fastB->retained->records().size() == 1);
            CHECK(slow->retained->records().size() == 1);
            CHECK(fastA->retained->records()[0].typedValues[2].raw == 0);
            fastA->retained.reset();
            fastB->retained.reset();
            slow->retained.reset();
        }
        fastA.reset();
        fastB.reset();
        slow.reset();
    }
    const auto remaining = outstanding.load();
    const auto highWater = peak.load();
    const auto calls = measuredCalls.load();
    const auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    peer.stop();
    CHECK(peer.snapshot().error == root::test::TcpPeerError::NONE);

    for (const auto &session : peer.snapshot().sessions) {
        CHECK(session.accepted);
        CHECK(session.replyBytesWritten != 0);
    }

    CHECK(weakA.expired());
    CHECK(weakB.expired());
    CHECK(weakSlow.expired());
    weakA.reset();
    weakB.reset();
    weakSlow.reset();
    CHECK(remaining == 0);
    CHECK(highWater <= REQUEST_CEILING);
    CHECK(calls > RECORD_COUNT);
    CHECK(turns > RECORD_COUNT);
    CHECK(churnCommands == 48);
    CHECK(pausedTurns == 64);
    std::cout << "workload epochs=" << EPOCHS << " records-per-epoch=" << RECORDS_PER_EPOCH
              << " total-records=" << RECORD_COUNT << " healthy-clients=2 paused-clients=1"
              << " schema=R-indexed-three-int32 rate=bounded-per-connection-burst retry-ms=1000"
              << " healthy-client-data-items=32 slow-client-data-items=2 app-retained-batches-per-client=1"
              << " source-records-per-event=4 route-records=2 batch-records=1 session-operations-per-poll=16"
              << " callbacks-per-poll=2 poll-turns=" << turns << " offline-commands=" << churnCommands
              << " duration-ms=" << duration.count() << " ordinary-request-peak=" << highWater
              << " ordinary-request-ceiling=" << REQUEST_CEILING << " ordinary-outstanding-after-teardown=" << remaining
              << " ordinary-allocation-calls=" << calls << '\n';
}

TEST_CASE("bounded native recovery sustains traffic and recovers its measured request scopes twice") {
    runScenario();
    runScenario();
}
