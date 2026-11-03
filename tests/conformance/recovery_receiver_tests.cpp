// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: adopted recovery receiver registration; controlled backend, no SDK/native calls.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/quote.hpp"
#include "qd/transport/tls_recovery.hpp"

#include "recovery_core.hpp"
#include "recovery_history_status_support.hpp"
#include <algorithm>

#include <functional>
#include <memory>
#include <thread>
#include <type_traits>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace d = root::client;
namespace p = root::protocol;
namespace m = root::mux;
namespace t = root::transport;
namespace q = root::events;
namespace detail = t::detail;
namespace r = root::test::recovery;
namespace h = root::test::history_status;
using namespace std::chrono_literals;

using OpenResult = std::expected<m::MuxHandle, t::RecoveryReceiverFailure>;
using OpenMember = OpenResult (t::TcpRecovery::*)(c::SubscriptionFamily,
                                                  std::span<const d::PreparedSubscriptionBinding>,
                                                  const std::shared_ptr<d::SubscriptionConsumer> &, d::ReceiverLimits,
                                                  m::ClientLimits, d::ReceiverWork &, m::QueueWork &);
using TlsOpenMember = OpenResult (t::TlsRecovery::*)(c::SubscriptionFamily,
                                                     std::span<const d::PreparedSubscriptionBinding>,
                                                     const std::shared_ptr<d::SubscriptionConsumer> &,
                                                     d::ReceiverLimits, m::ClientLimits, d::ReceiverWork &,
                                                     m::QueueWork &);
static_assert(std::is_same_v<decltype(&t::TcpRecovery::openSubscriptionReceiver), OpenMember>);
static_assert(std::is_same_v<decltype(&t::TlsRecovery::openSubscriptionReceiver), TlsOpenMember>);

/** Actual caller-owned projection/preparation allowance, independently supplied to each attempt. */
constexpr q::MappingLimits MAPPING_LIMITS{64, 128, 32, 64, 64, 128, 8192, 100000};

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
    std::expected<void, t::RecoveryDiagnostic>
    subscribe(p::SubscriptionMessage, std::span<const c::SubscriptionRequest>, p::LiveWork &) override {
        ++advancingCalls;

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

/** Detached callback capture with an optional synchronous reentry attempt. */
struct Consumer final : d::SubscriptionConsumer {
    std::vector<m::OwnedMuxBatch> batches; ///< Generic immutable values only, never a receiver/queue owner.
    std::function<void()> action;          ///< Explicit test callback, with no owning cycle.

    /** Retain real values and execute the selected reentry check.
     * @param batch Actual callback-only receiver service.
     * @return Existing whole-batch acknowledgement.
     */
    m::BatchAction onBatch(const d::SubscriptionBatch &batch) override {
        batches.push_back(batch.retain());

        if (action) {
            action();
        }

        return m::BatchAction::CONTINUE;
    }
};

/** Complete finite registration reservations.
 * @return Existing queue resources, independent of receiver-plan limits.
 */
m::ClientLimits clientLimits() {
    return {32, 512 * 1024, 8, 128 * 1024, 4, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096};
}

/** Create one actual family owner with identical finite delivery capacities.
 * @param local Exact immutable provenance.
 * @param family Supported family, dispatched independently from recovery slot ordering.
 * @return Pristine unbound source owner.
 */
m::MuxQueue queue(const c::LocalSchema &local, c::SubscriptionFamily family) {
    const m::MuxLimits interests{16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024};
    const c::SubscriptionLimits desired{16, 32, 128, 4096, 64, 8192, 64};
    const m::QueueLimits delivery{
        4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024};
    auto work = r::queueWork();
    auto created =
        family == c::SubscriptionFamily::TICKER
            ? m::MuxQueue::createTicker(local, interests, {4, 32, 64, 4 * 1024 * 1024, 16 * 1024 * 1024, desired},
                                        {false, 100}, delivery, work)
        : family == c::SubscriptionFamily::STREAM
            ? m::MuxQueue::createStream(local, interests, desired, {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024}, delivery,
                                        work)
            : m::MuxQueue::createHistory(
                  local, interests,
                  {4, 16, 128, 8, 4 * 1024 * 1024, 4, 16, 8, 4 * 1024 * 1024, 32 * 1024 * 1024, desired},
                  {false, 100, 10000}, delivery, work);
    REQUIRE(created);

    return std::move(*created);
}

/** Own backend before Core, preserving all decoder/retirement lifetimes. */
struct Fixture {
    c::LocalSchema schema;                       ///< Exact source provenance, never reconstructed by forwarding.
    Backend backend;                             ///< Pure controlled adapter with actual LiveSession events.
    std::unique_ptr<detail::RecoveryCore> owner; ///< Adopted actual queue/scheduler owner.

    /** Adopt explicitly selected queues without native activation.
     * @param local Exact shared schema origin.
     * @param mask Recovery slot bits TICKER1, STREAM2, HISTORY4.
     */
    explicit Fixture(c::LocalSchema local = r::schema(), unsigned mask = 7) : schema(std::move(local)) {
        t::RecoveryQueues queues;

        if ((mask & 1) != 0) {
            queues.ticker.emplace(queue(schema, c::SubscriptionFamily::TICKER));
        }

        if ((mask & 2) != 0) {
            queues.stream.emplace(queue(schema, c::SubscriptionFamily::STREAM));
        }

        if ((mask & 4) != 0) {
            queues.history.emplace(queue(schema, c::SubscriptionFamily::HISTORY));
        }

        auto input = r::settings(1234, 2, 2s);
        h::enableHistory(input);
        auto endpoint = r::endpoint(schema, input);
        std::vector<p::OutgoingRecord> blueprint;

        for (const auto &record : schema.records()) {
            p::OutgoingRecord declaration{71 + record.id, record.id, record.name, {}};

            for (std::size_t index = 0; index < record.fields.size(); ++index) {
                const auto lookup = std::ranges::find_if(record.lookups, [index](const auto &entry) {
                    return entry.fieldId == index;
                });
                REQUIRE(lookup != record.lookups.end());
                declaration.fields.push_back(
                    {lookup->name, record.fields[index].serialTypeId, static_cast<std::int32_t>(index)});
            }

            blueprint.push_back(std::move(declaration));
        }

        auto work = r::preparation();
        auto prepared = detail::RecoveryConfiguration::prepare(endpoint, blueprint, queues, r::options(),
                                                               d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, work);
        REQUIRE(prepared);
        const auto retirement = detail::admitRecoveryOwner(*prepared, sizeof(Fixture), work.recovery);
        REQUIRE(retirement);
        owner = std::make_unique<detail::RecoveryCore>(std::move(*prepared), backend, *retirement);
        REQUIRE(owner->adopt(std::move(blueprint), std::move(queues)));
    }

    /** Open only a generic receiver with explicit local work.
     * @param family Requested adopted queue.
     * @param consumer Actual application callback.
     * @return Existing empty logical handle or original typed failure.
     */
    OpenResult open(c::SubscriptionFamily family, const std::shared_ptr<Consumer> &consumer) {
        d::ReceiverWork receiverWork{100000, 1000000};
        auto queueWork = r::queueWork();

        return owner->openSubscriptionReceiver(family, {}, consumer, {}, clientLimits(), receiverWork, queueWork);
    }

    /** Fund finite turns until the actual production predicate is true.
     * @tparam Predicate Detached synchronous observation.
     * @param predicate Required actual condition.
     */
    template <class Predicate> void until(Predicate predicate) {
        for (unsigned index = 0; index < 20000 && !predicate(); ++index) {
            auto work = r::turn();
            REQUIRE(owner->poll(work));
        }

        INFO("phase=", static_cast<int>(owner->status().phase), " stage=", owner->status().first.stage);
        REQUIRE(predicate());
    }

    /** Start and observe the actual ACTIVE model after the ready event retires. */
    void start() {
        REQUIRE(owner->start());
        until([&] {
            return owner->status().phase == t::RecoveryPhase::ACTIVE && !backend.next;
        });
    }
};

/** Check pre-factory rejection and every unchanged work coordinate.
 * @param fixture Actual owner in the tested state.
 * @param family Requested family.
 * @param expected Expected owner-admission category.
 */
void rejected(Fixture &fixture, c::SubscriptionFamily family, t::RecoveryError expected) {
    auto consumer = std::make_shared<Consumer>();
    d::ReceiverWork receiverWork{77, 88};
    auto queueWork = r::queueWork();
    const auto beforeQueue = h::coordinates(queueWork);
    const auto calls = fixture.backend.advancingCalls;
    const auto facts = fixture.backend.factsReads;
    auto result =
        fixture.owner->openSubscriptionReceiver(family, {}, consumer, {}, clientLimits(), receiverWork, queueWork);
    REQUIRE_FALSE(result);
    REQUIRE(std::holds_alternative<t::RecoveryDiagnostic>(result.error()));
    CHECK(std::get<t::RecoveryDiagnostic>(result.error()).error == expected);
    CHECK(receiverWork.visits == 77);
    CHECK(receiverWork.preparationWork == 88);
    CHECK(h::coordinates(queueWork) == beforeQueue);
    CHECK(fixture.backend.advancingCalls == calls);
    CHECK(fixture.backend.factsReads == facts);
    CHECK(consumer->batches.empty());
}
} // namespace

TEST_CASE("recovery receiver opens every adopted family before start without backend progress") {
    Fixture fixture;
    auto consumer = std::make_shared<Consumer>();
    std::vector<m::MuxHandle> handles;

    for (const auto family :
         {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        auto opened = fixture.open(family, consumer);
        REQUIRE(opened);
        handles.push_back(std::move(*opened));
    }

    CHECK(fixture.backend.advancingCalls == 0);
    CHECK(fixture.backend.factsReads == 0);
    CHECK(fixture.owner->status().attempts == 0);
    CHECK(consumer->batches.empty());
}

TEST_CASE("recovery receiver rejects missing invalid closed and wrong-thread owners before work") {
    Fixture fixture(r::schema(), 2);
    rejected(fixture, c::SubscriptionFamily::TICKER, t::RecoveryError::INVALID_STATE);
    rejected(fixture, static_cast<c::SubscriptionFamily>(-1), t::RecoveryError::INVALID_CONFIGURATION);
    std::thread wrong([&] {
        rejected(fixture, c::SubscriptionFamily::STREAM, t::RecoveryError::WRONG_THREAD);
    });
    wrong.join();
    REQUIRE(fixture.owner->requestDrain(detail::RecoveryClock::now() + 1s));
    rejected(fixture, c::SubscriptionFamily::STREAM, t::RecoveryError::INVALID_STATE);
    fixture.until([&] {
        return fixture.owner->status().terminal.has_value();
    });
    rejected(fixture, c::SubscriptionFamily::STREAM, t::RecoveryError::INVALID_STATE);
}

TEST_CASE("recovery receiver preserves exact receiver mapping and queue refusal alternatives") {
    Fixture fixture;
    auto consumer = std::make_shared<Consumer>();
    auto queueWork = r::queueWork();
    d::ReceiverWork zero{};
    auto noReceiverWork = fixture.owner->openSubscriptionReceiver(c::SubscriptionFamily::STREAM, {}, consumer, {},
                                                                  clientLimits(), zero, queueWork);
    REQUIRE_FALSE(noReceiverWork);
    const auto &receiver = std::get<d::ReceiverFailure>(noReceiverWork.error());
    CHECK(std::get<d::ReceiverError>(receiver.cause) == d::ReceiverError::WORK_LIMIT);
    CHECK_FALSE(receiver.bindingIndex);

    d::ReceiverWork work{100000, 1000000};
    m::QueueWork noQueueWork{};
    auto noQueue = fixture.owner->openSubscriptionReceiver(c::SubscriptionFamily::STREAM, {}, consumer, {},
                                                           clientLimits(), work, noQueueWork);
    REQUIRE_FALSE(noQueue);
    CHECK(std::get<m::QueueFailure>(std::get<d::ReceiverFailure>(noQueue.error()).cause).error ==
          m::QueueError::WORK_LIMIT);

    const std::array routes{
        d::PreparedSubscriptionBinding{0, d::PreparedEventKind::QUOTE, MAPPING_LIMITS, {16, 128, 256}}};
    work = {100000, 1000000};
    queueWork = r::queueWork();
    auto missingQuote = fixture.owner->openSubscriptionReceiver(c::SubscriptionFamily::STREAM, routes, consumer,
                                                                {1, 64, 64}, clientLimits(), work, queueWork);
    REQUIRE_FALSE(missingQuote);
    const auto &mapping = std::get<d::ReceiverFailure>(missingQuote.error());
    CHECK(std::get<q::MappingError>(mapping.cause) == q::MappingError::MISSING_REQUIRED);
    CHECK(mapping.bindingIndex == 0);
    CHECK(work.visits < 100000);
    CHECK(work.preparationWork < 1000000);
    CHECK(consumer->batches.empty());
}

TEST_CASE("recovery receiver opens independent subscriptions after ACTIVE and rejects callback reentry") {
    Fixture fixture;
    fixture.start();
    auto first = std::make_shared<Consumer>();
    auto second = std::make_shared<Consumer>();
    const auto calls = fixture.backend.advancingCalls;
    const auto facts = fixture.backend.factsReads;
    auto one = fixture.open(c::SubscriptionFamily::STREAM, first);
    auto two = fixture.open(c::SubscriptionFamily::STREAM, second);
    REQUIRE(one);
    REQUIRE(two);
    CHECK(fixture.backend.advancingCalls == calls);
    CHECK(fixture.backend.factsReads == facts);
    const std::array<c::SubscriptionRequest, 1> a{{{{fixture.schema.recordContext(0), u"A"}, {}}}};
    const std::array<c::SubscriptionRequest, 1> b{{{{fixture.schema.recordContext(0), u"B"}, {}}}};
    m::QueueSubmitWork submit{10000, 1000000, 1000000, 1000000};
    auto ticketA = one->replace(a, submit);
    auto ticketB = two->replace(b, submit);
    REQUIRE(ticketA);
    REQUIRE(ticketB);
    fixture.until([&] {
        return ticketA->result() && ticketB->result() && fixture.owner->status().phase == t::RecoveryPhase::ACTIVE;
    });
    bool reentered = false;
    first->action = [&] {
        rejected(fixture, c::SubscriptionFamily::STREAM, t::RecoveryError::REENTRANT);
        reentered = true;
    };
    fixture.backend.next.emplace(fixture.backend.decode(r::data(41, u"A", 17)));
    fixture.until([&] {
        return first->batches.size() == 1;
    });
    CHECK(reentered);
    CHECK(second->batches.empty());
    fixture.backend.next.emplace(fixture.backend.decode(r::data(41, u"B", 19)));
    fixture.until([&] {
        return second->batches.size() == 1;
    });
    CHECK(first->batches.size() == 1);
    CHECK(first->batches[0].records()[0].symbol == u"A");
    CHECK(second->batches[0].records()[0].symbol == u"B");
    REQUIRE(fixture.owner->requestStop());
    rejected(fixture, c::SubscriptionFamily::STREAM, t::RecoveryError::INVALID_STATE);
}

TEST_CASE("recovery receiver remains available through temporary retirement reset and backoff") {
    Fixture fixture;
    fixture.start();
    fixture.backend.failureValue = {
        {t::RecoveryError::TRANSPORT_FAILURE, "controlled-peer-end"}, detail::RecoveryCause::TRANSPORT, true};
    fixture.backend.actual.connected = false;
    fixture.backend.actual.terminal = true;
    fixture.until([&] {
        return fixture.owner->status().phase == t::RecoveryPhase::RETIRING;
    });
    auto consumer = std::make_shared<Consumer>();
    auto retiring = fixture.open(c::SubscriptionFamily::STREAM, consumer);
    REQUIRE(retiring);
    fixture.until([&] {
        return fixture.owner->status().phase == t::RecoveryPhase::RESETTING;
    });
    auto resetting = fixture.open(c::SubscriptionFamily::STREAM, consumer);
    REQUIRE(resetting);
    fixture.until([&] {
        return fixture.owner->status().phase == t::RecoveryPhase::BACKOFF;
    });
    const auto calls = fixture.backend.advancingCalls;
    auto backoff = fixture.open(c::SubscriptionFamily::STREAM, consumer);
    REQUIRE(backoff);
    CHECK(fixture.backend.advancingCalls == calls);
    REQUIRE(fixture.owner->requestStop());
}

TEST_CASE("recovery receiver prepares Quote against the adopted original immutable schema") {
    constexpr c::SchemaLimits bounds{16, 128, 256, 128, 16384};
    auto defaults = q::defaultQuoteSchema(bounds);
    REQUIRE(defaults);
    auto local = c::LocalSchema::create(defaults->records(), bounds, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    Fixture fixture(std::move(*local), 1);
    auto consumer = std::make_shared<Consumer>();
    const std::array routes{
        d::PreparedSubscriptionBinding{0, d::PreparedEventKind::QUOTE, MAPPING_LIMITS, {16, 128, 256}}};
    d::ReceiverWork receiverWork{100000, 1000000};
    auto queueWork = r::queueWork();
    auto opened = fixture.owner->openSubscriptionReceiver(c::SubscriptionFamily::TICKER, routes, consumer, {1, 64, 64},
                                                          clientLimits(), receiverWork, queueWork);
    REQUIRE(opened);
    const std::array<c::SubscriptionRequest, 1> requested{{{{fixture.schema.recordContext(0), u"IBM"}, {}}}};
    m::QueueSubmitWork submit{10000, 1000000, 1000000, 1000000};
    auto ticket = opened->replace(requested, submit);
    REQUIRE(ticket);
    fixture.until([&] {
        return ticket->result().has_value();
    });
    REQUIRE(ticket->result());
    CHECK(ticket->result()->status == m::CommandStatus::APPLIED);
    CHECK(fixture.backend.advancingCalls == 0);
    CHECK(consumer->batches.empty());
}

TEST_CASE("public TCP receiver forwarding keeps adopted queues and rejects a moved-from handle") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto local = r::schema();
    auto endpoint = r::endpoint(local, r::settings(1234, 1));
    auto blueprint = r::blueprint();
    t::RecoveryQueues queues;
    queues.stream.emplace(queue(local, c::SubscriptionFamily::STREAM));
    auto preparation = r::preparation();
    auto created = t::TcpRecovery::create(*bootstrap, endpoint, std::move(blueprint), std::move(queues), r::options(),
                                          preparation);
    REQUIRE(created);
    auto owner = std::move(*created);
    auto consumer = std::make_shared<Consumer>();
    d::ReceiverWork receiverWork{77, 88};
    auto queueWork = r::queueWork();
    const auto beforeQueue = h::coordinates(queueWork);
    auto moved = created->openSubscriptionReceiver(c::SubscriptionFamily::STREAM, {}, consumer, {}, clientLimits(),
                                                   receiverWork, queueWork);
    REQUIRE_FALSE(moved);
    CHECK(std::get<t::RecoveryDiagnostic>(moved.error()).error == t::RecoveryError::INVALID_STATE);
    CHECK(receiverWork.visits == 77);
    CHECK(receiverWork.preparationWork == 88);
    CHECK(h::coordinates(queueWork) == beforeQueue);
    receiverWork = {100000, 1000000};
    auto actual = owner.openSubscriptionReceiver(c::SubscriptionFamily::STREAM, {}, consumer, {}, clientLimits(),
                                                 receiverWork, queueWork);
    REQUIRE(actual);
    CHECK(owner.status().attempts == 0);
    CHECK(consumer->batches.empty());
    REQUIRE(owner.requestStop());
}
