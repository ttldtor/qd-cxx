// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// CTest owns the only bounded loopback peer and joins every helper before runtime teardown.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/endpoint_configuration.hpp"
#include "recovery_test_peer.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <doctest/doctest.h>
#include <fstream>
#include <limits>
#include <sstream>
#include <thread>
using namespace org::ttldtor::qdcxx;

namespace {
/** @param snapshot Owning explicit observation. @param key Canonical supported key. @return Matching policy row. */
const api::EffectiveEndpointProperty &property(const api::EndpointConfigurationSnapshot &snapshot,
                                               std::string_view key) {
    const auto found = std::ranges::find(snapshot.properties, key, &api::EffectiveEndpointProperty::key);

    if (found == snapshot.properties.end())
        throw std::runtime_error("missing expected policy");

    return *found;
}

/** @param action Finite synchronous call. @return Detached error category without swallowing foreign failures. */
template <class Action> api::ErrorCode error(Action action) {
    try {
        action();
    } catch (const api::Error &failure) {
        return failure.diagnostic().code;
    }

    return api::ErrorCode::NONE;
}

/** @param address Explicit route. @return Pure configuration with that selected route. */
api::EndpointConfiguration configured(std::string_view address) {
    const std::array properties{api::EndpointProperty{"dxfeed.address", address}};

    return api::EndpointConfiguration::create({}, {}, properties);
}

/** @return Explicit runtime DNS; numeric loopback does not consume it. */
api::RuntimeOptions runtimeOptions() {
    api::RuntimeOptions result;
    result.dnsServers.push_back({{127, 0, 0, 1}});

    return result;
}
} // namespace

TEST_CASE("configuration owns explicit layers and reports selected origins and consumer normalization") {
    std::string label = "first";
    const std::array defaults{api::EndpointProperty{"qdcxx.name", label},
                              api::EndpointProperty{"qdcxx.retryDelayMillis", "31"},
                              api::EndpointProperty{"qdcxx.maximumAttempts", "2"}};
    const std::array overrides{
        api::EndpointProperty{"qdcxx.retryDelayMillis", "41"}, api::EndpointProperty{"qdcxx.retryDelayMillis", "51"},
        api::EndpointProperty{"dxfeed.address",
                              "localhost:1234[reconnectDelay=61][RECONNECTDELAY=71,qdcxx.name=inline]"}};
    auto configuration = api::EndpointConfiguration::create({}, defaults, overrides);
    label.assign("changed");
    auto snapshot = configuration.snapshot();
    CHECK(snapshot.properties.size() == 13);
    CHECK(snapshot.configuredAddress == "localhost:1234");
    CHECK_FALSE(snapshot.acceptedAddress);
    CHECK_FALSE(snapshot.activationRequested);
    CHECK(property(snapshot, "qdcxx.name").effectiveValue == "inline");
    CHECK(property(snapshot, "qdcxx.name").origin == api::EndpointPropertyOrigin::ADDRESS_PROPERTIES);
    CHECK(property(snapshot, "qdcxx.retryDelayMillis").requestedValue == "71");
    CHECK(configuration.options().retryDelay == std::chrono::milliseconds{71});
    CHECK(property(snapshot, "qdcxx.maximumAttempts").origin == api::EndpointPropertyOrigin::DEFAULT_PROPERTIES);
    snapshot.properties[0].effectiveValue = "caller edit";
    CHECK(property(configuration.snapshot(), "qdcxx.name").effectiveValue == "inline");
    const std::array one{api::EndpointProperty{"qdcxx.maximumAttempts", "1"},
                         api::EndpointProperty{"qdcxx.retryDelayMillis", "99"}};
    const auto single = api::EndpointConfiguration::create({}, {}, one).snapshot();
    CHECK(property(single, "qdcxx.retryDelayMillis").requestedValue == "99");
    CHECK(property(single, "qdcxx.retryDelayMillis").effectiveValue == "0");
}

TEST_CASE("configuration accepts and bounds the local aggregation property independently") {
    const std::array properties{api::EndpointProperty{"dxfeed.aggregationPeriod", "PT0.25S"}};
    const auto config = api::EndpointConfiguration::create({}, {}, properties);
    CHECK(config.options().aggregationPeriod == std::chrono::milliseconds{250});
    CHECK(property(config.snapshot(), "dxfeed.aggregationPeriod").origin ==
          api::EndpointPropertyOrigin::EXPLICIT_PROPERTIES);
    CHECK_FALSE(config.options().requestedAggregationPeriod);

    for (const auto *value : {"-1", "PT25H", "invalid"}) {
        const std::array invalid{api::EndpointProperty{"dxfeed.aggregationPeriod", value}};
        CHECK(error([&] {
                  static_cast<void>(api::EndpointConfiguration::create({}, {}, invalid));
              }) == api::ErrorCode::INVALID_ARGUMENT);
    }
}

TEST_CASE("configuration validates complete decimal boolean profile and finite input before publication") {
    for (auto value : {"", "-1", "+1", "1x", " 1", "0", "2147483648", "999999999999999999999999"}) {
        const std::array input{api::EndpointProperty{"qdcxx.connectTimeoutMillis", value}};
        CHECK(error([&] {
                  static_cast<void>(api::EndpointConfiguration::create({}, {}, input));
              }) == api::ErrorCode::INVALID_ARGUMENT);
    }

    const std::array maximum{api::EndpointProperty{"qdcxx.connectTimeoutMillis", "2147483647"},
                             api::EndpointProperty{"qdcxx.maximumAttempts", "4294967295"}};
    CHECK(api::EndpointConfiguration::create({}, {}, maximum).options().maximumAttempts ==
          std::numeric_limits<std::uint32_t>::max());
    const std::array wildcard{api::EndpointProperty{"qdcxx.deliveryContract", "stream"},
                              api::EndpointProperty{"dxfeed.wildcard.enable", "TrUe"},
                              api::EndpointProperty{"qdcxx.name", ""}};
    CHECK(api::EndpointConfiguration::create({}, {}, wildcard).options().enableWildcard);
    const std::array malformed{api::EndpointProperty{"dxfeed.wildcard.enable", "truthy"}};
    CHECK(error([&] {
              static_cast<void>(api::EndpointConfiguration::create({}, {}, malformed));
          }) == api::ErrorCode::INVALID_ARGUMENT);
    const std::array entries{api::EndpointProperty{"qdcxx.name", "a"}, api::EndpointProperty{"qdcxx.name", "b"}};
    api::EndpointConfigurationLimits limits;
    limits.properties = 1;
    CHECK(error([&] {
              static_cast<void>(api::EndpointConfiguration::create({}, {}, entries, limits));
          }) == api::ErrorCode::RESOURCE_LIMIT);
    limits.properties = 2;
    limits.totalBytes = 1;
    CHECK(error([&] {
              static_cast<void>(api::EndpointConfiguration::create({}, {}, entries, limits));
          }) == api::ErrorCode::RESOURCE_LIMIT);
    limits.totalBytes = 100;
    limits.keyBytes = 0;
    CHECK(error([&] {
              static_cast<void>(api::EndpointConfiguration::create({}, {}, {}, limits));
          }) == api::ErrorCode::INVALID_ARGUMENT);
}

TEST_CASE("configuration plain addresses validate DNS IPv4 IPv6 before any runtime exists") {
    for (auto route : {"localhost:1", "EXAMPLE.test.:65535", "127.0.0.1:1234", "[::1]:1234", "[2001:db8::1]:1234"}) {
        CHECK(configured(route).snapshot().configuredAddress == route);
    }

    for (auto route :
         {"", "host", "host:0", "host:65536", "host:-1", "host:+1", "host:1x", "host name:1", "bad_host:1", "-bad:1",
          "bad-:1", "host..test:1", "127.00.0.1:1", "999.1.1.1:1", "[not:ipv6]:1", "::1:1234", "[::1:1234"}) {
        CHECK(error([&] {
                  static_cast<void>(configured(route));
              }) != api::ErrorCode::NONE);
    }
}

TEST_CASE("configuration rejects unsupported extensions and secret inputs without echo or retention") {
    constexpr std::string_view secret = "NeverEchoSecret";

    for (auto key : {"user", "password", "dxfeed.user", "dxfeed.password", "name", "unknown"}) {
        const std::array input{api::EndpointProperty{key, secret}};
        try {
            static_cast<void>(api::EndpointConfiguration::create({}, {}, input));
            FAIL("unsupported input accepted");
        } catch (const api::Error &failure) {
            CHECK(failure.diagnostic().code == api::ErrorCode::UNSUPPORTED_PROFILE);
            CHECK(std::string_view(failure.what()).find(secret) == std::string_view::npos);
            CHECK(failure.diagnostic().stage.find(secret) == std::string_view::npos);
        }
    }

    for (auto route : {"tls+host:1", "file:/tmp/input", "host:1,other:2", ":1234", "user:NeverEchoSecret@host:1234",
                       "host:1(proxy=x)", "host:1[proxy=x]", "host:1[user=NeverEchoSecret]",
                       "host:1[reconnectDelay=1\\,2]", "host:1[reconnectDelay=(1)]"}) {
        CHECK(error([&] {
                  static_cast<void>(configured(route));
              }) != api::ErrorCode::NONE);
    }

    CHECK_FALSE(configured("localhost:1").snapshot().acceptedAddress);
}

TEST_CASE("configuration snapshots remain isolated and readable through concurrent explicit connect and retirement") {
    api::ApiRuntime runtime(runtimeOptions());
    test::TcpPeerScript script;
    script.waitForClientClose = true;
    test::TcpPeerConfig peerConfig;
    peerConfig.sessions.push_back(script);
    test::OwnedTcpPeer peer(std::move(peerConfig));
    REQUIRE(peer.port() != 0);
    const auto route = "127.0.0.1:" + std::to_string(peer.port());
    auto configuration = configured(route + "[qdcxx.maximumAttempts=1,qdcxx.name=one]");
    auto first = runtime.createEndpoint(configuration);
    auto second = runtime.createEndpoint(configured("127.0.0.1:1[qdcxx.name=two]"));
    CHECK(first->getState() == api::EndpointState::NOT_CONNECTED);
    CHECK_FALSE(peer.snapshot().sessions[0].accepted);

    for (auto bad : {"bad_host:1", "tls+localhost:1", "localhost:1[reconnectDelay=1]"}) {
        CHECK(error([&] {
                  first->connect(bad);
              }) != api::ErrorCode::NONE);
        CHECK(first->getEffectiveConfiguration().connectionRevision == 0);
        CHECK(first->getState() == api::EndpointState::NOT_CONNECTED);
    }

    std::barrier start(2);
    std::atomic<bool> consistent{true};
    std::jthread reader([&] {
        start.arrive_and_wait();

        for (unsigned i = 0; i < 2000; ++i) {
            const auto observed = first->getEffectiveConfiguration();

            if (observed.configuredAddress != route || property(observed, "qdcxx.name").effectiveValue != "one" ||
                (observed.connectionRevision == 0 && (observed.acceptedAddress || observed.activationRequested)) ||
                (observed.connectionRevision != 0 && observed.acceptedAddress != route))
                consistent = false;
        }
    });
    start.arrive_and_wait();
    first->connect();
    reader.join();
    CHECK(consistent.load());
    const auto accepted = first->getEffectiveConfiguration();
    CHECK(accepted.acceptedAddress == route);
    CHECK(accepted.activationRequested);
    CHECK(accepted.connectionRevision == 1);
    first->connect();
    CHECK(first->getEffectiveConfiguration().connectionRevision == 1);
    CHECK_FALSE(second->getEffectiveConfiguration().acceptedAddress);
    CHECK(property(second->getEffectiveConfiguration(), "qdcxx.name").effectiveValue == "two");
    first->disconnect();
    CHECK_FALSE(first->getEffectiveConfiguration().activationRequested);
    static_cast<void>(first->closeAndAwaitTermination());
    static_cast<void>(second->closeAndAwaitTermination());
    runtime.closeAndAwaitTermination();
    CHECK(first->getEffectiveConfiguration().acceptedAddress == route);
    CHECK_FALSE(first->getEffectiveConfiguration().activationRequested);
    CHECK(property(configuration.snapshot(), "qdcxx.name").effectiveValue == "one");
}

TEST_CASE("legacy endpoint defaults require explicit address and immutable configuration does not consume slots") {
    api::RuntimeOptions options = runtimeOptions();
    options.maximumEndpoints = 1;
    api::ApiRuntime runtime(options);
    auto endpoint = runtime.createEndpoint({});
    CHECK(error([&] {
              endpoint->connect();
          }) == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(endpoint->getEffectiveConfiguration().connectionRevision == 0);

    for (unsigned i = 0; i < 100; ++i) {
        CHECK(error([&] {
                  static_cast<void>(configured("host:0"));
              }) == api::ErrorCode::INVALID_ARGUMENT);
    }

    CHECK(error([&] {
              static_cast<void>(runtime.createEndpoint());
          }) == api::ErrorCode::RESOURCE_LIMIT);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    auto replacement = runtime.createEndpoint();
    CHECK(replacement->getState() == api::EndpointState::NOT_CONNECTED);
    static_cast<void>(replacement->closeAndAwaitTermination());
}

TEST_CASE("native admitted aliases agree with pinned production connector values and preserve explicit deviations") {
    std::ifstream fixture(ORG_TTLDTOR_QDCXX_ENDPOINT_CONFIGURATION_FIXTURE);
    REQUIRE(fixture.good());
    std::size_t rows = 0, matched = 0;
    const std::array routes{"localhost:1234", "localhost:1234[reconnectDelay=17]",
                            "localhost:1234[reconnectDelay=17,reconnectDelay=29]",
                            "localhost:1234[reconnectDelay=17][RECONNECTDELAY=31]"};

    for (std::string line; std::getline(fixture, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        ++rows;
        std::istringstream cells(line);
        std::string kind, id, outcome, count, delay;
        std::getline(cells, kind, '\t');
        std::getline(cells, id, '\t');
        std::getline(cells, outcome, '\t');

        if (kind == "connector" && (id == "1" || id == "2" || id == "3")) {
            std::getline(cells, count, '\t');
            std::getline(cells, delay, '\t');
            REQUIRE(outcome == "ok");
            REQUIRE(count == "1");
            const auto configuration = configured(routes[static_cast<std::size_t>(std::stoul(id))]);
            CHECK(configuration.options().retryDelay.count() == std::stoll(delay));
            ++matched;
        }

        if (kind == "connector" && id == "4") {
            CHECK(outcome == "ok");
            CHECK(error([&] {
                      static_cast<void>(configured("localhost:1234(reconnectDelay=37)"));
                  }) == api::ErrorCode::UNSUPPORTED_PROFILE);
            ++matched;
        }

        if (kind == "property" && id == "malformed-boolean-retained") {
            CHECK(outcome == "truthy");
            const std::array input{api::EndpointProperty{"dxfeed.wildcard.enable", outcome}};
            CHECK(error([&] {
                      static_cast<void>(api::EndpointConfiguration::create({}, {}, input));
                  }) == api::ErrorCode::INVALID_ARGUMENT);
            ++matched;
        }
    }

    CHECK(rows == 44);
    CHECK(matched == 5);
}

TEST_CASE(
    "configured endpoints preserve nine registered kinds and independent statistics through observer retirement") {
    api::ApiRuntime runtime(runtimeOptions());
    const std::array properties{api::EndpointProperty{"qdcxx.deliveryContract", "stream"},
                                api::EndpointProperty{"qdcxx.name", "union"},
                                api::EndpointProperty{"qdcxx.maximumAttempts", "1"}};
    auto endpoint = runtime.createEndpoint(api::EndpointConfiguration::create({}, {}, properties));
    auto second = runtime.createEndpoint();
    const auto identity = endpoint->getStatistics().ownerIdentity;
    REQUIRE(identity);
    CHECK(second->getStatistics().ownerIdentity != identity);
    const std::vector kinds{api::EventType::QUOTE,
                            api::EventType::PROFILE,
                            api::EventType::TRADE,
                            api::EventType::TRADE_ETH,
                            api::EventType::SUMMARY,
                            api::EventType::TEXT_MESSAGE,
                            api::EventType::TEXT_CONFIGURATION,
                            api::EventType::MESSAGE,
                            api::EventType::CONFIGURATION};
    auto mixed = endpoint->getFeed()->createSubscription(kinds);
    auto message = endpoint->getFeed()->createSubscription<events::Message>();
    auto configuration = endpoint->getFeed()->createSubscription<events::Configuration>();
    CHECK(mixed->getEventTypes() == kinds);
    CHECK(message->getEventTypes() == std::vector{api::EventType::MESSAGE});
    CHECK(configuration->getEventTypes() == std::vector{api::EventType::CONFIGURATION});
    unsigned added = 0;
    mixed->addChangeListener([&](const api::SubscriptionChange &change) {
        if (change.kind == api::SubscriptionChangeKind::ADDED) {
            ++added;
            CHECK(change.snapshot.eventTypes == kinds);
            CHECK(endpoint->getStatistics().ownerIdentity == identity);
            CHECK_FALSE(endpoint->getStatistics().final);
            CHECK(property(endpoint->getEffectiveConfiguration(), "qdcxx.name").effectiveValue == "union");
        }
    });
    mixed->addSymbols("TOPIC");
    CHECK(added == 1);
    const auto retained = mixed->getSnapshot();
    static_cast<void>(endpoint->closeAndAwaitTermination());
    const auto statistics = endpoint->getStatistics();
    CHECK(statistics.final);
    CHECK(statistics.ownerIdentity == identity);
    CHECK(retained.eventTypes == kinds);
    CHECK_FALSE(endpoint->getEffectiveConfiguration().activationRequested);
    CHECK(property(endpoint->getEffectiveConfiguration(), "qdcxx.name").effectiveValue == "union");
    CHECK_FALSE(second->getStatistics().final);
    static_cast<void>(second->closeAndAwaitTermination());
    CHECK(second->getStatistics().final);
}
