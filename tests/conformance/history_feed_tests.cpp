// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal finite runner: public eligibility and the production detached HISTORY observation boundary.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../core/delivery_statistics.hpp"
#include "history_observation.hpp"
#include "qd/api/endpoint_configuration.hpp"
#include "qd/api/feed.hpp"
#include "qd/encoding/symbol_reader.hpp"
#include "qd/protocol/frame_reader.hpp"
#include "receiving_profile.hpp"
#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"
#include "subscription_desired.hpp"
#include "subscription_observer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace a = q::api;
namespace d = a::detail;

/** Real detached facade state with a known completed publication, without a simulated business mapper. */
struct ObservationFixture {
    std::shared_ptr<d::EndpointPort> endpoint = std::make_shared<d::EndpointPort>();
    std::shared_ptr<d::SubscriptionPort> port = std::make_shared<d::SubscriptionPort>();

    ObservationFixture() {
        endpoint->options.contract = a::DeliveryContract::HISTORY;
        endpoint->connectionIntent = true;
        endpoint->connectionRevision = endpoint->activeDeliveryRevision = 3;
        endpoint->clearRevision = endpoint->clearedRevision = 2;
        port->endpoint = endpoint;
        port->statistics = std::make_shared<q::core::detail::DeliveryStatisticsState>();
        port->statistics->value.ownerIdentity = std::make_shared<const std::byte>();
        port->changes = std::make_shared<d::SubscriptionChangeDomain>();
        port->revision = 7;
        port->state = a::SubscriptionState::ACTIVE;
        port->history.ownerIdentity = port->statistics->value.ownerIdentity;
        port->history.state = a::HistoryObservationState::CURRENT;
        port->history.publicationSerial = 4;
        port->history.logicalRevision = 7;
        port->history.mappingLogicalRevision = 7;
        port->history.connectionRevision = 3;
        port->history.clearRevision = port->history.clearedRevision = 2;
        q::mux::QueueRangeStatus status;
        status.completeDeliveredRange = true;
        port->history.ranges.push_back({{}, u"IBM", q::core::SubscriptionSelectorKind::EXACT, status});
    }
};
} // namespace

TEST_CASE("live fromTime API refuses unverified native types and ordinary mode atomically") {
    a::RuntimeOptions limits;
    limits.maximumSubscriptionsPerEndpoint = 1;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint();
    auto feed = endpoint->getFeed();
    CHECK_THROWS_AS(static_cast<void>(feed->createTimeSeriesSubscription<q::events::Quote>()), a::Error);
    auto ordinary = feed->createSubscription<q::events::Quote>();
    ordinary->addSymbols("IBM");
    const auto before = ordinary->getSnapshot();
    CHECK_FALSE(before.fromTime);
    CHECK_THROWS_AS(static_cast<void>(ordinary->getFromTime()), a::Error);
    CHECK_THROWS_AS(ordinary->setFromTime(2000), a::Error);
    CHECK(ordinary->getSnapshot().revision == before.revision);
    CHECK(ordinary->getSnapshot().symbols == before.symbols);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    auto historyOptions = a::EndpointOptions{};
    historyOptions.contract = a::DeliveryContract::HISTORY;
    auto history = runtime.createEndpoint(historyOptions);

    for (auto type :
         {a::EventType::QUOTE, a::EventType::PROFILE, a::EventType::TRADE, a::EventType::TRADE_ETH,
          a::EventType::SUMMARY, a::EventType::TEXT_MESSAGE, a::EventType::TEXT_CONFIGURATION, a::EventType::MESSAGE,
          a::EventType::CONFIGURATION, a::EventType::ORDER, a::EventType::MARKET_MAKER}) {
        try {
            static_cast<void>(history->getFeed()->createTimeSeriesSubscription({type}));
            FAIL("an enum or raw indexed layout is not a verified time-series mapper");
        } catch (const a::Error &error) {
            CHECK(error.diagnostic().code == a::ErrorCode::UNSUPPORTED_PROFILE);
            CHECK(error.diagnostic().stage == "subscription-time-series-event-type");
        }
    }

    const auto refuseIndexed = [&]<class Event> {
        try {
            static_cast<void>(history->getFeed()->createTimeSeriesSubscription<Event>());
            FAIL("a native indexed event must not acquire a timestamp range");
        } catch (const a::Error &error) {
            CHECK(error.diagnostic().code == a::ErrorCode::UNSUPPORTED_PROFILE);
            CHECK(error.diagnostic().stage == "subscription-time-series-event-type");
        }
    };
    refuseIndexed.operator()<q::events::Order>();
    refuseIndexed.operator()<q::events::MarketMaker>();

    static_cast<void>(history->closeAndAwaitTermination());
    runtime.closeAndAwaitTermination();
}

TEST_CASE("live fromTime observer candidates contain the new range and refusal preserves the whole cut") {
    ObservationFixture f;
    f.port->timeSeries = true;
    f.port->fromTime = 2000;
    f.port->maximumSymbols = 2;
    f.port->symbols = {a::SubscriptionSelector::timeSeries(u"IBM", 2000)};
    f.port->changes->maximumWork = 8;
    // Preparation needs a nonempty observer registry, but this test never invokes a synthetic callback.
    f.port->observers.listeners = std::make_shared<const d::ListenerSnapshot>(1);
    f.port->observers.phases.resize(1);
    {
        d::SubscriptionObserverAdmission admission(f.port->changes);
        std::lock_guard lock(f.port->mutex);
        REQUIRE(d::replaceSubscriptionFromTimeLocked(*f.port, 3000));
        CHECK(f.port->revision == 8);
        CHECK(f.port->fromTime == 3000);
        REQUIRE(f.port->observers.latest);
        CHECK(f.port->observers.latest->snapshot.fromTime == 3000);
        CHECK(f.port->observers.latest->snapshot.revision == 8);
        CHECK(f.port->observers.latest->snapshot.symbols[0].fromTime() == 3000);
        CHECK(f.port->observers.latest->added[0].fromTime() == 3000);
        CHECK(f.port->observers.latest->removed.empty()); // Range is not identity.
        const auto saved = f.port->observers.latest;
        CHECK_FALSE(d::replaceSubscriptionFromTimeLocked(*f.port, 3000));
        CHECK(f.port->revision == 8);
        CHECK_THROWS_AS(d::replaceSubscriptionFromTimeLocked(*f.port, 1000), a::Error);
        CHECK(f.port->revision == 8);
        CHECK(f.port->fromTime == 3000);
        CHECK(f.port->symbols[0].fromTime() == 3000);
        CHECK(f.port->observers.latest == saved);
        CHECK(f.port->statistics->snapshot().desiredRevision == 8);
    }
    f.port->observers.phases.clear();
    f.port->observers.count = 0;
    f.port->observers.phases.resize(1);
    f.port->changes->maximumWork = 0;
    {
        d::SubscriptionObserverAdmission admission(f.port->changes);
        std::lock_guard lock(f.port->mutex);
        try {
            static_cast<void>(d::replaceSubscriptionFromTimeLocked(*f.port, 1000));
            FAIL("observer work must be funded before accepted range publication");
        } catch (const a::Error &error) {
            CHECK(error.diagnostic().code == a::ErrorCode::WORK_LIMIT);
        }
        CHECK(f.port->revision == 8);
        CHECK(f.port->fromTime == 3000);
        CHECK(f.port->observers.count == 0);
    }
    f.port->changes->maximumWork = 8;
    f.port->observers.listeners = std::make_shared<const d::ListenerSnapshot>();
    {
        d::SubscriptionObserverAdmission admission(f.port->changes);
        std::lock_guard lock(f.port->mutex);
        REQUIRE(d::commitSubscriptionDesiredLocked(*f.port, {}, 3000));
        REQUIRE(d::replaceSubscriptionFromTimeLocked(*f.port, -1001));
        CHECK(f.port->symbols.empty());
        CHECK(f.port->revision == 10);
        CHECK(f.port->fromTime == -1001);
        f.port->revision = std::numeric_limits<std::uint64_t>::max();
        CHECK_THROWS_AS(d::replaceSubscriptionFromTimeLocked(*f.port, 0), a::Error);
        CHECK(f.port->fromTime == -1001);
        CHECK(f.port->symbols.empty());
    }
}

TEST_CASE("public HISTORY contract is explicit and current business mappers fail before native registration") {
    a::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(runtimeOptions);
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    auto endpoint = runtime.createEndpoint(options);
    REQUIRE(endpoint);
    const auto effective = endpoint->getEffectiveConfiguration();
    const auto contract = std::ranges::find_if(effective.properties, [](const auto &entry) {
        return entry.key == "qdcxx.deliveryContract";
    });
    REQUIRE(contract != effective.properties.end());
    CHECK(contract->effectiveValue == "history");

    for (auto type : {a::EventType::QUOTE, a::EventType::PROFILE, a::EventType::TRADE, a::EventType::TRADE_ETH,
                      a::EventType::SUMMARY, a::EventType::TEXT_MESSAGE, a::EventType::TEXT_CONFIGURATION,
                      a::EventType::MESSAGE, a::EventType::CONFIGURATION}) {
        try {
            static_cast<void>(endpoint->getFeed()->createSubscription({type}));
            FAIL("no current business mapper may gain HISTORY from a raw layout or enum fallback");
        } catch (const a::Error &error) {
            CHECK(error.diagnostic().code == a::ErrorCode::UNSUPPORTED_PROFILE);
            CHECK(error.diagnostic().stage == "subscription-history-event-type");
        }
    }

    auto ordinary = runtime.createEndpoint();
    auto quotes = ordinary->getFeed()->createSubscription<q::events::Quote>();
    auto cut = quotes->getHistorySnapshot();
    CHECK(cut.state == a::HistoryObservationState::UNSUPPORTED);
    CHECK(cut.ownerIdentity == quotes->getStatistics().ownerIdentity);
    CHECK(cut.ranges.empty());
    static_cast<void>(endpoint->closeAndAwaitTermination());
    static_cast<void>(ordinary->closeAndAwaitTermination());
    runtime.closeAndAwaitTermination();
    CHECK(quotes->getHistorySnapshot().final);
}

TEST_CASE("passive HISTORY reads fence accepted clear desired source and callback changes without mutating old cuts") {
    ObservationFixture f;
    const auto before = d::copyHistorySnapshot(f.port);
    REQUIRE(before.state == a::HistoryObservationState::CURRENT);
    REQUIRE(before.ranges.size() == 1);
    CHECK(before.ranges[0].status.completeDeliveredRange);
    {
        std::lock_guard lock(f.endpoint->mutex);
        ++f.endpoint->clearRevision;
    }
    auto clear = d::copyHistorySnapshot(f.port);
    CHECK(clear.state == a::HistoryObservationState::RESET_PENDING);
    CHECK(clear.acceptedClearRevision == 3);
    CHECK(clear.clearedRevision == 2);
    CHECK_FALSE(clear.ranges[0].status.completeDeliveredRange);
    {
        std::lock_guard lock(f.endpoint->mutex);
        --f.endpoint->clearRevision;
    }
    {
        std::lock_guard lock(f.port->mutex);
        ++f.port->revision;
    }
    CHECK(d::copyHistorySnapshot(f.port).state == a::HistoryObservationState::NOT_MAPPED);
    {
        std::lock_guard lock(f.port->mutex);
        --f.port->revision;
    }
    {
        std::lock_guard lock(f.port->mutex);
        f.port->callbackSynchronousExecuting = true;
    }
    CHECK(d::copyHistorySnapshot(f.port).state == a::HistoryObservationState::CALLBACK_PENDING);
    {
        std::lock_guard lock(f.port->mutex);
        f.port->callbackSynchronousExecuting = false;
        ++f.port->callbackSynchronousRetirementRevision;
    }
    CHECK(d::copyHistorySnapshot(f.port).state == a::HistoryObservationState::CALLBACK_PENDING);
    CHECK(before.ranges[0].status.completeDeliveredRange);
    CHECK(before.acceptedClearRevision == 2);
    f.port->closeRequested.store(true);
    CHECK(d::copyHistorySnapshot(f.port).state == a::HistoryObservationState::RETIRED);
    f.port->retired.store(true);
    f.endpoint.reset();
    auto final = d::copyHistorySnapshot(f.port);
    CHECK(final.final);
    CHECK(final.state == a::HistoryObservationState::RETIRED);
    CHECK(final.ownerIdentity == before.ownerIdentity);
    CHECK_FALSE(final.ranges[0].status.completeDeliveredRange);
}

TEST_CASE("HISTORY passive reader overlaps actual producer fence commits without work progress or callback waits") {
    ObservationFixture f;
    std::atomic<bool> consistent{true};
    std::atomic<unsigned> observations{0};
    std::jthread reader([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            auto cut = d::copyHistorySnapshot(f.port);

            if (cut.state == a::HistoryObservationState::CURRENT &&
                (cut.acceptedClearRevision != cut.clearRevision || cut.acceptedLogicalRevision != cut.logicalRevision))
                consistent = false;

            ++observations;
            std::this_thread::yield();
        }
    });

    for (unsigned turn = 0; turn < 100; ++turn) {
        std::lock_guard lock(f.endpoint->mutex);
        f.endpoint->clearRevision = turn % 2 ? 2 : 3;
    }

    while (!observations.load())
        std::this_thread::yield();

    reader.request_stop();
    reader.join();
    CHECK(consistent.load());
    CHECK(observations.load() > 0);
    CHECK(f.port->history.publicationSerial == 4);
}

namespace {
namespace c = q::core;
namespace m = q::mux;
namespace t = q::transport;
namespace r = q::test::recovery;
using namespace std::chrono_literals;

/** Actual standard TimeAndSale declarations: physical VOID tails do not become logical fields. */
c::LocalSchema timeAndSaleSchema() {
    const std::array<c::LocalRecord, 1> records{{{0,
                                                  u"TimeAndSale",
                                                  true,
                                                  {{56}, {72}, {2}, {104}, {120}, {120}, {120}, {120}, {40}, {8}},
                                                  {{u"Time", 0},
                                                   {u"Sequence", 1},
                                                   {u"Exchange", 2},
                                                   {u"TradeId", 3},
                                                   {u"Price", 4},
                                                   {u"Size", 5},
                                                   {u"BidPrice", 6},
                                                   {u"AskPrice", 7},
                                                   {u"SaleConditions", 8},
                                                   {u"Flags", 9}},
                                                  c::IndexedTimePairInt32{0, 1}}}};
    auto schema = c::LocalSchema::create(records, {16, 128, 256, 128, 16384}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);

    return std::move(*schema);
}

/** Read exact bytes produced by pinned Java BinaryQTPComposer, without a native business mapper. */
std::vector<std::byte> wireStep(unsigned step) {
    const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures" / "history-receiver" /
                      ("wire-step-" + std::to_string(step) + ".qtp");
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input);
    const std::vector<char> raw{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    REQUIRE(raw.size() <= 65536);
    std::vector<std::byte> bytes;

    for (const auto unit : raw)
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(unit)));

    return bytes;
}

/** Read a physical bound from the actual attached Java subscription fixture, not a native recomputation.
 * @param phase Unique captured ADD phase under the standard TimeAndSale route.
 * @return Exact observed signed raw HISTORY bound.
 */
std::int64_t attachedDemandBound(std::string_view phase) {
    const auto path =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures" / "live-history-range" / "oracle.tsv";
    std::ifstream input(path);
    REQUIRE(input);
    const auto prefix = std::string("wire-demand\t") + std::string(phase) + "\tADD\tTimeAndSale\tIBM\t";

    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (!line.starts_with(prefix))
            continue;

        std::int64_t result = 0;
        const auto text = std::string_view(line).substr(prefix.size());
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
        REQUIRE(parsed.ec == std::errc{});
        REQUIRE(parsed.ptr == text.data() + text.size());

        return result;
    }

    FAIL("attached Java demand phase is missing");

    return 0;
}

/** Generic lower receiver only; it exercises the same production queue, receiver and facade observation helpers.
 * The fixture's whole callback finishes its real attached140 accounting; no public typed TimeAndSale is invented.
 */
struct HistoryRecorder final : q::client::SubscriptionConsumer {
    std::shared_ptr<d::SubscriptionPort> port;
    std::vector<m::OwnedMuxBatch> batches;
    bool callbackWasFenced = true;

    explicit HistoryRecorder(std::shared_ptr<d::SubscriptionPort> value)
        : SubscriptionConsumer(value->statistics), port(std::move(value)) {
    }

    m::BatchAction onBatch(const q::client::SubscriptionBatch &batch) override {
        struct Completion {
            std::shared_ptr<c::detail::DeliveryStatisticsState> state;
            std::size_t count;

            ~Completion() {
                state->update([&](auto &s) {
                    s.inFlightEvents -= count;
                    s.deliveredEvents.add(count);
                });
            }
        } completion{port->statistics, batch.records().size()};

        d::SubscriptionDataCallbackScope callback(port->changes);
        callbackWasFenced =
            callbackWasFenced && d::copyHistorySnapshot(port).state == a::HistoryObservationState::CALLBACK_PENDING;
        batches.push_back(batch.retain());

        return m::BatchAction::CONTINUE;
    }

    std::size_t records() const {
        std::size_t count = 0;

        for (const auto &batch : batches)
            count += batch.records().size();

        return count;
    }
};
} // namespace

TEST_CASE("HISTORY worker receiver publishes actual standard Java wire ranges with concurrent passive readers") {
    auto bootstrap = t::RuntimeBootstrap::create(); // Process state precedes both owned peer and reader threads.
    REQUIRE(bootstrap);
    q::test::TcpPeerScript script;
    script.initialBytes = r::ready();
    r::append(script.initialBytes, wireStep(0));
    r::append(script.initialBytes, wireStep(1));
    script.replyBytes = wireStep(2);
    script.readThroughMessage = 21;
    script.waitForControllerReply = true;
    script.waitForControllerClose = true;
    q::test::TcpPeerScript reconnected;
    reconnected.initialBytes = r::ready();

    for (unsigned step = 0; step < 3; ++step)
        r::append(reconnected.replyBytes, wireStep(step));

    reconnected.readThroughMessage = 21;
    reconnected.waitForControllerReply = true;
    reconnected.waitForControllerClose = true;
    q::test::OwnedTcpPeer peer({{std::move(script), std::move(reconnected)}});
    REQUIRE(peer.port() != 0);
    auto schema = timeAndSaleSchema();
    a::EventRegistryWork registryWork;
    auto registry = a::EventRegistry::create(schema, {}, {}, registryWork);
    REQUIRE(registry);
    a::RuntimeOptions runtimeOptions;
    REQUIRE(runtimeOptions.maximumSubscriptionsPerEndpoint == 8);
    runtimeOptions.maximumSymbolsPerSubscription = 1;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    options.maximumAttempts = 2;
    options.eventRegistry = *registry;
    auto profile = d::prepareReceivingProfile(runtimeOptions, options, "127.0.0.1", peer.port());
    CHECK(profile.family == c::SubscriptionFamily::HISTORY);
    CHECK(profile.endpoint.settings().budgets.session.historySnapshot);
    CHECK(profile.bindings.empty()); // Honest generic common seam; typed public eligibility is separate.
    auto initialWork = profile.queueWork;
    auto queue = d::createReceivingQueue(profile, initialWork);

    if (!queue) {
        INFO("queue error=", static_cast<int>(queue.error().error));

        if (queue.error().source) {
            INFO("source error=", std::visit(
                                      [](auto error) {
                                          return static_cast<int>(error);
                                      },
                                      *queue.error().source));
            REQUIRE(queue);
        }

        REQUIRE(queue);
    }

    REQUIRE(queue);
    std::array<ObservationFixture, 8> fixtures;
    std::array<std::shared_ptr<HistoryRecorder>, 8> consumers;
    std::array<m::MuxHandle, 8> handles;
    std::array<m::MuxCommandTicket, 8> tickets;
    // Two actual Java ranges, with overlapping high-range registrations at the default eight-view capacity.
    constexpr std::array<std::int64_t, 8> bounds{
        0, 8589934592LL, 8589934592LL, 8589934592LL, 8589934592LL, 8589934592LL, 8589934592LL, 8589934592LL};

    for (std::size_t index = 0; index < fixtures.size(); ++index) {
        auto &f = fixtures[index];
        f.endpoint->options = options;
        f.port->history.ranges.clear();
        f.port->history.state = a::HistoryObservationState::NOT_MAPPED;
        f.port->statistics->submittedSelectors = 1;
        f.port->statistics->submittedDesiredRevision = f.port->revision;
        consumers[index] = std::make_shared<HistoryRecorder>(f.port);
        q::client::ReceiverWork receiverWork{1000000, 100000000};
        auto work = profile.queueWork;
        auto opened = q::client::openSubscriptionReceiver(*queue, profile.schema, profile.family,
                                                          profile.recoveryOptions.selectors, {}, consumers[index],
                                                          {0, 0, 0}, profile.clientLimits, receiverWork, work);
        REQUIRE(opened);
        handles[index] = std::move(*opened);
        const c::SubscriptionRequest request{{profile.schema.recordContext(0), u"IBM"}, bounds[index]};
        auto submit = profile.submitWork;
        auto accepted = handles[index].replace(std::span(&request, 1), submit);
        REQUIRE(accepted);
        tickets[index] = std::move(*accepted);
    }

    // Establish both actual mappings before connecting; initial DATA cannot outrun the second view.
    bool mapped = false;

    for (unsigned turn = 0; turn < 256 && !mapped; ++turn) {
        auto work = profile.queueWork;
        REQUIRE(queue->progress(work));
        mapped = true;

        for (std::size_t index = 0; index < fixtures.size(); ++index) {
            const auto statistics = fixtures[index].port->statistics->snapshot();
            mapped = mapped && tickets[index].result().has_value() && statistics.activeDesiredRevision == 7 &&
                     statistics.activeSelectors == 1;
        }
    }

    REQUIRE(mapped);
    t::RecoveryQueues queues;
    queues.history = std::move(*queue);
    auto preparation = profile.preparationWork;
    auto recovery = t::TcpRecovery::create(*bootstrap, profile.endpoint, std::move(profile.blueprint),
                                           std::move(queues), profile.recoveryOptions, preparation);
    REQUIRE(recovery);
    REQUIRE(recovery->start());
    std::atomic<unsigned> observations{0};
    std::atomic<bool> consistent{true};
    std::jthread reader([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            for (const auto &f : fixtures) {
                auto cut = d::copyHistorySnapshot(f.port);

                if (cut.state == a::HistoryObservationState::CURRENT &&
                    (cut.logicalRevision != cut.acceptedLogicalRevision ||
                     cut.clearRevision != cut.acceptedClearRevision ||
                     cut.connectionRevision != cut.acceptedConnectionRevision))
                    consistent = false;
            }

            ++observations;
            std::this_thread::yield();
        }
    });
    auto pump = [&](auto predicate) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;

        do {
            auto turn = profile.turnWork;

            for (unsigned action = 0; action < 64; ++action) {
                const auto result = recovery->poll(turn);
                REQUIRE(result);
            }

            auto observationWork = profile.historyObservationWork;

            for (std::size_t index = 0; index < fixtures.size(); ++index)
                d::publishHistorySnapshot(*recovery, handles[index], fixtures[index].port, observationWork);

            if (predicate())
                return;

            std::this_thread::yield();
        } while (std::chrono::steady_clock::now() < deadline);

        INFO("phase=", static_cast<int>(recovery->status().phase), " stage=", recovery->status().first.stage);
        INFO("low=", consumers[0]->records(), " high=", consumers[1]->records());
        const auto peerState = peer.snapshot();
        INFO("peerError=", static_cast<int>(peerState.error), " accepted=", peerState.sessions[0].accepted,
             " initial=", peerState.sessions[0].initialBytesWritten,
             " captured=", peerState.sessions[0].received.size(),
             " demanded=", static_cast<unsigned>(recovery->status().demandedFamilies),
             " converged=", static_cast<unsigned>(recovery->status().convergedFamilies));
        const auto lowState = d::copyHistorySnapshot(fixtures[0].port);
        INFO("lowObservation=", static_cast<int>(lowState.state), " rows=", lowState.ranges.size(),
             " sourceReset=", lowState.sourceResetPending);
        const auto io = recovery->statistics();
        INFO("readBytes=", io.readBytes.value, " readFrames=", io.readMessages.value,
             " readRecords=", io.readDataRecords.value, " writtenFrames=", io.writtenMessages.value,
             " writtenSubscriptions=", io.writtenSubscriptionRecords.value);
        const auto highState = d::copyHistorySnapshot(fixtures[1].port);
        INFO("high state=", static_cast<int>(highState.state), " rows=", highState.ranges.size());

        if (!highState.ranges.empty()) {
            const auto &range = highState.ranges[0].status;
            INFO("high bound=", range.requestedFrom, " mapped=", range.mappingCurrent, " queued=", range.queuedForKey,
                 " executing=", range.executingForKey, " gap=", range.hadGap, " source=", range.source.has_value());

            if (range.source) {
                INFO("frontier=", range.source->sourceFrontier,
                     " sourceComplete=", range.source->completeRequestedRange,
                     " initialPending=", range.source->initialPending, " viewTX=", range.source->viewTransactionDirty);
                REQUIRE(predicate());
            }

            REQUIRE(predicate());
        }

        REQUIRE(predicate());
    };
    pump([&] {
        const auto high = d::copyHistorySnapshot(fixtures[1].port);

        return std::ranges::all_of(consumers,
                                   [](const auto &consumer) {
                                       return consumer->records() == 2;
                                   }) &&
               high.ranges.size() == 1 && high.ranges[0].status.completeDeliveredRange;
    });
    const auto partial = d::copyHistorySnapshot(fixtures[0].port);
    const auto high = d::copyHistorySnapshot(fixtures[1].port);
    REQUIRE(partial.ranges.size() == 1);
    CHECK(partial.state == a::HistoryObservationState::CURRENT);
    CHECK_FALSE(partial.ranges[0].status.completeDeliveredRange);
    CHECK(partial.ranges[0].status.requestedFrom == bounds[0]);
    CHECK(high.ranges[0].status.requestedFrom == bounds[1]);
    CHECK(high.ranges[0].status.completeDeliveredRange);
    const auto first = consumers[0]->batches[0].records()[0];
    REQUIRE(first.typedValues.size() == 10);
    CHECK(first.rawTime == 12884901888LL);
    CHECK(first.eventFlags == 4);
    CHECK(first.typedValues[4].raw == 774530);  // Actual Java wide-decimal30.25 bits.
    CHECK(first.typedValues[5].raw == 2572930); // Actual Java wide-decimal100.5 bits.
    peer.allowReply(0);
    pump([&] {
        const auto low = d::copyHistorySnapshot(fixtures[0].port);

        return low.ranges.size() == 1 && low.ranges[0].status.completeDeliveredRange;
    });
    const auto complete = d::copyHistorySnapshot(fixtures[0].port);
    CHECK(consumers[0]->records() == 3);
    CHECK(consumers[1]->records() == 2);
    CHECK(complete.sourceEpoch.has_value());

    for (std::size_t index = 0; index < fixtures.size(); ++index) {
        CHECK(consumers[index]->callbackWasFenced);
        const auto stats = fixtures[index].port->statistics->snapshot();
        CHECK(stats.admittedEvents.value == consumers[index]->records());
        CHECK(stats.deliveredEvents.value == consumers[index]->records());
        CHECK(stats.pendingEvents == 0);
        CHECK(stats.inFlightEvents == 0);
    }

    auto noWork = profile.historyObservationWork;
    noWork.operations = 0;
    d::publishHistorySnapshot(*recovery, handles[0], fixtures[0].port, noWork);
    const auto refused = d::copyHistorySnapshot(fixtures[0].port);
    CHECK(refused.state == a::HistoryObservationState::PUBLICATION_FAILED);
    CHECK(refused.ranges.empty());
    CHECK(complete.ranges[0].status.completeDeliveredRange); // Saved owning cut remains AS-OF.
    {
        std::lock_guard lock(fixtures[0].endpoint->mutex);
        ++fixtures[0].endpoint->clearRevision;
    }
    CHECK(d::copyHistorySnapshot(fixtures[0].port).state == a::HistoryObservationState::RESET_PENDING);
    REQUIRE(recovery->requestDisconnect(true));
    pump([&] {
        for (auto &handle : handles)
            if (const auto notice = handle.resetNotice())
                static_cast<void>(handle.acknowledgeReset(notice->sequence));

        return recovery->status().phase == t::RecoveryPhase::SUSPENDED;
    });
    CHECK_FALSE(d::copyHistorySnapshot(fixtures[1].port).state == a::HistoryObservationState::CURRENT);
    peer.allowClose(0);

    for (auto &f : fixtures) {
        std::lock_guard lock(f.endpoint->mutex);
        f.endpoint->clearedRevision = f.endpoint->clearRevision;
        ++f.endpoint->connectionRevision;
        f.endpoint->activeDeliveryRevision = f.endpoint->connectionRevision;
    }

    REQUIRE(recovery->requestReconnect());
    pump([&] {
        return recovery->status().phase == t::RecoveryPhase::ACTIVE && std::ranges::all_of(fixtures, [](const auto &f) {
                   return f.port->statistics->snapshot().activeDesiredRevision == 7;
               });
    });
    peer.allowReply(1);
    pump([&] {
        const auto fresh = d::copyHistorySnapshot(fixtures[0].port);

        const bool delivered = consumers[0]->records() == 6 &&
                               std::ranges::all_of(std::span(consumers).subspan(1), [](const auto &consumer) {
                                   return consumer->records() == 4;
                               });

        return delivered && fresh.ranges.size() == 1 && fresh.ranges[0].status.completeDeliveredRange;
    });
    const auto fresh = d::copyHistorySnapshot(fixtures[0].port);
    REQUIRE(fresh.sourceEpoch);
    CHECK(*fresh.sourceEpoch > *complete.sourceEpoch);
    CHECK_FALSE(fresh.ranges[0].status.epoch.matches(complete.ranges[0].status.epoch));
    CHECK(complete.ranges[0].status.completeDeliveredRange);
    CHECK(consumers[1]->records() == 4);

    for (std::size_t index = 0; index < fixtures.size(); ++index) {
        const auto statistics = fixtures[index].port->statistics->snapshot();
        CHECK(statistics.generation == 1);
        CHECK(statistics.admittedEvents.value == consumers[index]->records());
        CHECK(statistics.deliveredEvents.value == consumers[index]->records());
        CHECK(statistics.pendingEvents == 0);
        CHECK(statistics.inFlightEvents == 0);
    }

    REQUIRE(recovery->requestStop());
    pump([&] {
        return recovery->status().phase == t::RecoveryPhase::CLOSED;
    });
    reader.request_stop();
    reader.join();
    CHECK(observations.load() > 0);
    CHECK(consistent.load());
    peer.stop();
    CHECK(first.typedValues[4].raw == 774530); // Retained immutable data outlives native retirement.
}

TEST_CASE("HISTORY defaults activate all eight reserved views and exact resident bounds refuse without overcommit") {
    const auto schema = timeAndSaleSchema();
    a::EventRegistryWork registryWork;
    auto registry = a::EventRegistry::create(schema, {}, {}, registryWork);
    REQUIRE(registry);
    a::RuntimeOptions runtime;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    REQUIRE(runtime.maximumSubscriptionsPerEndpoint == 8);
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    options.eventRegistry = *registry;
    auto profile = d::prepareReceivingProfile(runtime, options, "127.0.0.1", 1234);
    CHECK(profile.historyLimits.viewSlots == 8);
    CHECK(profile.historyLimits.pendingBytes == options.history.pendingBytesPerSubscription);
    auto construction = profile.queueWork;
    auto queue = d::createReceivingQueue(profile, construction);
    REQUIRE(queue);
    std::array<ObservationFixture, 8> fixtures;
    std::array<std::shared_ptr<HistoryRecorder>, 8> consumers;
    std::array<m::MuxHandle, 8> handles;
    std::array<m::MuxCommandTicket, 8> tickets;

    for (std::size_t index = 0; index < fixtures.size(); ++index) {
        auto &port = fixtures[index].port;
        port->statistics->submittedSelectors = 1;
        port->statistics->submittedDesiredRevision = 7;
        consumers[index] = std::make_shared<HistoryRecorder>(port);
        q::client::ReceiverWork receiverWork{1000000, 100000000};
        auto work = profile.queueWork;
        auto opened = q::client::openSubscriptionReceiver(*queue, profile.schema, profile.family,
                                                          profile.recoveryOptions.selectors, {}, consumers[index],
                                                          {0, 0, 0}, profile.clientLimits, receiverWork, work);
        REQUIRE(opened);
        handles[index] = std::move(*opened);
        const c::SubscriptionRequest request{{profile.schema.recordContext(0), u"IBM"},
                                             static_cast<std::int64_t>(index)};
        auto submit = profile.submitWork;
        auto ticket = handles[index].replace(std::span(&request, 1), submit);
        REQUIRE(ticket);
        tickets[index] = std::move(*ticket);
    }

    bool mapped = false;

    for (unsigned turn = 0; turn < 1024 && !mapped; ++turn) {
        auto work = profile.queueWork;
        REQUIRE(queue->progress(work));
        mapped = true;

        for (const auto &f : fixtures)
            mapped = mapped && f.port->statistics->snapshot().activeDesiredRevision == 7;
    }

    REQUIRE(mapped);

    for (const auto &ticket : tickets) {
        REQUIRE(ticket.result());
        CHECK(ticket.result()->status == m::CommandStatus::APPLIED);
    }

    // Measure actual fixed backing through the existing collector API, not a copied private sizeof formula.
    auto sourceSchema = profile.schema;
    auto historyWork = profile.queueWork.history;
    auto collector =
        q::collectors::HistoryCollector::create(std::move(sourceSchema), profile.historyLimits,
                                                {false, 100, 10000, profile.recoveryOptions.selectors}, historyWork);
    REQUIRE(collector);
    const auto reserve =
        profile.historyLimits.cacheBytes + profile.historyLimits.viewSlots * profile.historyLimits.pendingBytes;
    const auto exactResident = collector->residentBytes() + reserve;
    auto exactProfile = profile;
    exactProfile.historyLimits.residentBytes = exactResident;
    auto exactWork = exactProfile.queueWork;
    auto exact = d::createReceivingQueue(exactProfile, exactWork);
    REQUIRE(exact);
    --exactProfile.historyLimits.residentBytes;
    auto shortWork = exactProfile.queueWork;
    auto shortQueue = d::createReceivingQueue(exactProfile, shortWork);
    REQUIRE_FALSE(shortQueue);
    CHECK(shortQueue.error().error == m::QueueError::SOURCE_FAILURE);
    REQUIRE(shortQueue.error().source);
    CHECK(std::get<q::collectors::HistoryError>(*shortQueue.error().source) ==
          q::collectors::HistoryError::RESOURCE_LIMIT);
    auto under = options;
    under.history.maximumResidentBytes = reserve - 1;
    try {
        static_cast<void>(d::prepareReceivingProfile(runtime, under, "127.0.0.1", 1234));
        FAIL("aggregate whole-owner backing must refuse before native activation");
    } catch (const a::Error &error) {
        CHECK(error.diagnostic().code == a::ErrorCode::RESOURCE_LIMIT);
        CHECK(error.diagnostic().stage == "profile-history-resident-capacity");
    }
    auto overflow = options;
    overflow.history.pendingBytesPerSubscription = std::numeric_limits<std::size_t>::max();
    CHECK_THROWS_AS(static_cast<void>(d::prepareReceivingProfile(runtime, overflow, "127.0.0.1", 1234)), a::Error);
}

TEST_CASE("live HISTORY physical demand preserves MAX and coalesces range attributes without granting capability") {
    const auto schema = timeAndSaleSchema();
    std::vector<c::SubscriptionRequest> requests;
    requests.reserve(4);
    const auto append = [&](std::optional<std::int64_t> from, d::HistoryRangeEncoding encoding) {
        d::appendReceivingDesiredRequest(requests, schema.recordContext(0), u"IBM", c::SubscriptionSelectorKind::EXACT,
                                         c::SubscriptionFamily::HISTORY, from, encoding);
    };
    append(std::numeric_limits<std::int64_t>::max(), d::HistoryRangeEncoding::UNIX_MILLISECONDS_SEQUENCE);
    REQUIRE(requests.size() == 1);
    CHECK(requests[0].historyBound == std::numeric_limits<std::int64_t>::max());
    append(3000, d::HistoryRangeEncoding::UNIX_MILLISECONDS_SEQUENCE);
    CHECK(requests[0].historyBound == 12884901888LL); // Actual attached Java demand.
    append(2000, d::HistoryRangeEncoding::UNIX_MILLISECONDS_SEQUENCE);
    CHECK(requests[0].historyBound == 8589934592LL);
    CHECK_THROWS_AS(append(1000, d::HistoryRangeEncoding::INDEX_DOMAIN), a::Error);
    CHECK(requests[0].historyBound == 8589934592LL);
    append({}, d::HistoryRangeEncoding::INDEX_DOMAIN);
    CHECK(requests[0].historyBound == 0); // Ordinary/indexed view keeps the full physical range.
    CHECK(requests.size() == 1);
    CHECK_THROWS_AS(d::appendReceivingDesiredRequest(requests, schema.recordContext(0), u"IBM",
                                                     c::SubscriptionSelectorKind::EXACT, c::SubscriptionFamily::STREAM,
                                                     1000, d::HistoryRangeEncoding::UNIX_MILLISECONDS_SEQUENCE),
                    a::Error);
}

TEST_CASE("live HISTORY production ranges shrink expand disable remove and replay without cross-range callbacks") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    q::test::TcpPeerScript script;
    script.initialBytes = r::ready();
    r::append(script.initialBytes, wireStep(0));
    r::append(script.initialBytes, wireStep(1));
    // Expansion invalidates old coverage; the peer responds with a fresh actual BEGIN-through-END snapshot.
    script.replyBytes = wireStep(0);
    r::append(script.replyBytes, wireStep(1));
    r::append(script.replyBytes, wireStep(2));
    script.readThroughMessage = 21;
    script.waitForControllerReply = true;
    script.waitForClientClose = true;
    q::test::TcpPeerScript reconnected;
    reconnected.initialBytes = r::ready();
    reconnected.replyBytes = script.replyBytes;
    reconnected.readThroughMessage = 21;
    reconnected.waitForClientClose = true;
    q::test::OwnedTcpPeer peer({{std::move(script), std::move(reconnected)}});
    REQUIRE(peer.port() != 0);
    const auto schema = timeAndSaleSchema();
    a::EventRegistryWork registryWork;
    auto registry = a::EventRegistry::create(schema, {}, {}, registryWork);
    REQUIRE(registry);
    a::RuntimeOptions limits;
    limits.maximumSymbolsPerSubscription = 1;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    options.eventRegistry = *registry;
    auto profile = d::prepareReceivingProfile(limits, options, "127.0.0.1", peer.port());
    CHECK(profile.bindings.empty()); // No invented public TimeAndSale mapper.
    auto exactRouting = profile.routing.work;
    CHECK(d::prepayReceivingRouting(profile, exactRouting));
    CHECK(exactRouting.visits == 0);
    CHECK(exactRouting.textUnits == 0);
    CHECK(exactRouting.bytes == 0);
    auto shortRouting = profile.routing.work;
    REQUIRE(shortRouting.visits > 0);
    --shortRouting.visits;
    const auto savedRouting = shortRouting;
    CHECK_FALSE(d::prepayReceivingRouting(profile, shortRouting));
    CHECK(shortRouting.visits == savedRouting.visits);
    CHECK(shortRouting.textUnits == savedRouting.textUnits);
    CHECK(shortRouting.bytes == savedRouting.bytes);
    m::QueueSubmitWork noRouting{};
    CHECK_FALSE(d::prepayReceivingRouting(profile, noRouting));
    auto initialWork = profile.queueWork;
    auto queue = d::createReceivingQueue(profile, initialWork);
    REQUIRE(queue);
    ObservationFixture f;
    f.endpoint->options = options;
    f.port->timeSeries = true;
    f.port->maximumSymbols = 1;
    f.port->fromTime = 2000;
    f.port->symbols = {a::SubscriptionSelector::timeSeries(u"IBM", 2000)};
    f.port->history.ranges.clear();
    f.port->history.state = a::HistoryObservationState::NOT_MAPPED;
    auto consumer = std::make_shared<HistoryRecorder>(f.port);
    q::client::ReceiverWork receiverWork{100, 1000000};
    auto openWork = profile.queueWork;
    auto opened =
        q::client::openSubscriptionReceiver(*queue, profile.schema, profile.family, profile.recoveryOptions.selectors,
                                            {}, consumer, {0, 0, 0}, profile.clientLimits, receiverWork, openWork);
    REQUIRE(opened);
    auto handle = std::move(*opened);
    auto submit = [&] {
        std::lock_guard lock(f.port->mutex);
        std::vector<c::SubscriptionRequest> requests;
        requests.reserve(1);

        for (const auto &symbol : f.port->symbols)
            d::appendReceivingDesiredRequest(requests, schema.recordContext(0), symbol.eventSymbol(),
                                             c::SubscriptionSelectorKind::EXACT, profile.family, symbol.fromTime(),
                                             d::HistoryRangeEncoding::UNIX_MILLISECONDS_SEQUENCE);

        {
            std::lock_guard statisticsLock(f.port->statistics->mutex);
            f.port->statistics->submittedSelectors = f.port->symbols.size();
            f.port->statistics->submittedDesiredRevision = f.port->revision;
        }
        auto work = profile.submitWork;
        auto ticket = handle.replace(requests, work);
        REQUIRE(ticket);

        return std::move(*ticket);
    };
    auto ticket = submit();

    for (unsigned turn = 0; turn < 256 && !ticket.result(); ++turn) {
        auto work = profile.queueWork;
        REQUIRE(queue->progress(work));
    }

    REQUIRE(ticket.result());
    t::RecoveryQueues queues;
    queues.history = std::move(*queue);
    auto preparation = profile.preparationWork;
    auto owner = t::TcpRecovery::create(*bootstrap, profile.endpoint, std::move(profile.blueprint), std::move(queues),
                                        profile.recoveryOptions, preparation);
    REQUIRE(owner);
    REQUIRE(owner->start());
    std::string_view phase = "initial";
    auto pump = [&](auto predicate) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;

        do {
            auto work = profile.turnWork;

            for (unsigned action = 0; action < 64; ++action)
                REQUIRE(owner->poll(work));

            auto observation = profile.historyObservationWork;
            d::publishHistorySnapshot(*owner, handle, f.port, observation);

            if (predicate())
                return;

            std::this_thread::yield();
        } while (std::chrono::steady_clock::now() < deadline);

        INFO("phase=", static_cast<int>(owner->status().phase), " stage=", owner->status().first.stage,
             " callbacks=", consumer->records(), " revision=", f.port->revision);
        INFO("testPhase=", phase, " written=", owner->statistics().writtenSubscriptionRecords.value,
             " activeRevision=", f.port->statistics->snapshot().activeDesiredRevision.value_or(0),
             " readMessages=", owner->statistics().readMessages.value);
        const auto observed = d::copyHistorySnapshot(f.port);
        std::cout << "LIVE_RANGE state=" << static_cast<int>(observed.state) << " ranges=" << observed.ranges.size()
                  << " peerError=" << static_cast<int>(peer.snapshot().error)
                  << " replyWritten=" << peer.snapshot().sessions[0].replyBytesWritten
                  << " clientState=" << static_cast<int>(handle.state()) << '\n';

        if (const auto terminal = handle.terminal(); terminal && terminal->primary) {
            std::cout << "LIVE_RANGE terminal=" << static_cast<int>(terminal->primary->error);

            if (terminal->primary->source)
                std::cout << " source="
                          << std::visit(
                                 [](auto error) {
                                     return static_cast<int>(error);
                                 },
                                 *terminal->primary->source);

            std::cout << '\n';
        }

        for (const auto &range : observed.ranges) {
            std::cout << "LIVE_RANGE requested=" << range.status.requestedFrom
                      << " mapped=" << range.status.mappingCurrent << " queued=" << range.status.queuedForKey
                      << " gap=" << range.status.hadGap << " complete=" << range.status.completeDeliveredRange;

            if (range.status.source)
                std::cout << " sourceFrontier=" << range.status.source->sourceFrontier
                          << " sourceComplete=" << range.status.source->completeRequestedRange;

            std::cout << '\n';
        }

        for (const auto &batch : consumer->batches) {
            for (const auto &record : batch.records()) {
                std::cout << "LIVE_RANGE batchRevision=" << batch.appliedDesiredRevision().value_or(0)
                          << " rawTime=" << record.rawTime.value_or(-1) << " flags=" << record.eventFlags << '\n';
            }
        }

        REQUIRE(predicate());
    };
    auto mapped = [&] {
        return ticket.result() && f.port->statistics->snapshot().activeDesiredRevision == f.port->revision;
    };
    pump([&] {
        return consumer->records() == 2 && owner->statistics().writtenSubscriptionRecords.value == 1;
    });
    const auto first = d::copyHistorySnapshot(f.port);
    REQUIRE(first.ranges.size() == 1);
    CHECK(first.ranges[0].status.completeDeliveredRange);
    const auto retained = consumer->batches.front();
    REQUIRE(retained.appliedDesiredRevision() == 7);
    auto change = [&](std::int64_t fromTime) {
        {
            d::SubscriptionObserverAdmission admission(f.port->changes);
            std::lock_guard lock(f.port->mutex);
            REQUIRE(d::replaceSubscriptionFromTimeLocked(*f.port, fromTime));
        }
        CHECK(d::copyHistorySnapshot(f.port).state == a::HistoryObservationState::NOT_MAPPED);
        ticket = submit();
    };
    std::atomic<unsigned> reads{0};
    std::atomic<bool> coherent{true};
    std::jthread reader([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            const auto cut = d::copyHistorySnapshot(f.port);

            if (cut.state == a::HistoryObservationState::CURRENT && cut.logicalRevision != cut.acceptedLogicalRevision)
                coherent = false;

            ++reads;
            std::this_thread::yield();
        }
    });
    const auto beforeShrink = consumer->records();
    const auto beforeShrinkBatches = consumer->batches.size();
    phase = "shrink";
    change(3000);
    pump([&] {
        return mapped() && owner->statistics().writtenSubscriptionRecords.value == 2 &&
               consumer->records() > beforeShrink;
    });

    for (std::size_t batch = beforeShrinkBatches; batch < consumer->batches.size(); ++batch)
        for (const auto &record : consumer->batches[batch].records())
            CHECK(record.rawTime >= 12884901888LL);

    const auto beforeExpand = consumer->records();
    phase = "expand";
    change(1000);
    pump([&] {
        return mapped() && owner->statistics().writtenSubscriptionRecords.value == 3 &&
               consumer->records() > beforeExpand;
    });
    const auto partial = d::copyHistorySnapshot(f.port);
    REQUIRE(partial.ranges.size() == 1);
    CHECK_FALSE(partial.ranges[0].status.completeDeliveredRange);
    peer.allowReply(0);
    phase = "expanded-fresh-coverage";
    pump([&] {
        const auto cut = d::copyHistorySnapshot(f.port);

        return cut.ranges.size() == 1 && cut.ranges[0].status.completeDeliveredRange;
    });
    const auto beforeDisable = consumer->records();
    change(std::numeric_limits<std::int64_t>::max());
    pump([&] {
        return mapped() && owner->statistics().writtenSubscriptionRecords.value == 4;
    });
    CHECK(consumer->records() == beforeDisable);
    {
        d::SubscriptionObserverAdmission admission(f.port->changes);
        std::lock_guard lock(f.port->mutex);
        // Accepted changes coalesce before the worker captures a replacement; none of these ranges leak to wire.
        REQUIRE(d::replaceSubscriptionFromTimeLocked(*f.port, 1000));
        REQUIRE(d::replaceSubscriptionFromTimeLocked(*f.port, 2000));
        REQUIRE(d::commitSubscriptionDesiredLocked(*f.port, {}, f.port->fromTime));
    }
    ticket = submit();
    pump([&] {
        return mapped() && owner->statistics().writtenSubscriptionRecords.value == 5;
    });
    CHECK(d::copyHistorySnapshot(f.port).ranges.empty());
    CHECK(retained.appliedDesiredRevision() == 7);
    CHECK(retained.records()[0].symbol == u"IBM");
    CHECK(first.ranges[0].status.completeDeliveredRange);
    const auto oldSourceEpoch = first.sourceEpoch;
    // Keep the selected old lease, but release completed test captures before the eight-batch reservation fills.
    consumer->batches.clear();
    const auto beforeReconnect = consumer->records();
    REQUIRE(owner->requestDisconnect(true));
    phase = "disconnect";
    pump([&] {
        if (const auto notice = handle.resetNotice())
            static_cast<void>(handle.acknowledgeReset(notice->sequence));

        return owner->status().phase == t::RecoveryPhase::SUSPENDED;
    });
    {
        std::lock_guard lock(f.endpoint->mutex);
        ++f.endpoint->connectionRevision;
        f.endpoint->activeDeliveryRevision = f.endpoint->connectionRevision;
    }
    {
        d::SubscriptionObserverAdmission admission(f.port->changes);
        std::lock_guard lock(f.port->mutex);
        REQUIRE(d::replaceSubscriptionFromTimeLocked(*f.port, 1000));
        REQUIRE(d::commitSubscriptionDesiredLocked(*f.port, {a::SubscriptionSelector::timeSeries(u"IBM", 1000)},
                                                   f.port->fromTime));
    }
    ticket = submit();
    REQUIRE(owner->requestReconnect());
    phase = "reconnected-latest-range";
    pump([&] {
        const auto cut = d::copyHistorySnapshot(f.port);

        return mapped() && consumer->records() > beforeReconnect && cut.ranges.size() == 1 &&
               cut.ranges[0].status.requestedFrom == 4294967296LL && cut.ranges[0].status.completeDeliveredRange;
    });
    const auto fresh = d::copyHistorySnapshot(f.port);
    REQUIRE(fresh.sourceEpoch);
    REQUIRE(oldSourceEpoch);
    CHECK(*fresh.sourceEpoch > *oldSourceEpoch);
    CHECK_FALSE(fresh.ranges[0].status.epoch.matches(first.ranges[0].status.epoch));
    CHECK(first.ranges[0].status.completeDeliveredRange);
    REQUIRE(owner->requestStop());
    pump([&] {
        return owner->status().phase == t::RecoveryPhase::CLOSED;
    });
    reader.request_stop();
    reader.join();
    CHECK(reads.load() > 0);
    CHECK(coherent.load());
    const auto finish = std::chrono::steady_clock::now() + 5s;

    while (!peer.snapshot().finished && std::chrono::steady_clock::now() < finish)
        std::this_thread::yield();

    const auto captured = peer.snapshot();
    REQUIRE(captured.error == q::test::TcpPeerError::NONE);
    REQUIRE(captured.finished);
    auto decodeDemand = [&](std::span<const std::byte> bytes) {
        q::protocol::FrameReader frames({65536, 65545});
        std::vector<std::pair<std::int32_t, std::optional<std::int64_t>>> demand;

        while (!bytes.empty()) {
            auto frame = frames.feed(bytes);
            bytes = bytes.subspan(frame.accepted);
            REQUIRE(frame.status != q::protocol::FrameStatus::FAILED);

            if (!frame.frame || (frame.frame->type != 21 && frame.frame->type != 22))
                continue;

            auto payload = std::span<const std::byte>(frame.frame->payload);
            q::encoding::SymbolReader symbols({128, 128, 128}, 1);
            auto symbol = symbols.read(payload);
            REQUIRE(symbol.value);
            CHECK(symbol.value->units == u"IBM");
            payload = payload.subspan(symbol.consumed);
            const auto id = q::encoding::readCompactInt(payload);
            REQUIRE(id.status == q::encoding::Status::OK);
            payload = payload.subspan(id.consumed);
            std::optional<std::int64_t> bound;

            if (frame.frame->type == 21) {
                const auto value = q::encoding::readCompactLong(payload);
                REQUIRE(value.status == q::encoding::Status::OK);
                payload = payload.subspan(value.consumed);
                bound = value.value;
            }

            CHECK(payload.empty());
            demand.emplace_back(*frame.frame->type, bound);
        }

        return demand;
    };
    const auto demand = decodeDemand(captured.sessions[0].received);
    const std::vector<std::pair<std::int32_t, std::optional<std::int64_t>>> expected{
        {21, attachedDemandBound("first-range")},
        {21, attachedDemandBound("queued-after-shrink")},
        {21, attachedDemandBound("expand")},
        {21, attachedDemandBound("disable")},
        {22, {}}};
    CHECK(demand == expected); // Actual attached Java transition semantics, independently decoded native wire.
    const std::vector<std::pair<std::int32_t, std::optional<std::int64_t>>> reconnectedDemand{
        {21, attachedDemandBound("expand")}};
    CHECK(decodeDemand(captured.sessions[1].received) == reconnectedDemand);
}
