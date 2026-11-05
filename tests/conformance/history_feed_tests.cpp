// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal finite runner: public eligibility and the production detached HISTORY observation boundary.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../core/delivery_statistics.hpp"
#include "history_observation.hpp"
#include "qd/api/endpoint_configuration.hpp"
#include "qd/api/feed.hpp"
#include "receiving_profile.hpp"
#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"
#include "subscription_observer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
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
