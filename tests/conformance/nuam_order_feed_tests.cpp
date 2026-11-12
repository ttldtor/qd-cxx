// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "evidence_cells.hpp"
#include "qd/api/feed.hpp"
#include "qd/events/nuam_order.hpp"
#include "qd/protocol/subscription_writer.hpp"
#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace a = q::api;
namespace e = q::events;
namespace p = q::protocol;
using namespace std::chrono_literals;

struct Observation {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<e::NuamOrder> received;
};

std::optional<std::u16string> text(const std::string &value) {
    if (value == "<null>")
        return std::nullopt;

    return std::u16string(value.begin(), value.end());
}

void verify(const std::filesystem::path &root, const std::string &stem) {
    std::ifstream expected(root / (stem + ".tsv"));
    REQUIRE(expected);
    std::vector<std::string> row;

    for (std::string line; std::getline(expected, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const auto cells = q::test_support::split(line);

        if (cells[0] == "event")
            row = cells;
    }

    REQUIRE(row.size() == 27);
    std::ifstream input(root / (stem + ".qtp"), std::ios::binary);
    REQUIRE(input);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const auto wire = std::as_bytes(std::span(bytes));
    q::test::TcpPeerScript script;
    script.initialBytes = q::test::recovery::ready();
    script.replyBytes.assign(wire.begin(), wire.end());
    script.readThroughMessage = std::to_underlying(p::SubscriptionMessage::HISTORY_ADD);
    script.waitForClientClose = true;
    q::test::OwnedTcpPeer peer({{std::move(script)}});
    REQUIRE(peer.port() != 0);
    const auto observation = std::make_shared<Observation>();
    a::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(runtimeOptions);
    a::EndpointOptions options;
    options.maximumAttempts = 1;
    options.contract = a::DeliveryContract::HISTORY;
    const auto endpoint = runtime.createEndpoint(options);
    const auto subscription = endpoint->getFeed()->createSubscription<e::NuamOrder>();
    const auto source = e::OrderSource::valueOf(std::stoi(row[4]));
    subscription->setSelectors(std::array{a::SubscriptionSelector::indexed(u"AAPL", source.indexedSource())});
    subscription->addEventListener([observation](std::span<const e::NuamOrder> batch) {
        {
            const std::lock_guard lock(observation->mutex);
            observation->received.insert(observation->received.end(), batch.begin(), batch.end());
        }
        observation->changed.notify_all();
    });
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    {
        std::unique_lock lock(observation->mutex);
        const bool delivered = observation->changed.wait_for(lock, 5s, [&] {
            return !observation->received.empty();
        });
        INFO("endpoint stage=", endpoint->getDiagnostic().stage,
             " subscription stage=", subscription->getDiagnostic().stage);
        REQUIRE(delivered);
    }
    const auto closed = endpoint->closeAndAwaitTermination();
    REQUIRE_FALSE(closed.deadlineExpired);
    INFO("closed drained=", closed.drained, " gap=", closed.hadGap, " stage=", closed.diagnostic.stage,
         " code=", static_cast<int>(closed.diagnostic.code));
    CHECK(endpoint->getStatistics().final);
    CHECK(subscription->getStatistics().final);
    REQUIRE(observation->received.size() == 1);
    const auto &value = observation->received[0];
    CHECK(value.symbol == std::u16string(row[1].begin(), row[1].end()));
    CHECK(value.eventFlags == std::stoi(row[2]));
    CHECK(value.index == std::stoll(row[3]));
    CHECK(value.getSourceId() == std::stoi(row[4]));
    CHECK(value.getTime() == std::stoll(row[5]));
    CHECK(value.getSequence() == std::stoi(row[6]));
    CHECK(value.actorId == std::stoi(row[7]));
    CHECK(value.participantId == std::stoi(row[8]));
    CHECK(value.submitterId == std::stoi(row[9]));
    CHECK(value.onBehalfOfSubmitterId == std::stoi(row[10]));
    CHECK(value.clientOrderId == text(row[11]));
    CHECK(value.customerAccount == text(row[12]));
    CHECK(value.customerInfo == text(row[13]));
    CHECK(value.exchangeInfo == text(row[14]));
    CHECK(value.timeInForceData == std::stoi(row[15]));
    CHECK(value.triggerOrderBookId == std::stoi(row[16]));
    CHECK(value.triggerSessionType == std::stoi(row[17]));
    CHECK(value.nuamFlags == std::stoi(row[18]));
    CHECK(std::to_underlying(value.getOrderType()) == std::stoi(row[19]));
    CHECK(std::to_underlying(value.getTimeInForce()) == std::stoi(row[20]));
    CHECK(std::bit_cast<std::int64_t>(value.getTriggerPrice()) == std::stoll(row[21]));
    CHECK(std::bit_cast<std::int64_t>(value.getOrderQuantity()) == std::stoll(row[22]));
    CHECK(std::bit_cast<std::int64_t>(value.getDisplayQuantity()) == std::stoll(row[23]));
    CHECK(std::bit_cast<std::int64_t>(value.getRefreshQuantity()) == std::stoll(row[24]));
    CHECK(std::bit_cast<std::int64_t>(value.getLeavesQuantity()) == std::stoll(row[25]));
    CHECK(std::bit_cast<std::int64_t>(value.getMatchedQuantity()) == std::stoll(row[26]));
}
} // namespace

TEST_CASE("Public NuamOrder matches production QD for all default indexed sources") {
    const std::filesystem::path root(ORG_TTLDTOR_QDCXX_NUAM_ORDER_FIXTURES);
    SUBCASE("DEFAULT") {
        verify(root, "default");
    }
    SUBCASE("NUAM") {
        verify(root, "upper");
    }
    SUBCASE("nuam") {
        verify(root, "lower");
    }
}
