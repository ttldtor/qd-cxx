// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/feed.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/time_helpers.hpp"
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
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace a = q::api;
namespace e = q::events;
using namespace std::chrono_literals;

std::vector<std::vector<std::string>> eventRows(const std::filesystem::path &path) {
    std::ifstream input(path);
    REQUIRE(input);
    std::vector<std::vector<std::string>> result;

    for (std::string row; std::getline(input, row);) {
        if (!row.starts_with("event\t"))
            continue;

        std::istringstream cells(row);
        std::vector<std::string> fields;

        for (std::string cell; std::getline(cells, cell, '\t');)
            fields.push_back(std::move(cell));

        REQUIRE(fields.size() > 8);
        result.push_back(std::move(fields));
    }

    return result;
}

auto businessValues(const e::Greeks &value) {
    return std::array{value.getPrice(), value.getVolatility(), value.getDelta(), value.getGamma(),
                      value.getTheta(), value.getRho(),        value.getVega()};
}

auto businessValues(const e::Underlying &value) {
    return std::array{value.getVolatility(),  value.getFrontVolatility(), value.getBackVolatility(),
                      value.getCallVolume(),  value.getPutVolume(),       value.getOptionVolume(),
                      value.getPutCallRatio()};
}

auto businessValues(const e::TheoPrice &value) {
    return std::array{value.getPrice(), value.getUnderlyingPrice(), value.getDelta(),
                      value.getGamma(), value.getDividend(),        value.getInterest()};
}

template <class Event> void checkEvent(const Event &value, const std::vector<std::string> &row) {
    CHECK(value.symbol == std::u16string(row[1].begin(), row[1].end()));
    CHECK(value.eventTime == std::stoll(row[2]));
    CHECK(value.getSource().id() == std::stoi(row[4]));
    CHECK(value.getIndex() == std::stoll(row[5]));
    CHECK(value.getTime() == std::stoll(row[6]));
    CHECK(value.getSequence() == std::stoi(row[7]));
    const auto numeric = businessValues(value);
    REQUIRE(row.size() == numeric.size() + 8);

    for (std::size_t index = 0; index < numeric.size(); ++index)
        CHECK(std::bit_cast<std::int64_t>(numeric[index]) == std::stoll(row[index + 8]));
}

template <class Event, class Schema>
void verify(const std::filesystem::path &root, a::EventType type, Schema defaultSchema, bool history, bool tiny,
            std::int64_t fromTime, bool reconnect = false) {
    const auto stem = history ? "history" : "ticker";
    const auto rows = eventRows(root / (std::string(stem) + ".tsv"));
    REQUIRE(rows.size() == (history ? 3 : 1));
    const bool narrowed = history && fromTime != 0;
    const std::size_t expectedCount = narrowed ? 2 : rows.size();
    std::ifstream input(root / (std::string(stem) + ".qtp"), std::ios::binary);
    REQUIRE(input);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const auto wire = std::as_bytes(std::span(bytes));
    q::test::TcpPeerScript script;
    script.initialBytes = q::test::recovery::ready();
    script.replyBytes.assign(wire.begin(), wire.end());
    using q::protocol::SubscriptionMessage;
    script.readThroughMessage =
        std::to_underlying(history ? SubscriptionMessage::HISTORY_ADD : SubscriptionMessage::TICKER_ADD);
    script.waitForClientClose = true;
    q::test::TcpPeerConfig config{{script}};

    if (reconnect)
        config.sessions.push_back(std::move(script));

    q::test::OwnedTcpPeer peer(std::move(config));
    REQUIRE(peer.port() != 0);
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<Event> received;
    a::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(runtimeOptions);
    a::EndpointOptions options;
    options.maximumAttempts = reconnect ? 2 : 1;
    options.contract = history ? a::DeliveryContract::HISTORY : a::DeliveryContract::TICKER;

    if (tiny) {
        a::EventRegistryLimits limits;
        const auto standard = defaultSchema(limits.schema);
        REQUIRE(standard);
        auto record = standard->records().front();

        for (auto &field : record.fields)
            if (field.serialTypeId == std::to_underlying(q::core::SerialFieldType::WIDE_DECIMAL))
                field.serialTypeId = std::to_underlying(q::core::SerialFieldType::DECIMAL);

        const auto schema =
            q::core::LocalSchema::create(std::span(&record, 1), limits.schema, q::core::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);
        const std::array routes{a::EventRegistration{type, 0}};
        a::EventRegistryWork work;
        const auto registry = a::EventRegistry::create(*schema, routes, limits, work);
        REQUIRE(registry);
        options.eventRegistry = *registry;
    }

    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    auto subscription = history ? feed->createTimeSeriesSubscription<Event>() : feed->createSubscription<Event>();

    if (history)
        subscription->setFromTime(fromTime);

    subscription->addSymbols("AAPL");
    subscription->addEventListener([&](std::span<const Event> batch) {
        {
            const std::lock_guard lock(mutex);
            received.insert(received.end(), batch.begin(), batch.end());
        }

        changed.notify_all();
    });
    const auto waitForRecords = [&](std::size_t count) {
        std::unique_lock lock(mutex);
        const bool delivered = changed.wait_for(lock, 5s, [&] {
            return received.size() >= count;
        });
        INFO("endpoint=", static_cast<int>(endpoint->getState()), " stage=", endpoint->getDiagnostic().stage,
             " subscription=", static_cast<int>(subscription->getState()),
             " stage=", subscription->getDiagnostic().stage, " records=", received.size());
        REQUIRE(delivered);
    };
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    waitForRecords(expectedCount);

    const auto awaitCoverage = [&](std::optional<std::uint64_t> previousEpoch) {
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        auto snapshot = subscription->getHistorySnapshot();
        const auto complete = [&] {
            return snapshot.state == a::HistoryObservationState::CURRENT && snapshot.ranges.size() == 1 &&
                   snapshot.ranges.front().status.completeDeliveredRange && snapshot.sourceEpoch &&
                   snapshot.sourceEpoch != previousEpoch;
        };

        while (!complete() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
            snapshot = subscription->getHistorySnapshot();
        }

        REQUIRE(complete());
        CHECK(snapshot.ranges.front().status.requestedFrom == e::withTimeSequenceTime(0, fromTime));

        return snapshot.sourceEpoch;
    };
    const auto initialEpoch = history ? awaitCoverage({}) : std::nullopt;

    if (reconnect) {
        endpoint->reconnect();
        waitForRecords(2 * expectedCount);

        if (history)
            static_cast<void>(awaitCoverage(initialEpoch));
    }

    const auto closed = endpoint->closeAndAwaitTermination();
    REQUIRE_FALSE(closed.deadlineExpired);
    CHECK(endpoint->getStatistics().final);
    CHECK(subscription->getStatistics().final);

    if (!reconnect)
        CHECK(closed.drained);

    REQUIRE(received.size() == (reconnect ? 2 : 1) * expectedCount);

    if (narrowed) {
        checkEvent(received.front(), rows.front());
        CHECK(received.back().getTime() == fromTime);
        CHECK(received.back().getSequence() == 0);
        CHECK(received.back().eventTime == 0);
    } else {
        for (std::size_t index = 0; index < rows.size(); ++index)
            checkEvent(received[index], rows[index]);

        if (reconnect)
            for (std::size_t index = 0; index < rows.size(); ++index)
                checkEvent(received[expectedCount + index], rows[index]);
    }

    CHECK(received.front().eventFlags == std::stoi(rows.front()[3]));
    CHECK(received.back().eventFlags == std::stoi(rows.back()[3]));
    const auto peerStatus = peer.snapshot();
    CHECK(peerStatus.sessions.front().matched);

    if (reconnect)
        CHECK(peerStatus.sessions.back().matched);
}

void verifyGreeks(const std::filesystem::path &root, bool history, bool tiny = false, std::int64_t fromTime = 0) {
    verify<e::Greeks>(root, a::EventType::GREEKS, e::defaultGreeksSchema, history, tiny, fromTime);
}

void verifyUnderlying(const std::filesystem::path &root, bool history, bool tiny = false) {
    verify<e::Underlying>(root, a::EventType::UNDERLYING, e::defaultUnderlyingSchema, history, tiny, 0);
}

void verifyTheoPrice(const std::filesystem::path &root, bool history, bool tiny = false, bool reconnect = false) {
    verify<e::TheoPrice>(root, a::EventType::THEO_PRICE, e::defaultTheoPriceSchema, history, tiny, 0, reconnect);
}
} // namespace

TEST_CASE("Public Greeks matches pinned QD TICKER and time-series HISTORY values") {
    const std::filesystem::path root(QDCXX_GREEKS_FIXTURES);
    SUBCASE("TICKER wide") {
        verifyGreeks(root / "wide", false);
    }
    SUBCASE("TICKER tiny") {
        verifyGreeks(root / "tiny", false, true);
    }
    SUBCASE("HISTORY wide") {
        verifyGreeks(root / "wide", true);
    }
    SUBCASE("HISTORY tiny") {
        verifyGreeks(root / "tiny", true, true);
    }
    SUBCASE("HISTORY converts a nonzero time-series bound and projects the inclusive boundary") {
        verifyGreeks(root / "wide", true, false, 1000);
    }
}

TEST_CASE("Public Underlying matches pinned QD TICKER and time-series HISTORY values") {
    const std::filesystem::path root(QDCXX_UNDERLYING_FIXTURES);
    SUBCASE("TICKER wide") {
        verifyUnderlying(root / "wide", false);
    }
    SUBCASE("TICKER tiny") {
        verifyUnderlying(root / "tiny", false, true);
    }
    SUBCASE("HISTORY wide") {
        verifyUnderlying(root / "wide", true);
    }
    SUBCASE("HISTORY tiny") {
        verifyUnderlying(root / "tiny", true, true);
    }
}

TEST_CASE("Public TheoPrice matches pinned QD values and replays the time-series range after reconnect") {
    const std::filesystem::path root(QDCXX_THEO_PRICE_FIXTURES);
    SUBCASE("TICKER wide") {
        verifyTheoPrice(root / "wide", false);
    }
    SUBCASE("TICKER tiny") {
        verifyTheoPrice(root / "tiny", false, true);
    }
    SUBCASE("HISTORY wide with a new connection") {
        verifyTheoPrice(root / "wide", true, false, true);
    }
    SUBCASE("HISTORY tiny") {
        verifyTheoPrice(root / "tiny", true, true);
    }
}
