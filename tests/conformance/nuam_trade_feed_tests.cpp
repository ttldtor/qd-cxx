// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "connect_peer.hpp"
#include "qd/api/feed.hpp"
#include "qd/events/nuam_trade.hpp"
#include "qd/protocol/subscription_writer.hpp"
#include "recovery_test_peer.hpp"

#include <array>
#include <chrono>
#include <condition_variable>
#include <doctest/doctest.h>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace a = q::api;
namespace e = q::events;
namespace c = q::core;
namespace peer = q::connect_tests;
using namespace std::chrono_literals;

/** Deliver a complete NuamTrade through an actual SDK endpoint and native loopback peer.
 * @param contract Qualified TICKER or STREAM delivery contract.
 */
void checkFeed(a::DeliveryContract contract) {
    constexpr c::SchemaLimits schemaLimits{1, 32, 64, 128, 4096};
    const auto schema = e::defaultNuamTradeSchema(schemaLimits);
    REQUIRE(schema);
    std::vector<c::FieldValue> fields;

    for (const auto &field : schema->records()[0].fields) {
        fields.push_back({static_cast<c::SerialFieldType>(field.serialTypeId), 0});
    }

    fields[5].raw = 0x180;
    fields[6].raw = 0x280;
    fields[13].raw = -123456789;
    fields[14].raw = 0x380;
    fields[15].raw = 0x480;
    fields[16].raw = 77;
    fields[17].raw = 0x580;
    const std::array records{peer::Record{u"IBM", fields}};
    const auto family =
        contract == a::DeliveryContract::TICKER ? c::SubscriptionFamily::TICKER : c::SubscriptionFamily::STREAM;
    q::test::TcpPeerScript script;
    script.initialBytes = peer::fixturePeerPrefix(*schema, family);
    script.replyBytes = peer::fixturePeerData(records, family, true);
    script.readThroughMessage =
        std::to_underlying(contract == a::DeliveryContract::TICKER ? q::protocol::SubscriptionMessage::TICKER_ADD
                                                                   : q::protocol::SubscriptionMessage::STREAM_ADD);
    script.waitForClientClose = true;
    q::test::OwnedTcpPeer source(q::test::TcpPeerConfig{{script}});
    REQUIRE(source.port() != 0);
    a::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(runtimeOptions);
    a::EndpointOptions options;
    options.contract = contract;
    auto endpoint = runtime.createEndpoint(options);
    auto subscription = endpoint->getFeed()->createSubscription<e::NuamTrade>();

    struct Observation {
        std::mutex mutex;
        std::condition_variable changed;
        std::optional<e::NuamTrade> event;
    };

    auto observation = std::make_shared<Observation>();
    subscription->addSymbols("IBM");
    subscription->addEventListener([observation](std::span<const e::NuamTrade> batch) {
        {
            const std::lock_guard lock(observation->mutex);

            if (!batch.empty()) {
                observation->event = batch.front();
            }
        }
        observation->changed.notify_all();
    });
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    {
        std::unique_lock lock(observation->mutex);
        const bool delivered = observation->changed.wait_for(lock, 5s, [&] {
            return observation->event.has_value();
        });
        INFO("endpoint=", static_cast<int>(endpoint->getState()), " stage=", endpoint->getDiagnostic().stage,
             " subscription=", static_cast<int>(subscription->getState()),
             " stage=", subscription->getDiagnostic().stage);
        REQUIRE(delivered);
        CHECK(observation->event->symbol == u"IBM");
        CHECK(observation->event->getPrice() == 1);
        CHECK(observation->event->getSizeAsDouble() == 2);
        CHECK(observation->event->getTradeStatTime() == -123456789);
        CHECK(observation->event->getLastSignificantPrice() == 3);
        CHECK(observation->event->getLastPriceForAll() == 4);
        CHECK(observation->event->getNumberOfTrades() == 77);
        CHECK(observation->event->getVWAP() == 5);
    }
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK_FALSE(closed.deadlineExpired);
    CHECK(source.snapshot().error == q::test::TcpPeerError::NONE);
}
} // namespace

TEST_CASE("NuamTrade reaches typed Feed subscriptions on TICKER and STREAM") {
    checkFeed(a::DeliveryContract::TICKER);
    checkFeed(a::DeliveryContract::STREAM);
}
