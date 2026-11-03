// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: finite controlled loopback peers and explicit immutable registry snapshots.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/event_registry.hpp"
#include "qd/api/feed.hpp"

#include "connect_peer.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace api = root::api;
namespace core = root::core;
namespace events = root::events;
namespace peer = root::connect_tests;
using namespace std::chrono_literals;

/** Test-owned callback observation retained through any failure-path runtime retirement. */
struct Observation {
    std::mutex mutex;                    ///< Protects the complete independently owned event copies.
    std::vector<events::Quote> received; ///< Values remain valid after callback/schema/native retirement.
    std::atomic<std::size_t> count{0};   ///< Release/acquire completion observation, without native progress work.
};

/** Owning Profile callback observation, safe through failed-check runtime retirement. */
struct ProfileObservation {
    std::mutex mutex;                      ///< Protects copied Profile values.
    std::vector<events::Profile> received; ///< Owns nullable strings independently of generic/native storage.
    std::atomic<std::size_t> count{0};     ///< Release/acquire delivery observation.
};

/** Construct two caller-named Quote records after a generic record with explicit aliases and reordered fields.
 * @return Complete loader-independent binary schema, using explicit local IDs1/2 for Quotes.
 */
core::LocalSchema customSchema() {
    std::array records{core::LocalRecord{0, u"Generic", false, {{8}}, {{u"Value", 0}}},
                       core::LocalRecord{1,
                                         u"FirstCustomQuote",
                                         false,
                                         {{8}, {56}, {8}, {8}, {8}, {8}},
                                         {{u"WirePrice", 2},
                                          {u"BidPrice", 2},
                                          {u"BidSize", 0},
                                          {u"BidTime", 1},
                                          {u"AskPrice", 3},
                                          {u"AskSize", 4},
                                          {u"Extra", 5}}},
                       core::LocalRecord{2,
                                         u"SecondCustomQuote",
                                         false,
                                         {{8}, {56}, {8}, {8}, {8}, {8}},
                                         {{u"WirePrice", 2},
                                          {u"BidPrice", 2},
                                          {u"BidSize", 0},
                                          {u"BidTime", 1},
                                          {u"AskPrice", 3},
                                          {u"AskSize", 4},
                                          {u"Extra", 5}}}};
    auto result = core::LocalSchema::create(records, {16, 128, 256, 128, 16384}, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Fill every configured local field with recognizable exact values.
 * @param seconds Distinguishing optional bid time.
 * @param bidPrice Distinguishing required integer price, admitted by the existing adapter.
 * @return Complete configured local-order values, including the generic extra field.
 */
std::vector<core::FieldValue> values(std::int64_t seconds, std::int64_t bidPrice) {
    return {{core::SerialFieldType::COMPACT_INT, 2},        {core::SerialFieldType::TIME_SECONDS, seconds},
            {core::SerialFieldType::COMPACT_INT, bidPrice}, {core::SerialFieldType::COMPACT_INT, 23},
            {core::SerialFieldType::COMPACT_INT, 5},        {core::SerialFieldType::COMPACT_INT, 77}};
}

/** Observe detached state without driving native or queue work.
 * @tparam Predicate Atomic/locked test-owned observation.
 * @param predicate Complete observation callable.
 * @return Whether the observation became true within the finite whole wait.
 */
template <class Predicate> bool eventually(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        std::this_thread::sleep_for(1ms);
    }

    return true;
}
} // namespace

TEST_CASE("public single-type subscription routes every registered custom record through immutable schema snapshot") {
    for (const auto family : {core::SubscriptionFamily::STREAM, core::SubscriptionFamily::TICKER}) {
        auto source = customSchema();
        const std::array routes{api::EventRegistration{api::EventType::QUOTE, 1},
                                api::EventRegistration{api::EventType::QUOTE, 2}};
        api::EventRegistryWork work;
        auto configured = api::EventRegistry::create(source, routes, {}, work);
        REQUIRE(configured);

        api::RuntimeOptions runtimeOptions;
        runtimeOptions.maximumSubscriptionsPerEndpoint = 1;
        runtimeOptions.maximumSymbolsPerSubscription = 1;
        runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
        api::ApiRuntime runtime(runtimeOptions);
        api::EndpointOptions options;
        options.contract =
            family == core::SubscriptionFamily::STREAM ? api::DeliveryContract::STREAM : api::DeliveryContract::TICKER;
        options.maximumAttempts = 1;
        options.readTimeout = 5000ms;
        options.drainTimeout = 100ms;
        options.eventRegistry = *configured;
        auto endpoint = runtime.createEndpoint(options);
        auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();

        try {
            static_cast<void>(endpoint->getFeed()->createSubscription<events::Profile>());
            FAIL("An unregistered typed event must reject before opening a logical subscription");
        } catch (const api::Error &error) {
            CHECK(error.diagnostic().code == api::ErrorCode::UNSUPPORTED_PROFILE);
        }

        peer::PeerScript script;
        script.family = family;
        script.canonicalSymbols = true;
        script.describeAllRecords = true;
        script.subscriptionRecordIds = {1, 2};
        std::array records{peer::Record{u"REG", values(3, 11), 0, {}, 1}, peer::Record{u"REG", values(9, 19), 0, {}, 2},
                           peer::Record{u"REG", {{core::SerialFieldType::COMPACT_INT, 101}}, 0, {}, 0}};
        const std::array<std::u16string, 2> expected{u"REG", u"REG"};
        peer::Peer sourcePeer(source, records, expected, 1, peer::PeerMode::HOLD, script);

        // Replacing caller wrappers cannot replace the endpoint's already copied immutable state.
        options.eventRegistry.reset();
        work = {};
        configured = api::EventRegistry::builtIn({}, work);
        REQUIRE(configured);
        source = configured->schema();

        const auto observation = std::make_shared<Observation>();
        subscription->addEventListener([observation](std::span<const events::Quote> batch) {
            std::lock_guard lock(observation->mutex);
            observation->received.insert(observation->received.end(), batch.begin(), batch.end());
            observation->count.store(observation->received.size(), std::memory_order_release);
        });
        subscription->addSymbols("REG");
        endpoint->connect("127.0.0.1:" + std::to_string(sourcePeer.port()));
        const auto delivered = eventually([&] {
            return observation->count.load(std::memory_order_acquire) == 2 || sourcePeer.failed() ||
                   subscription->getDiagnostic().code != api::ErrorCode::NONE;
        });
        INFO("peer adds " << sourcePeer.adds() << " sent " << sourcePeer.initialDataSent() << " failed "
                          << sourcePeer.failed());
        INFO("endpoint " << static_cast<int>(endpoint->getState()) << " diagnostic "
                         << static_cast<int>(endpoint->getDiagnostic().code) << " " << endpoint->getDiagnostic().stage
                         << " subscription " << static_cast<int>(subscription->getState()) << " diagnostic "
                         << static_cast<int>(subscription->getDiagnostic().code) << " "
                         << subscription->getDiagnostic().stage);
        REQUIRE(delivered);
        CHECK_FALSE(sourcePeer.failed());
        CHECK(sourcePeer.adds() == 1);
        CHECK(subscription->getDiagnostic().code == api::ErrorCode::NONE);
        REQUIRE(observation->count.load(std::memory_order_acquire) == 2);
        subscription->clearSymbols();
        REQUIRE(eventually([&] {
            return sourcePeer.removes() == 1 || sourcePeer.failed() ||
                   subscription->getDiagnostic().code != api::ErrorCode::NONE;
        }));
        CHECK_FALSE(sourcePeer.failed());
        CHECK(sourcePeer.removes() == 1);
        CHECK(subscription->getDiagnostic().code == api::ErrorCode::NONE);
        static_cast<void>(endpoint->closeAndAwaitTermination());

        std::lock_guard lock(observation->mutex);
        auto &received = observation->received;
        REQUIRE(received.size() == 2);
        std::ranges::sort(received, {}, &events::Quote::bidTimeMs);
        CHECK(received[0].symbol == u"REG");
        CHECK(received[0].bidTimeMs == 3000);
        CHECK(received[0].bidPrice.toDouble() == 11.0);
        CHECK(received[1].bidTimeMs == 9000);
        CHECK(received[1].bidPrice.toDouble() == 19.0);
    }
}

TEST_CASE("registered Profile uses explicit record zero and reversed caller field layout") {
    constexpr core::SchemaLimits LIMITS{16, 128, 256, 128, 16384};
    auto builtin = events::defaultProfileSchema(LIMITS);
    REQUIRE(builtin);

    auto record = builtin->records()[0];
    record.name = u"ConfiguredProfile";
    std::ranges::reverse(record.fields);

    for (auto &lookup : record.lookups) {
        lookup.fieldId = record.fields.size() - 1 - lookup.fieldId;
    }

    auto source = core::LocalSchema::create(std::span(&record, 1), LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(source);

    const api::EventRegistration route{api::EventType::PROFILE, 0};
    api::EventRegistryWork work;
    auto registry = api::EventRegistry::create(*source, std::span(&route, 1), {}, work);
    REQUIRE(registry);

    std::vector<core::FieldValue> fields;

    for (const auto &field : record.fields) {
        const auto type = static_cast<core::SerialFieldType>(field.serialTypeId);
        core::FieldValue value{type, 0};

        if (type == core::SerialFieldType::UTF_CHAR_ARRAY) {
            value.units = u"Configured profile";
        } else if (type == core::SerialFieldType::TIME_SECONDS) {
            value.raw = 7;
        }

        fields.push_back(std::move(value));
    }

    const auto flags = std::ranges::find_if(record.lookups, [](const auto &lookup) {
        return lookup.name == u"Flags";
    });
    REQUIRE(flags != record.lookups.end());
    fields[flags->fieldId].raw = 83;
    const auto haltEnd = std::ranges::find_if(record.lookups, [](const auto &lookup) {
        return lookup.name == u"HaltEndTime";
    });
    REQUIRE(haltEnd != record.lookups.end());
    fields[haltEnd->fieldId].raw = 13;
    const auto reason = std::ranges::find_if(record.lookups, [](const auto &lookup) {
        return lookup.name == u"StatusReason";
    });
    REQUIRE(reason != record.lookups.end());
    fields[reason->fieldId].units = u"Independent reason";

    api::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    api::ApiRuntime runtime(runtimeOptions);
    api::EndpointOptions options;
    options.contract = api::DeliveryContract::STREAM;
    options.maximumAttempts = 1;
    options.drainTimeout = 100ms;
    options.eventRegistry = *registry;
    auto endpoint = runtime.createEndpoint(options);
    auto subscription = endpoint->getFeed()->createSubscription<events::Profile>();
    peer::PeerScript script;
    script.family = core::SubscriptionFamily::STREAM;
    script.canonicalSymbols = true;
    const peer::Record tuple{u"PROFILE", std::move(fields)};
    const std::array<std::u16string, 1> expected{u"PROFILE"};
    constexpr std::size_t PROFILE_FRAGMENT_BYTES =
        32; ///< Fragment descriptors without a sleep-per-byte deadline bound.
    peer::Peer sourcePeer(*source, std::span(&tuple, 1), expected, PROFILE_FRAGMENT_BYTES, peer::PeerMode::HOLD,
                          script);
    const auto observation = std::make_shared<ProfileObservation>();
    subscription->addEventListener([observation](std::span<const events::Profile> batch) {
        std::lock_guard lock(observation->mutex);
        observation->received.insert(observation->received.end(), batch.begin(), batch.end());
        observation->count.store(observation->received.size(), std::memory_order_release);
    });
    subscription->addSymbols("PROFILE");
    endpoint->connect("127.0.0.1:" + std::to_string(sourcePeer.port()));
    const auto delivered = eventually([&] {
        return observation->count.load(std::memory_order_acquire) != 0 || sourcePeer.failed() ||
               subscription->getDiagnostic().code != api::ErrorCode::NONE;
    });
    INFO("peer adds " << sourcePeer.adds() << " sent " << sourcePeer.initialDataSent() << " failed "
                      << sourcePeer.failed());
    INFO("endpoint " << static_cast<int>(endpoint->getState()) << " diagnostic "
                     << static_cast<int>(endpoint->getDiagnostic().code) << " " << endpoint->getDiagnostic().stage
                     << " subscription " << static_cast<int>(subscription->getState()) << " diagnostic "
                     << static_cast<int>(subscription->getDiagnostic().code) << " "
                     << subscription->getDiagnostic().stage);
    REQUIRE(delivered);
    CHECK_FALSE(sourcePeer.failed());
    CHECK(subscription->getDiagnostic().code == api::ErrorCode::NONE);
    REQUIRE(observation->count.load(std::memory_order_acquire) == 1);
    static_cast<void>(endpoint->closeAndAwaitTermination());

    std::lock_guard lock(observation->mutex);
    const auto &event = observation->received[0];
    CHECK(event.symbol == u"PROFILE");
    CHECK(event.flags == 83);
    CHECK(event.haltStartTimeMs == 7000);
    CHECK(event.haltEndTimeMs == 13000);
    REQUIRE(event.description);
    CHECK(*event.description == u"Configured profile");
    REQUIRE(event.statusReason);
    CHECK(*event.statusReason == u"Independent reason");
}

TEST_CASE("moved-from endpoint registry rejects before consuming the only endpoint reservation") {
    api::EventRegistryWork work;
    auto registry = api::EventRegistry::builtIn({}, work);
    REQUIRE(registry);

    auto valid = std::move(*registry);
    api::RuntimeOptions limits;
    limits.maximumEndpoints = 1;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    api::ApiRuntime runtime(limits);
    api::EndpointOptions options;
    options.eventRegistry = *registry;

    try {
        static_cast<void>(runtime.createEndpoint(options));
        FAIL("A moved-from registry must reject synchronously");
    } catch (const api::Error &error) {
        CHECK(error.diagnostic().code == api::ErrorCode::INVALID_ARGUMENT);
        CHECK(error.diagnostic().stage == "endpoint-registry");
    }

    options.eventRegistry = std::move(valid);
    auto endpoint = runtime.createEndpoint(options);
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    CHECK(subscription->getState() == api::SubscriptionState::CONFIGURED);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    CHECK(endpoint->getState() == api::EndpointState::CLOSED);
}
