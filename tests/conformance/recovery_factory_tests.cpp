// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: public factory ownership/work refusal; no sockets, peers or background threads.
// Allocation injection covers this executable's ordinary throwing new/new[] only, not aligned/C/SDK allocation.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <expected>
#include <memory>
#include <new>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace p = root::protocol;
namespace m = root::mux;
namespace t = root::transport;
namespace r = root::test::recovery;
using namespace std::chrono_literals;

constexpr std::size_t MAX_FACTORY_ALLOCATIONS = 256; ///< Finite calibrated-sweep bound, not an expected count.
bool probeActive = false;        ///< Only one synchronous public create call may enable ordinary allocation counting.
std::size_t allocationCalls = 0; ///< Ordinary allocations attempted within that create call only.
std::size_t failOrdinal = 0;     ///< One-based fault selection; zero counts without injecting a failure.
bool faultInjected = false;      ///< Whether the selected ordinary allocation was actually reached.

/** Scope only ordinary allocations of a single public create; every assertion executes after this guard. */
class AllocationProbe {
    public:
    /** Count a fresh create call and optionally fail its selected ordinary allocation.
     * @param ordinal One-based allocation checkpoint, or zero for successful calibration.
     */
    explicit AllocationProbe(std::size_t ordinal) noexcept {
        if (probeActive) {
            std::terminate();
        }

        allocationCalls = 0;
        failOrdinal = ordinal;
        faultInjected = false;
        probeActive = true;
    }

    /** Disable injection before result inspection, test assertions or returned owner destruction. */
    ~AllocationProbe() {
        probeActive = false;
    }
};

/** Detect any forbidden consumer dispatch during public factory preparation or unstarted retirement. */
struct Consumer final : m::MuxConsumer {
    std::size_t callbacks = 0; ///< Actual application invocations; factory scenarios expect zero.

    /** Count an unexpected callback without throwing an assertion through the consumer boundary.
     * @param batch Borrowed data, deliberately not retained.
     * @return Continue acknowledges only this unexpected invocation.
     */
    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        (void)batch;
        ++callbacks;

        return m::BatchAction::CONTINUE;
    }
};

/** Prepare a finite compatible source before any allocator probe is enabled.
 * @param local Exact immutable source provenance.
 * @return Unbound STREAM owner with no native/source-event activity.
 */
m::MuxQueue stream(const c::LocalSchema &local) {
    auto work = r::queueWork();
    auto queue = m::MuxQueue::createStream(
        local, {16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024}, {16, 32, 128, 4096, 64, 8192, 64},
        {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
        {4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024}, work);
    REQUIRE(queue);

    return std::move(*queue);
}

/** Fresh identical caller-owned inputs; construction and registration always occur outside the probe. */
struct Fixture {
    c::LocalSchema local = r::schema(); ///< Original immutable context retained by every queue/command.
    root::client::EndpointOptions endpoint = r::endpoint(local, r::settings(4321, 3, 20ms)); ///< Unactivated snapshot.
    std::vector<p::OutgoingRecord> blueprint = r::blueprint(); ///< Caller declarations adopted only after success.
    std::shared_ptr<Consumer> consumer = std::make_shared<Consumer>(); ///< Externally retained callback.
    m::MuxHandle handle;             ///< Stable original registration, independent of later queue ownership moves.
    m::MuxCommandTicket pending;     ///< Original desired command, unapplied until an owner progresses it.
    t::RecoveryQueues queues;        ///< Actual compatible caller source owner.
    m::ClientResources clientBefore; ///< Original registration reservations/command count before create.

    /** Build all test configuration without a native run or allocation fault. */
    Fixture() {
        queues.stream.emplace(stream(local));
        auto work = r::queueWork();
        auto opened = queues.stream->open(
            {32, 512 * 1024, 8, 128 * 1024, 1, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096}, consumer, work);
        REQUIRE(opened);
        handle = std::move(*opened);
        const std::array<c::SubscriptionRequest, 1> desired{{{{local.recordContext(0), u"A"}, {}}}};
        m::QueueSubmitWork submit{10000, 1000000, 1000000, 1000000};
        auto accepted = handle.replace(desired, submit);
        REQUIRE(accepted);
        pending = std::move(*accepted);
        auto resources = queues.stream->resources(handle);
        REQUIRE(resources);
        clientBefore = *resources;
    }
};

/** Original content and backing addresses, captured outside injection to check transactional refusal. */
struct BlueprintSnapshot {
    std::vector<p::OutgoingRecord> content; ///< Independent expected values, never lent to the factory.
    const p::OutgoingRecord *records;       ///< Original caller-owned outer backing.
    std::size_t capacity;                   ///< Original caller-requested outer capacity.
    const p::OutgoingField *fields;         ///< Original first record's complete field backing.
    const char16_t *recordName; ///< Original first record name storage, including SSO storage when selected.

    /** Capture the ordinary fixture's owning declarations before a public factory attempt.
     * @param source Actual caller-owned complete declarations.
     */
    explicit BlueprintSnapshot(const std::vector<p::OutgoingRecord> &source)
        : content(source), records(source.data()), capacity(source.capacity()), fields(source[0].fields.data()),
          recordName(source[0].wireName.data()) {
    }
};

/** Public result plus actual create-call-only ordinary allocation observations. */
struct ObservedFactory {
    std::expected<t::TcpRecovery, t::RecoveryDiagnostic> result; ///< Actual public factory result, never fabricated.
    std::size_t allocations; ///< Actual ordinary allocations visited in that create call.
    bool injected;           ///< Whether the requested ordinal was reached and threw std::bad_alloc.
};

/** Run only the public factory under the executable-local allocator probe.
 * @param bootstrap Explicit process owner created before any probe.
 * @param source Complete caller inputs, prepared outside injection.
 * @param work Fresh actual preparation credits, consumed without refunds.
 * @param ordinal One-based fault checkpoint, or zero for calibration.
 * @return Actual result and observations after injection has been disabled.
 */
ObservedFactory observe(t::RuntimeBootstrap &bootstrap, Fixture &source, t::RecoveryPreparationWork &work,
                        std::size_t ordinal) {
    const auto options = r::options();
    auto result = [&] {
        AllocationProbe probe(ordinal);

        return t::TcpRecovery::create(bootstrap, source.endpoint, std::move(source.blueprint), std::move(source.queues),
                                      options, work);
    }();

    return {std::move(result), allocationCalls, faultInjected};
}

/** Check full declaration values and actual source/registration provenance after failed create.
 * @param source Original fixture, still owned by the caller.
 * @param before Captured original declaration content/backing.
 */
void checkPreserved(Fixture &source, const BlueprintSnapshot &before) {
    CHECK_FALSE(probeActive);
    REQUIRE(source.blueprint.size() == before.content.size());
    CHECK(source.blueprint.data() == before.records);
    CHECK(source.blueprint.capacity() == before.capacity);
    CHECK(source.blueprint[0].fields.data() == before.fields);
    CHECK(source.blueprint[0].wireName.data() == before.recordName);

    for (std::size_t record = 0; record != before.content.size(); ++record) {
        const auto &actual = source.blueprint[record];
        const auto &expected = before.content[record];
        CHECK(actual.senderId == expected.senderId);
        CHECK(actual.localRecordId == expected.localRecordId);
        CHECK(actual.wireName == expected.wireName);
        REQUIRE(actual.fields.size() == expected.fields.size());

        for (std::size_t field = 0; field != expected.fields.size(); ++field) {
            CHECK(actual.fields[field].wireName == expected.fields[field].wireName);
            CHECK(actual.fields[field].serialTypeId == expected.fields[field].serialTypeId);
            CHECK(actual.fields[field].localFieldId == expected.fields[field].localFieldId);
        }
    }

    REQUIRE(source.queues.stream);
    auto work = r::queueWork();
    CHECK(source.queues.stream->validateBinding(source.local, c::SubscriptionFamily::STREAM,
                                                c::SubscriptionSelectorPolicy::EXACT_ONLY, work));
    const auto client = source.queues.stream->resources(source.handle);
    REQUIRE(client);
    CHECK(client->fixedReservationBytes == source.clientBefore.fixedReservationBytes);
    CHECK(client->queuedCommands == source.clientBefore.queuedCommands);
    CHECK(client->resultSlotsInUse == source.clientBefore.resultSlotsInUse);
    CHECK_FALSE(source.pending.result());
    CHECK(source.consumer->callbacks == 0);
}

/** Retry exactly the caller inputs preserved by a failed factory, then retire the adopted owner without start.
 * @param bootstrap Original explicit process owner.
 * @param source Preserved declarations, queue and registration.
 */
void fundedRetry(t::RuntimeBootstrap &bootstrap, Fixture &source) {
    auto work = r::preparation();
    auto retry = t::TcpRecovery::create(bootstrap, source.endpoint, std::move(source.blueprint),
                                        std::move(source.queues), r::options(), work);
    REQUIRE(retry);
    CHECK(retry->status().phase == t::RecoveryPhase::CONFIGURED);
    CHECK(retry->status().attempts == 0);
    CHECK(source.blueprint.empty());
    REQUIRE(source.queues.stream);
    CHECK(source.queues.stream->state() == m::QueueState::TERMINAL);
    CHECK(source.consumer->callbacks == 0);
    r::stop(*retry);
    CHECK(retry->status().attempts == 0);
    const auto terminal = source.handle.terminal();
    REQUIRE(terminal);
    CHECK(terminal->counts.callbackStartedBatches == 0);
    CHECK(source.consumer->callbacks == 0);
}
} // namespace

/** Replace only ordinary throwing allocation in this single-threaded test executable.
 * @param bytes Requested storage, with a valid minimum for zero-sized allocations.
 * @return Ordinary malloc-aligned backing, or std::bad_alloc at the selected checkpoint/failure.
 */
void *operator new(std::size_t bytes) {
    if (probeActive) {
        ++allocationCalls;

        if (failOrdinal != 0 && allocationCalls == failOrdinal) {
            faultInjected = true;

            throw std::bad_alloc{};
        }
    }

    if (auto *pointer = std::malloc(bytes == 0 ? 1 : bytes)) {
        return pointer;
    }

    throw std::bad_alloc{};
}

/** Release backing from this executable's ordinary replacement allocation.
 * @param pointer Owned backing, or null.
 */
void operator delete(void *pointer) noexcept {
    std::free(pointer);
}

/** Release sized ordinary replacement allocation with the same underlying allocator.
 * @param pointer Owned backing, or null.
 * @param bytes Supplied size, not needed by free.
 */
void operator delete(void *pointer, std::size_t bytes) noexcept {
    (void)bytes;
    std::free(pointer);
}

/** Route array allocation through the same single ordinary-allocation checkpoint.
 * @param bytes Requested complete array backing.
 * @return Matching ordinary backing, or std::bad_alloc.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** Release replacement array backing symmetrically.
 * @param pointer Owned array backing, or null.
 */
void operator delete[](void *pointer) noexcept {
    ::operator delete(pointer);
}

/** Release sized replacement array backing symmetrically.
 * @param pointer Owned array backing, or null.
 * @param bytes Supplied backing size.
 */
void operator delete[](void *pointer, std::size_t bytes) noexcept {
    ::operator delete(pointer, bytes);
}

TEST_CASE(
    "public recovery factory ordinary allocation failures preserve caller ownership through final Impl allocation") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    std::size_t calibrated = 0;
    {
        Fixture source;
        auto work = r::preparation();
        auto observed = observe(*bootstrap, source, work, 0);
        REQUIRE(observed.result);
        REQUIRE_FALSE(observed.injected);
        calibrated = observed.allocations;
        REQUIRE(calibrated != 0);
        REQUIRE(calibrated <= MAX_FACTORY_ALLOCATIONS);
        CHECK(observed.result->status().attempts == 0);
        CHECK(source.consumer->callbacks == 0);
    }

    std::string_view finalStage;

    for (std::size_t ordinal = 1; ordinal <= calibrated; ++ordinal) {
        Fixture source;
        REQUIRE(source.blueprint.size() == 1);
        REQUIRE_FALSE(source.blueprint[0].fields.empty());
        BlueprintSnapshot before(source.blueprint);
        auto work = r::preparation();
        const auto initial = work.recovery;
        auto observed = observe(*bootstrap, source, work, ordinal);
        INFO("ordinary allocation ordinal=", ordinal, " calibrated count=", calibrated,
             " actual visited=", observed.allocations,
             " stage=", observed.result ? "success" : observed.result.error().stage);
        REQUIRE(observed.injected);
        REQUIRE_FALSE(observed.result);
        CHECK(observed.result.error().error == t::RecoveryError::ALLOCATION_FAILURE);
        CHECK(work.recovery.operations < initial.operations);
        CHECK(work.recovery.bytes < initial.bytes);
        checkPreserved(source, before);

        if (ordinal == calibrated) {
            finalStage = observed.result.error().stage;
            CHECK(finalStage == "recovery-owner-storage");
        }

        fundedRetry(*bootstrap, source);
    }

    MESSAGE("ordinary factory allocations=", calibrated, " final injected stage=", finalStage,
            "; ordinary new/new[] coverage only");
}

TEST_CASE("public recovery separates pre-entry copy work pause from terminal first native factory work refusal") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    Fixture source;
    auto preparation = r::preparation();
    auto owner = t::TcpRecovery::create(*bootstrap, source.endpoint, std::move(source.blueprint),
                                        std::move(source.queues), r::options(), preparation);
    REQUIRE(owner);
    REQUIRE(source.endpoint.settings().retry.maximumAttempts > 1);
    REQUIRE(owner->start());

    for (unsigned turn = 0; turn != 8; ++turn) {
        auto work = r::turn();
        work.recovery.bytes = 0;
        work.writer.operations = 0;
        const auto initialOperations = work.recovery.operations;
        const auto initialSession = work.session;
        auto status = owner->poll(work);
        REQUIRE(status);
        CHECK(status->attempts == 0);
        CHECK(status->first.error == t::RecoveryError::NONE);
        CHECK(work.recovery.operations < initialOperations);
        CHECK(work.recovery.bytes == 0);
        CHECK(work.session.operations == initialSession.operations);
        CHECK(work.writer.operations == 0);
    }

    std::size_t enteredRecoveryBytes = 0;
    bool entered = false;

    for (unsigned turn = 0; turn != 128 && owner->status().phase != t::RecoveryPhase::FAILED; ++turn) {
        auto work = r::turn();
        work.session.operations = 0;
        work.writer.operations = 0;
        const auto initial = work.recovery;
        const auto before = owner->status().attempts;
        auto status = owner->poll(work);
        REQUIRE(status);
        CHECK(status->attempts <= 1);
        CHECK(work.session.operations == 0);
        CHECK(work.writer.operations == 0);

        if (before == 0 && status->attempts == 1) {
            entered = true;
            enteredRecoveryBytes = initial.bytes - work.recovery.bytes;
            CHECK(work.recovery.operations < initial.operations);
        }
    }

    const auto failed = owner->status();
    INFO("phase=", static_cast<int>(failed.phase), " attempts=", failed.attempts,
         " error=", static_cast<int>(failed.first.error), " stage=", failed.first.stage,
         " transport error=", static_cast<int>(failed.first.transport.error),
         " protocolError=", static_cast<int>(failed.first.transport.protocolError));
    REQUIRE(entered);
    CHECK(enteredRecoveryBytes != 0);
    CHECK(failed.phase == t::RecoveryPhase::FAILED);
    CHECK(failed.attempts == 1);
    CHECK(owner->statistics().connectionAttempts.value == 1);
    CHECK(owner->statistics().establishedConnections.value == 0);
    CHECK(owner->statistics().closedConnections.value == 0);
    CHECK_FALSE(owner->statistics().sourceEpoch);
    CHECK(failed.first.error == t::RecoveryError::WORK_LIMIT);
    CHECK(failed.first.stage == "runtime-create-attempt");
    CHECK(failed.first.transport.error == t::TcpError::WORK_LIMIT);
    CHECK(failed.first.transport.protocolError == p::LiveError::NONE);
    CHECK(source.consumer->callbacks == 0);

    for (unsigned turn = 0; turn != 4; ++turn) {
        auto work = r::turn();
        auto status = owner->poll(work);
        REQUIRE(status);
        CHECK(status->phase == t::RecoveryPhase::FAILED);
        CHECK(status->attempts == 1);
        CHECK(status->first.stage == failed.first.stage);
    }
}
