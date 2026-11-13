// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/feed.hpp"

#include "connect_peer.hpp"
#include "transport/tls_test_support.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <utility>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace api = q::api;
namespace peer = q::connect_tests;
namespace test = q::test;
namespace tls = test::tls;
using namespace std::chrono_literals;

api::EndpointOptions endpointOptions() {
    auto resources = tls::options();
    constexpr std::size_t FEED_NATIVE_ALLOWANCE = 128 * 1024 * 1024; // Includes Feed's DNS and TCP/session backing.
    api::EndpointOptions result;
    result.maximumAttempts = 1;
    result.tls.emplace(api::VerifiedTlsOptions{"localhost", tls::read("ca.pem"), 5s, 1s, resources.limits,
                                               resources.host, resources.sdkInstanceBytes, resources.sdkSharedBytes,
                                               resources.residentBytes + FEED_NATIVE_ALLOWANCE});

    return result;
}

api::RuntimeOptions runtimeOptions() {
    api::RuntimeOptions result;
    result.dnsServers.push_back({{127, 0, 0, 1}});

    return result;
}

test::TlsTestPeer quotePeer() {
    const auto schema = tls::schema();
    const auto family = q::core::SubscriptionFamily::TICKER;
    auto script = tls::peerOptions();
    script.pauseBeforeHandshake = 100ms;
    auto bytes = peer::fixturePeerPrefix(schema, family);
    peer::Record quote{u"IBM", {}};

    for (const auto &field : schema.records().front().fields)
        quote.fields.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});

    const auto data = peer::fixturePeerData(std::span(&quote, 1), family, true);
    bytes.insert(bytes.end(), data.begin(), data.end());

    for (const auto byte : bytes)
        script.initialBytes.push_back(std::to_integer<std::uint8_t>(byte));

    auto value = test::TlsTestPeer::start(std::move(script));
    REQUIRE(value);

    return std::move(*value);
}
} // namespace

TEST_CASE("Feed owns explicit TLS inputs and delivers Quote only from the verified source") {
    auto server = quotePeer();
    api::ApiRuntime runtime(runtimeOptions());
    auto options = endpointOptions();
    auto endpoint = runtime.createEndpoint(options);
    options.tls.reset();
    CHECK_THROWS_AS(endpoint->connectTape("unused.qtp"), api::Error);
    auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    const auto received = std::make_shared<std::atomic<bool>>(false);
    subscription->addEventListener([received](auto quotes) {
        if (!quotes.empty() && quotes.front().symbol == u"IBM")
            received->store(true);
    });
    subscription->addSymbols("IBM");
    endpoint->connect("127.0.0.1:" + std::to_string(server.port()));
    const auto deadline = std::chrono::steady_clock::now() + 8s;
    const auto connected = endpoint->awaitState(api::EndpointState::CONNECTED, deadline);
    INFO("endpoint stage=" << endpoint->getDiagnostic().stage);
    REQUIRE(connected.status == api::WaitStatus::REACHED);

    while (!received->load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(2ms);

    REQUIRE(received->load());
    const auto stats = endpoint->getStatistics().connection;
    CHECK(stats.sourceEpoch == 1);
    CHECK(stats.readBytes.value > 0);
    CHECK(stats.readMessages.availability == q::core::StatisticAvailability::UNSUPPORTED);
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK(closed.drained);
    CHECK_FALSE(closed.hadGap);
    CHECK_FALSE(closed.deadlineExpired);
    const auto final = endpoint->getStatistics();
    CHECK(final.final);
    CHECK(final.connection.closedConnections.value == 1);
    CHECK_FALSE(final.connection.sourceEpoch);
    const auto peer = server.stop();
    CHECK(peer.handshakeComplete);
    CHECK(peer.sniLocalhost);
}

TEST_CASE("Feed TLS rejects an untrusted chain or mismatched SAN before delivery") {
    for (const bool wrongIdentity : {false, true}) {
        CAPTURE(wrongIdentity);
        auto server = quotePeer();
        api::ApiRuntime runtime(runtimeOptions());
        auto options = endpointOptions();

        if (wrongIdentity)
            options.tls->referenceIdentity = "wrong.example";
        else
            options.tls->trustPem = tls::read("other-ca.pem");

        auto endpoint = runtime.createEndpoint(std::move(options));
        const auto received = std::make_shared<std::atomic<bool>>(false);
        auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
        subscription->addSymbols("IBM");
        subscription->addEventListener([received](auto) {
            received->store(true);
        });
        endpoint->connect("127.0.0.1:" + std::to_string(server.port()));
        const auto failed = endpoint->awaitState(api::EndpointState::FAILED, std::chrono::steady_clock::now() + 8s);
        REQUIRE(failed.status == api::WaitStatus::REACHED);
        const auto diagnostic = endpoint->getDiagnostic();
        INFO("endpoint stage=" << diagnostic.stage);
        CHECK(diagnostic.code == api::ErrorCode::PROTOCOL_FAILURE);
        CHECK(diagnostic.stage.starts_with("tls-"));
        CHECK_FALSE(received->load());
        CHECK_FALSE(endpoint->getStatistics().connection.sourceEpoch);
        CHECK(endpoint->getStatistics().connection.establishedConnections.value == 0);
        static_cast<void>(endpoint->closeAndAwaitTermination());
    }
}

TEST_CASE("Feed TLS refuses an insufficient whole-run allowance before native connect") {
    auto server = quotePeer();
    api::ApiRuntime runtime(runtimeOptions());
    auto options = endpointOptions();
    options.tls->residentBytes = 1;
    auto endpoint = runtime.createEndpoint(std::move(options));
    endpoint->connect("127.0.0.1:" + std::to_string(server.port()));
    const auto failed = endpoint->awaitState(api::EndpointState::FAILED, std::chrono::steady_clock::now() + 8s);
    REQUIRE(failed.status == api::WaitStatus::REACHED);
    const auto diagnostic = endpoint->getDiagnostic();
    CHECK(diagnostic.code == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(diagnostic.stage == "tls-resident-allowance");
    CHECK_FALSE(server.stop().accepted);
    static_cast<void>(endpoint->closeAndAwaitTermination());
}
