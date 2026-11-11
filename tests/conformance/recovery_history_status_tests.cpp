// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: production RecoveryCore and HISTORY graph with a controlled private backend.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_core.hpp"
#include "recovery_history_status_support.hpp"

#include <memory>
#include <thread>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace d = root::client;
namespace p = root::protocol;
namespace m = root::mux;
namespace t = root::transport;
namespace detail = t::detail;
namespace r = root::test::recovery;
namespace h = root::test::history_status;
using namespace std::chrono_literals;
constexpr unsigned MAXIMUM_INITIAL_COMMAND_TURNS = 128; ///< Finite preparation guard before queue adoption.

/** Controlled adapter only; decoder, scheduler, writer, queue and collector are the actual production types. */
class Backend final : public detail::RecoveryBackend {
    std::optional<p::LiveSession> session_; ///< Pure protocol epoch, outliving any event transferred to Core.

    public:
    detail::RecoveryAttemptFacts actual{};         ///< Test-controlled actual facts, distinct from stored model phase.
    detail::RecoveryAttemptFailure failureValue{}; ///< Test-controlled native refusal classification.
    std::optional<p::LiveEvent> next;              ///< At most one actual admitted event awaiting backend poll.
    m::MuxQueue *history = nullptr;     ///< Borrowed Core-owned queue for independent direct-query comparison only.
    mutable std::size_t factsReads = 0; ///< Read-only observation calls; no I/O is represented.
    std::size_t advancingCalls = 0;     ///< Prepare/start/poll/compose/abort/dispose calls, excluding facts.
    std::size_t eventTransfers = 0;     ///< Actual owning events handed to Core.
    bool deferAuthentication = false;   ///< Reproduce WORK_LIMIT after an actual authentication-state change.
    bool deferSubscriptions = false;    ///< Hold actual replay before its admission/bank commit.
    std::optional<std::pair<p::SubscriptionMessage, std::optional<std::int64_t>>> lastSubscription;
    std::vector<std::pair<p::SubscriptionMessage, std::optional<std::int64_t>>> admittedSubscriptions;

    /** Decode a real complete frame through the private backend's actual pure-session epoch.
     * @param bytes Complete independent binary frame.
     * @return Actual owning admitted event with unchanged production provenance.
     */
    p::LiveEvent decode(std::span<const std::byte> bytes) {
        REQUIRE(session_);
        auto work = r::turn().session;
        auto event = session_->feed(bytes, work);
        INFO("live error=", static_cast<int>(session_->diagnostic().error), " stage=", session_->diagnostic().stage);
        REQUIRE(event.status == p::LiveStatus::EVENT);
        REQUIRE(event.accepted == bytes.size());
        REQUIRE(event.event);

        return std::move(*event.event);
    }

    /** @copydoc detail::RecoveryBackend::prepare */
    std::expected<void, t::RecoveryDiagnostic> prepare(detail::RecoveryConfiguration &configuration,
                                                       p::SubscriptionWriter &&writer,
                                                       t::RecoveryTurnWork &work) override {
        ++advancingCalls;
        auto schema = configuration.endpoint.schema();
        auto session = p::LiveSession::create(std::move(schema), std::move(writer),
                                              configuration.endpoint.settings().budgets.session, work.session);
        REQUIRE(session);
        session_.emplace(std::move(*session));
        REQUIRE_FALSE(next);
        next.emplace(decode(r::ready()));
        (void)decode(r::description(41));
        history = configuration.queues.history ? &*configuration.queues.history : nullptr;
        actual = {
            true, false, true, false, session_->peer().identity().generation(), p::LiveAuthentication::UNCHALLENGED, 0};

        return {};
    }

    /** @copydoc detail::RecoveryBackend::start */
    std::expected<void, t::RecoveryDiagnostic> start() override {
        ++advancingCalls;
        actual.connected = true;

        return {};
    }

    /** @copydoc detail::RecoveryBackend::poll */
    std::expected<t::TcpPollResult, t::RecoveryDiagnostic> poll(t::RecoveryTurnWork &, bool decodeInput) override {
        ++advancingCalls;

        if (deferAuthentication) {
            actual.authenticated = false;
            actual.authentication = p::LiveAuthentication::REQUIRED;
            deferAuthentication = false;

            return std::unexpected(
                t::RecoveryDiagnostic{t::RecoveryError::WORK_LIMIT, "controlled-authentication-work"});
        }

        if (next && decodeInput) {
            t::TcpPollResult result{t::TcpPollStatus::EVENT, std::move(next)};
            next.reset();
            ++eventTransfers;

            return result;
        }

        return t::TcpPollResult{};
    }

    /** @copydoc detail::RecoveryBackend::protocol */
    std::expected<void, t::RecoveryDiagnostic> protocol(p::LiveProtocolView, p::LiveWork &) override {
        ++advancingCalls;

        return {};
    }

    /** @copydoc detail::RecoveryBackend::describe */
    std::expected<void, t::RecoveryDiagnostic> describe(std::span<const std::int32_t>, p::LiveWork &) override {
        ++advancingCalls;

        return {};
    }

    /** @copydoc detail::RecoveryBackend::subscribe */
    std::expected<void, t::RecoveryDiagnostic> subscribe(p::SubscriptionMessage message,
                                                         std::span<const c::SubscriptionRequest> requests,
                                                         p::LiveWork &) override {
        ++advancingCalls;
        REQUIRE(requests.size() == 1);
        lastSubscription = std::pair{message, requests[0].historyBound};

        if (deferSubscriptions)
            return std::unexpected(t::RecoveryDiagnostic{t::RecoveryError::WORK_LIMIT, "controlled-replay-work"});

        admittedSubscriptions.push_back(*lastSubscription);

        return {};
    }

    /** @copydoc detail::RecoveryBackend::heartbeat */
    std::expected<void, t::RecoveryDiagnostic> heartbeat(p::LiveWork &) override {
        ++advancingCalls;

        return {};
    }

    /** @copydoc detail::RecoveryBackend::abort */
    void abort() noexcept override {
        ++advancingCalls;
        actual.terminal = actual.present;
        actual.connected = false;
    }

    /** @copydoc detail::RecoveryBackend::dispose */
    void dispose() noexcept override {
        ++advancingCalls;
        next.reset();
        session_.reset();
        actual = {};
    }

    /** @copydoc detail::RecoveryBackend::facts */
    detail::RecoveryAttemptFacts facts() const noexcept override {
        ++factsReads;

        return actual;
    }

    /** @copydoc detail::RecoveryBackend::failure */
    detail::RecoveryAttemptFailure failure() const noexcept override {
        return failureValue;
    }

    /** @copydoc detail::RecoveryBackend::wireTerminal */
    std::optional<t::TcpTerminal> wireTerminal() const noexcept override {
        return actual.terminal ? std::optional<t::TcpTerminal>{t::TcpTerminal{}} : std::nullopt;
    }

    /** @copydoc detail::RecoveryBackend::nextWake */
    std::optional<detail::RecoveryClock::time_point> nextWake() const noexcept override {
        return {};
    }
};

/** Stable fixture declaration order preserves the backend through Core destruction and event retirement. */
struct Fixture {
    c::LocalSchema schema = r::schema();                                     ///< Exact shared schema provenance.
    std::shared_ptr<h::Recorder> recorder = std::make_shared<h::Recorder>(); ///< Actual callback state.
    std::shared_ptr<h::Recorder> lowerRecorder;  ///< Optional lower view keeping the aggregate snapshot open.
    m::MuxHandle handle;                         ///< Existing producer-safe registration, when HISTORY is enabled.
    m::MuxHandle lowerHandle;                    ///< Retains the optional lower desired range through owner retirement.
    Backend backend;                             ///< Embedded private adapter; deliberately declared before Core.
    std::unique_ptr<detail::RecoveryCore> owner; ///< Production scheduler and adopted actual source owner.

    /** Prepare and adopt real inputs without bootstrap or native I/O.
     * @param withHistory Whether to adopt a HISTORY collector/registration.
     * @param keepLowerRange Whether another view requests zero while the observed view requests two.
     */
    explicit Fixture(bool withHistory = true, bool keepLowerRange = false) {
        t::RecoveryQueues queues;

        if (withHistory) {
            auto queue = h::queue(schema);
            handle = h::open(queue, recorder);
            auto requested = h::replace(handle, schema, 2);

            if (keepLowerRange) {
                lowerRecorder = std::make_shared<h::Recorder>();
                lowerHandle = h::open(queue, lowerRecorder);
                auto lower = h::replace(lowerHandle, schema, 0);

                for (unsigned turn = 0;
                     turn < MAXIMUM_INITIAL_COMMAND_TURNS && (!requested.result() || !lower.result()); ++turn) {
                    auto credits = r::queueWork();
                    REQUIRE(queue.progress(credits));
                }

                REQUIRE(requested.result());
                REQUIRE(lower.result());
                REQUIRE(requested.result()->status == m::CommandStatus::APPLIED);
                REQUIRE(lower.result()->status == m::CommandStatus::APPLIED);
            }

            queues.history = std::move(queue);
        }

        auto input = r::settings(1234, 2, 2s);
        h::enableHistory(input);
        auto endpoint = r::endpoint(schema, input);
        auto blueprint = r::blueprint();
        auto work = r::preparation();
        auto configuration = detail::RecoveryConfiguration::prepare(endpoint, blueprint, queues, r::options(),
                                                                    d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, work);
        REQUIRE(configuration);
        const auto retirement = detail::admitRecoveryOwner(*configuration, sizeof(Fixture), work.recovery);
        REQUIRE(retirement);
        owner = std::make_unique<detail::RecoveryCore>(std::move(*configuration), backend, *retirement);
        REQUIRE(owner->adopt(std::move(blueprint), std::move(queues)));
    }

    /** Form the exact current key without inventing raw schema IDs.
     * @return Borrowed key under this fixture's schema lifetime.
     */
    c::SubscriptionKey key() const {
        return {schema.recordContext(0), u"A"};
    }

    /** Pump actual bounded Core turns until the test observation, with no fake queue state.
     * @tparam Predicate Synchronous bounded test observation.
     * @param predicate Required condition.
     */
    template <class Predicate> void until(Predicate predicate) {
        for (unsigned turn = 0; turn < 20000 && !predicate(); ++turn) {
            auto work = r::turn();
            const auto progressed = owner->poll(work);
            REQUIRE(progressed);
        }

        const auto satisfied = predicate();

        if (!satisfied) {
            const auto status = owner->status();
            std::cout << "CORE_HISTORY_TIMEOUT phase=" << static_cast<int>(status.phase)
                      << " stage=" << status.first.stage << " advancing=" << backend.advancingCalls
                      << " transfers=" << backend.eventTransfers << " callbackBatches=" << recorder->batches.size()
                      << " actualPresent=" << backend.actual.present << " actualConnected=" << backend.actual.connected
                      << " actualAuth=" << backend.actual.authenticated << " actualTerminal=" << backend.actual.terminal
                      << " next=" << backend.next.has_value() << '\n';
            auto work = r::queueWork();
            h::diagnose(owner->historyRangeStatus(handle, key(), work));
        }

        REQUIRE(satisfied);
    }

    /** Start the production scheduler and actual desired replay before inspecting a range. */
    void start() {
        REQUIRE(owner->start());
        until([&] {
            if (owner->status().phase != t::RecoveryPhase::ACTIVE || backend.next) {
                return false;
            }

            if (!backend.history) {
                return true;
            }

            auto work = r::queueWork();

            return owner->historyRangeStatus(handle, key(), work).has_value();
        });
    }

    /** Copy an actual current production query with sufficient explicit work.
     * @return Detached AS-OF result, without retaining a live owner.
     */
    m::QueueRangeStatus range() {
        auto work = r::queueWork();
        const auto status = owner->historyRangeStatus(handle, key(), work);
        REQUIRE(status);

        return *status;
    }

    /** Transfer one actual LiveSession DATA event and progress to a specified fresh range observation.
     * @tparam Predicate Actual returned range predicate, not internal state mutation.
     * @param bytes Complete binary frame to decode independently.
     * @param predicate Required delivered/source facts.
     */
    template <class Predicate> void send(std::span<const std::byte> bytes, Predicate predicate) {
        REQUIRE_FALSE(backend.next);
        backend.next.emplace(backend.decode(bytes));
        until([&] {
            auto work = r::queueWork();
            const auto status = owner->historyRangeStatus(handle, key(), work);

            return !backend.next && status && predicate(*status);
        });
    }

    /** Establish an actual covered and delivered prefix for the observed bound. */
    void complete() {
        send(h::data({{3, 30, h::SNAPSHOT_BEGIN}, {2, 20, 0}}), [](const auto &status) {
            return status.completeDeliveredRange;
        });
        const auto status = range();
        REQUIRE(status.source);
        CHECK(status.source->sourceFrontier == 2);
        CHECK(status.requestedFrom == 2);
    }
};

/** Verify every ineligible-query immutability coordinate and preserve actual failure provenance.
 * @param fixture Live stable owner with the desired eligibility state already arranged.
 * @param expected Expected top-level refusal category.
 * @param stage Optional exact static stage, absent for established owner/thread categories.
 */
void rejectUnchanged(Fixture &fixture, t::RecoveryError expected = t::RecoveryError::INVALID_STATE,
                     std::string_view stage = "recovery-history-ineligible") {
    auto work = r::queueWork();
    const auto beforeWork = h::coordinates(work);
    const auto before = fixture.owner->status();
    const auto advancing = fixture.backend.advancingCalls;
    const auto callbacks = fixture.recorder->batches.size();
    const auto rejected = fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == expected);

    if (!stage.empty()) {
        CHECK(rejected.error().stage == stage);
    }

    CHECK(h::coordinates(work) == beforeWork);
    CHECK(fixture.backend.advancingCalls == advancing);
    CHECK(fixture.recorder->batches.size() == callbacks);
    const auto after = fixture.owner->status();
    CHECK(after.phase == before.phase);
    CHECK(after.attempts == before.attempts);
    CHECK(after.first.error == before.first.error);
    CHECK(after.first.stage == before.first.stage);
    CHECK(after.first.transport.error == before.first.transport.error);
    CHECK(after.first.transport.nativeCode == before.first.transport.nativeCode);
    CHECK(after.first.queue.has_value() == before.first.queue.has_value());
    CHECK(after.secondary.error == before.secondary.error);
    CHECK(after.secondary.stage == before.secondary.stage);
}
} // namespace

TEST_CASE("recovery HISTORY query forwards actual covered RECEIVING range and exact existing query work") {
    Fixture fixture(true, true);
    rejectUnchanged(fixture);
    fixture.start();
    fixture.complete();
    REQUIRE(fixture.backend.history);
    auto directWork = r::queueWork();
    auto forwardedWork = directWork;
    const auto direct = fixture.backend.history->rangeStatus(fixture.handle, fixture.key(), directWork);
    const auto forwarded = fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), forwardedWork);
    REQUIRE(direct);
    REQUIRE(forwarded);
    h::sameStatus(*forwarded, *direct);
    CHECK(h::coordinates(forwardedWork) == h::coordinates(directWork));
    CHECK(forwarded->completeDeliveredRange);
    REQUIRE(forwarded->source);
    CHECK(forwarded->source->snapshot == root::collectors::HistorySnapshotState::RECEIVING);
    auto lowerWork = r::queueWork();
    const auto lower = fixture.owner->historyRangeStatus(fixture.lowerHandle, fixture.key(), lowerWork);
    REQUIRE(lower);
    CHECK_FALSE(lower->completeDeliveredRange);

    const auto detached = *forwarded;
    fixture.owner.reset();
    CHECK(detached.completeDeliveredRange);
    REQUIRE(detached.source);
    CHECK(detached.source->snapshot == root::collectors::HistorySnapshotState::RECEIVING);
    CHECK(detached.source->sourceFrontier == 2);
    CHECK(detached.requestedFrom == 2);
}

TEST_CASE("recovery HISTORY snapshot enumerates real applied interests without backend progress and preserves guards") {
    Fixture fixture(true, true);
    fixture.start();
    fixture.complete();
    const auto advancing = fixture.backend.advancingCalls;
    auto work = r::queueWork();
    const auto actual = fixture.owner->historyRangeSnapshot(fixture.handle, work);
    REQUIRE(actual);
    REQUIRE(actual->size() == 1);
    CHECK((*actual)[0].context.matches(fixture.schema.recordContext(0)));
    CHECK((*actual)[0].symbol == u"A");
    CHECK((*actual)[0].status.requestedFrom == 2);
    CHECK((*actual)[0].status.completeDeliveredRange);
    CHECK(fixture.backend.advancingCalls == advancing);
    fixture.backend.actual.authenticated = false;
    auto gatedWork = r::queueWork();
    const auto before = h::coordinates(gatedWork);
    const auto gated = fixture.owner->historyRangeSnapshot(fixture.handle, gatedWork);
    REQUIRE_FALSE(gated);
    CHECK(gated.error().error == t::RecoveryError::INVALID_STATE);
    CHECK(h::coordinates(gatedWork) == before);
    CHECK(fixture.backend.advancingCalls == advancing);
    CHECK((*actual)[0].status.completeDeliveredRange); // An older owning AS-OF cut does not mutate itself.
}

TEST_CASE("recovery HISTORY query exposes unknown expanded pending snipped and paused delivered states") {
    SUBCASE("unknown then expanded bound cannot reuse earlier covered prefix") {
        Fixture fixture;
        fixture.start();
        const auto unknown = fixture.range();
        CHECK_FALSE(unknown.completeDeliveredRange);
        fixture.complete();
        auto expanded = h::replace(fixture.handle, fixture.schema, 0);
        fixture.until([&] {
            if (!expanded.result()) {
                return false;
            }

            auto work = r::queueWork();
            const auto status = fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), work);

            return status && status->mappingCurrent && status->requestedFrom == 0;
        });
        const auto status = fixture.range();
        CHECK_FALSE(status.completeDeliveredRange);
        REQUIRE(status.source);
        CHECK_FALSE(status.source->completeRequestedRange);
        CHECK(status.source->knownFrontier == 2);
    }

    SUBCASE("actual TX pending and snapshot snip remain incomplete") {
        Fixture fixture;
        fixture.start();
        fixture.complete();
        fixture.send(h::data({{3, 31, h::TX_PENDING}}), [](const auto &status) {
            return status.source && status.source->sourceTransactionPending;
        });
        CHECK_FALSE(fixture.range().completeDeliveredRange);
        fixture.send(h::data({{3, 32, h::SNAPSHOT_BEGIN}, {3, 0, h::SNAPSHOT_SNIP | h::REMOVE_EVENT}}),
                     [](const auto &status) {
                         return status.source && status.source->snipFrontier == 3;
                     });
        const auto snipped = fixture.range();
        CHECK_FALSE(snipped.completeDeliveredRange);
        REQUIRE(snipped.source);
        CHECK(snipped.source->snapshot == root::collectors::HistorySnapshotState::TRUNCATED);
    }

    SUBCASE("actual paused and queued projection cannot certify delivered range") {
        Fixture fixture;
        fixture.recorder->pause = true;
        fixture.start();
        fixture.send(h::data({{3, 30, h::SNAPSHOT_BEGIN}, {2, 20, 0}}), [](const auto &status) {
            return status.paused;
        });
        CHECK_FALSE(fixture.range().completeDeliveredRange);
        fixture.recorder->pause = false;
        CHECK(fixture.handle.requestResume() == m::ControlAdmission::RECORDED);
        fixture.until([&] {
            return fixture.range().completeDeliveredRange;
        });
    }
}

TEST_CASE("recovery HISTORY query gates actual backend drift and DEFERRED authentication despite ACTIVE model") {
    Fixture fixture;
    fixture.start();
    fixture.complete();
    const auto original = fixture.backend.actual;

    for (unsigned kind = 0; kind < 6; ++kind) {
        fixture.backend.actual = original;

        switch (kind) {
        case 0:
            fixture.backend.actual.present = false;
            break;
        case 1:
            fixture.backend.actual.connected = false;
            break;
        case 2:
            fixture.backend.actual.authenticated = false;
            break;
        case 3:
            fixture.backend.actual.terminal = true;
            break;
        case 4:
            ++fixture.backend.actual.generation;
            break;
        default:
            fixture.backend.actual.authentication = p::LiveAuthentication::ACKNOWLEDGED;
            break;
        }

        REQUIRE(fixture.owner->status().phase == t::RecoveryPhase::ACTIVE);
        const auto reads = fixture.backend.factsReads;
        rejectUnchanged(fixture);
        CHECK(fixture.backend.factsReads == reads + 1);
    }

    fixture.backend.actual = original;
    CHECK(fixture.range().completeDeliveredRange);
    fixture.backend.deferAuthentication = true;
    fixture.until([&] {
        return !fixture.backend.deferAuthentication;
    });
    REQUIRE(fixture.owner->status().phase == t::RecoveryPhase::ACTIVE);
    CHECK_FALSE(fixture.backend.actual.authenticated);
    CHECK(fixture.backend.actual.authentication == p::LiveAuthentication::REQUIRED);
    rejectUnchanged(fixture);
}

TEST_CASE("recovery HISTORY query gates held complete DATA and non-DATA owners before existing queue admission") {
    for (const bool heartbeat : {false, true}) {
        Fixture fixture;
        fixture.start();
        fixture.complete();
        const auto transfers = fixture.backend.eventTransfers;
        const auto frame = heartbeat ? r::frame(0, {}) : h::data({{3, 31, 0}});
        fixture.backend.next.emplace(fixture.backend.decode(frame));

        for (unsigned turn = 0; turn < 1000 && fixture.backend.eventTransfers == transfers; ++turn) {
            auto work = r::turn();
            work.session = {};
            work.queues[2] = {};
            REQUIRE(fixture.owner->poll(work));
        }

        REQUIRE(fixture.backend.eventTransfers == transfers + 1);
        REQUIRE(fixture.owner->status().phase == t::RecoveryPhase::ACTIVE);
        rejectUnchanged(fixture);
        auto directWork = r::queueWork();
        const auto oldQueue = fixture.backend.history->rangeStatus(fixture.handle, fixture.key(), directWork);
        REQUIRE(oldQueue);
        CHECK(oldQueue->completeDeliveredRange);

        fixture.until([&] {
            auto work = r::queueWork();
            const auto status = fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), work);

            return status && status->completeDeliveredRange;
        });
        CHECK(fixture.range().completeDeliveredRange);
    }
}

TEST_CASE("recovery HISTORY query rejects retirement before source reset and immediate cutoff without poll") {
    Fixture fixture;
    fixture.start();
    fixture.complete();
    const auto old = fixture.range();
    fixture.backend.failureValue = {
        {t::RecoveryError::TRANSPORT_FAILURE, "controlled-peer-end"}, detail::RecoveryCause::TRANSPORT, true};
    fixture.backend.actual.connected = false;
    fixture.backend.actual.terminal = true;
    fixture.until([&] {
        return fixture.owner->status().phase == t::RecoveryPhase::RETIRING;
    });
    rejectUnchanged(fixture);
    auto directWork = r::queueWork();
    const auto notReset = fixture.backend.history->rangeStatus(fixture.handle, fixture.key(), directWork);
    REQUIRE(notReset);
    CHECK(notReset->epoch.matches(old.epoch));
    CHECK(notReset->completeDeliveredRange);
    fixture.until([&] {
        return fixture.owner->status().phase == t::RecoveryPhase::BACKOFF;
    });
    rejectUnchanged(fixture);
    REQUIRE(fixture.owner->requestStop());
    rejectUnchanged(fixture);

    Fixture stopped;
    stopped.start();
    stopped.complete();
    const auto attempts = stopped.owner->status().attempts;
    REQUIRE(stopped.owner->requestStop());
    CHECK(stopped.owner->status().stopped);
    CHECK(stopped.owner->status().attempts == attempts);
    CHECK_FALSE(stopped.owner->status().terminal);
    rejectUnchanged(stopped);
}

TEST_CASE("recovery HISTORY query owner rejection precedes backend reads and callback reentry touches no work") {
    Fixture fixture;
    fixture.start();
    fixture.complete();
    const auto reads = fixture.backend.factsReads;
    auto wrongThreadWork = r::queueWork();
    const auto before = h::coordinates(wrongThreadWork);
    std::optional<std::expected<m::QueueRangeStatus, t::RecoveryDiagnostic>> wrongThread;
    std::thread other([&] {
        wrongThread.emplace(fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), wrongThreadWork));
    });
    other.join();
    REQUIRE(wrongThread);
    REQUIRE_FALSE(*wrongThread);
    CHECK(wrongThread->error().error == t::RecoveryError::WRONG_THREAD);
    CHECK(fixture.backend.factsReads == reads);
    CHECK(h::coordinates(wrongThreadWork) == before);

    bool callbackObserved = false;
    fixture.recorder->action = [&] {
        const auto beforeReads = fixture.backend.factsReads;
        rejectUnchanged(fixture, t::RecoveryError::REENTRANT, "");
        CHECK(fixture.backend.factsReads == beforeReads);
        callbackObserved = true;
    };
    fixture.send(h::data({{3, 33, 0}}), [&](const auto &) {
        return callbackObserved;
    });

    Fixture absent(false);
    const auto absentReads = absent.backend.factsReads;
    rejectUnchanged(absent, t::RecoveryError::INVALID_STATE, "");
    CHECK(absent.backend.factsReads == absentReads);
}

TEST_CASE("recovery HISTORY eligible query preserves direct queue errors foreign keys and one-short work") {
    Fixture fixture;
    fixture.start();
    fixture.complete();
    auto zero = m::QueueWork{};
    const auto noWork = fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), zero);
    REQUIRE_FALSE(noWork);
    CHECK(noWork.error().error == t::RecoveryError::WORK_LIMIT);
    REQUIRE(noWork.error().queue);
    CHECK(noWork.error().queue->error == m::QueueError::WORK_LIMIT);

    auto full = r::queueWork();
    const auto before = full;
    REQUIRE(fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), full));
    const auto spent = before.textUnits - full.textUnits;
    REQUIRE(spent > 0);
    auto exact = r::queueWork();
    exact.textUnits = spent;
    REQUIRE(fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), exact));
    CHECK(exact.textUnits == 0);
    auto shortWork = r::queueWork();
    shortWork.textUnits = spent - 1;
    const auto shortResult = fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), shortWork);
    REQUIRE_FALSE(shortResult);
    REQUIRE(shortResult.error().queue);
    CHECK(shortResult.error().queue->error == m::QueueError::WORK_LIMIT);

    const auto foreignSchema = r::schema();
    const c::SubscriptionKey foreignKey{foreignSchema.recordContext(0), u"A"};
    const m::MuxHandle invalid;
    auto foreignQueue = h::queue(fixture.schema);
    auto foreignRecorder = std::make_shared<h::Recorder>();
    auto foreignHandle = h::open(foreignQueue, foreignRecorder);

    for (unsigned kind = 0; kind < 3; ++kind) {
        auto directWork = r::queueWork();
        auto forwardedWork = directWork;
        const auto &handle = kind == 0 ? fixture.handle : kind == 1 ? invalid : foreignHandle;
        const auto key = kind == 0 ? foreignKey : fixture.key();
        const auto direct = fixture.backend.history->rangeStatus(handle, key, directWork);
        const auto forwarded = fixture.owner->historyRangeStatus(handle, key, forwardedWork);
        REQUIRE_FALSE(direct);
        REQUIRE_FALSE(forwarded);
        REQUIRE(forwarded.error().queue);
        CHECK(forwarded.error().queue->error == direct.error().error);
        CHECK(h::coordinates(forwardedWork) == h::coordinates(directWork));
    }

    CHECK(fixture.range().completeDeliveredRange);
    CHECK(fixture.handle.requestStop(m::StopMode::ABORT) == m::ControlAdmission::RECORDED);
    fixture.until([&] {
        return fixture.handle.terminal().has_value() && fixture.owner->status().phase == t::RecoveryPhase::ACTIVE;
    });
    auto staleWork = r::queueWork();
    const auto stale = fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), staleWork);
    REQUIRE_FALSE(stale);
    REQUIRE(stale.error().queue);
    CHECK(stale.error().queue->error == m::QueueError::INVALID_CLIENT);
}

TEST_CASE("HISTORY bound upsert coalesces blocked replay without publishing an unadmitted bank") {
    Fixture fixture;
    fixture.start();
    fixture.until([&] {
        return fixture.backend.admittedSubscriptions.size() == 1;
    });
    CHECK(fixture.backend.admittedSubscriptions[0].first == p::SubscriptionMessage::HISTORY_ADD);
    CHECK(fixture.backend.admittedSubscriptions[0].second == 2);
    fixture.backend.deferSubscriptions = true;
    auto narrow = h::replace(fixture.handle, fixture.schema, 3);
    fixture.until([&] {
        return narrow.result() && fixture.backend.lastSubscription && fixture.backend.lastSubscription->second == 3;
    });
    CHECK(fixture.backend.admittedSubscriptions.size() == 1);
    CHECK(fixture.backend.lastSubscription->first == p::SubscriptionMessage::HISTORY_ADD);
    auto expand = h::replace(fixture.handle, fixture.schema, 1);
    fixture.until([&] {
        return expand.result() && fixture.backend.lastSubscription && fixture.backend.lastSubscription->second == 1;
    });
    CHECK(fixture.backend.admittedSubscriptions.size() == 1);
    fixture.backend.deferSubscriptions = false;
    fixture.until([&] {
        return fixture.backend.admittedSubscriptions.size() == 2;
    });
    CHECK(fixture.backend.admittedSubscriptions[1].first == p::SubscriptionMessage::HISTORY_ADD);
    CHECK(fixture.backend.admittedSubscriptions[1].second == 1);
    fixture.until([&] {
        auto work = r::queueWork();
        const auto range = fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), work);

        return range && range->requestedFrom == 1;
    });
    CHECK(fixture.range().requestedFrom == 1);
}

TEST_CASE("HISTORY range mutation and removal preserve an overlapping lower view and its physical demand") {
    Fixture fixture(true, true);
    fixture.start();
    fixture.send(
        h::data({{3, 30, h::SNAPSHOT_BEGIN}, {2, 20, 0}, {1, 10, 0}, {0, 0, h::SNAPSHOT_END | h::REMOVE_EVENT}}),
        [](const auto &range) {
            return range.completeDeliveredRange;
        });
    const auto records = [](const auto &recorder) {
        std::size_t count = 0;

        for (const auto &batch : recorder->batches)
            count += batch.records().size();

        return count;
    };
    fixture.until([&] {
        return records(fixture.lowerRecorder) == 4;
    });
    REQUIRE(fixture.backend.admittedSubscriptions.size() == 1);
    CHECK(fixture.backend.admittedSubscriptions[0].second == 0);
    const auto lowerCount = records(fixture.lowerRecorder);
    const auto oldBatches = fixture.recorder->batches.size();
    auto shrink = h::replace(fixture.handle, fixture.schema, 3);
    fixture.until([&] {
        auto work = r::queueWork();
        const auto range = fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), work);

        return shrink.result() && range && range->requestedFrom == 3 && range->completeDeliveredRange &&
               fixture.recorder->batches.size() > oldBatches;
    });

    for (std::size_t index = oldBatches; index < fixture.recorder->batches.size(); ++index)
        for (const auto &record : fixture.recorder->batches[index].records())
            CHECK(record.rawTime >= 3);

    CHECK(records(fixture.lowerRecorder) == lowerCount);
    CHECK(fixture.backend.admittedSubscriptions.size() == 1);
    auto expand = h::replace(fixture.handle, fixture.schema, 1);
    fixture.until([&] {
        auto work = r::queueWork();
        const auto range = fixture.owner->historyRangeStatus(fixture.handle, fixture.key(), work);

        return expand.result() && range && range->requestedFrom == 1 && range->completeDeliveredRange;
    });
    CHECK(records(fixture.lowerRecorder) == lowerCount);
    CHECK(fixture.backend.admittedSubscriptions.size() == 1);
    m::QueueSubmitWork work{10000, 1000000, 1000000, 1000000};
    auto removed = fixture.handle.clear(work);
    REQUIRE(removed);
    fixture.until([&] {
        auto queryWork = r::queueWork();
        const auto ranges = fixture.owner->historyRangeSnapshot(fixture.handle, queryWork);

        return removed->result() && ranges && ranges->empty();
    });
    CHECK(fixture.backend.admittedSubscriptions.size() == 1);
    auto lowerWork = r::queueWork();
    const auto lower = fixture.owner->historyRangeStatus(fixture.lowerHandle, fixture.key(), lowerWork);
    REQUIRE(lower);
    CHECK(lower->requestedFrom == 0);
    CHECK(lower->completeDeliveredRange);
}
