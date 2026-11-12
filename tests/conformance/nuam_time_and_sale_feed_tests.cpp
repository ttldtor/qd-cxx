// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "event_registry_defaults.hpp"
#include "evidence_cells.hpp"
#include "feed_filter_coordinates.hpp"
#include "qd/api/feed.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/symbol_reader.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/nuam_time_and_sale.hpp"
#include "qd/events/time_helpers.hpp"
#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/subscription_writer.hpp"
#include "receiving_profile.hpp"
#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"

#include <algorithm>
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
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace a = q::api;
namespace c = q::core;
namespace e = q::events;
namespace p = q::protocol;
namespace wire = q::encoding;
namespace d = q::api::detail;
namespace r = q::test::recovery;
using namespace std::chrono_literals;
constexpr std::int32_t STREAM_COMPOSITE_ID = 37;
constexpr std::int32_t HISTORY_COMPOSITE_ID = 40;
constexpr auto REGIONAL_Q_OFFSET = 1 + u'Q' - u'A';

struct Fixture {
    c::LocalRecord record{0, u"NuamTimeAndSale", true, {}, {}, c::IndexedTimePairInt32{0, 1}};
    std::vector<std::vector<std::string>> rows;
};

/** Load the exact Java physical declaration and preserve nullable/empty TSV cells. */
Fixture readFixture(const std::filesystem::path &path) {
    std::ifstream input(path);
    REQUIRE(input);
    Fixture fixture;

    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const auto cells = q::test_support::split(line);

        if (cells[0] == "record") {
            REQUIRE(cells.size() == 4);
            fixture.record.name.assign(cells[2].begin(), cells[2].end());
            REQUIRE(cells[3] == "true");
        } else if (cells[0] == "int-field" || cells[0] == "object-field") {
            REQUIRE(cells.size() == 5);
            const auto type = std::stoi(cells[2]);

            if (!type)
                continue; // A Java long's physical VOID tail is not a logical native field.

            const std::u16string local(cells[3].begin(), cells[3].end());
            const std::u16string property(cells[4].begin(), cells[4].end());
            const auto index = fixture.record.fields.size();
            fixture.record.fields.push_back({type});
            fixture.record.lookups.push_back({local, index});

            if (local != property)
                fixture.record.lookups.push_back({property, index});
        } else if (cells[0] == "event") {
            REQUIRE(cells.size() == 32);
            fixture.rows.push_back(cells);
        }
    }

    REQUIRE(fixture.rows.size() >= 3);

    return fixture;
}

std::optional<std::u16string> text(const std::string &cell) {
    if (cell == "<null>")
        return std::nullopt;

    return std::u16string(cell.begin(), cell.end());
}

bool boolean(const std::string &cell) {
    REQUIRE((cell == "true" || cell == "false"));

    return cell == "true";
}

/** Compare all typed values with one actual Java delegate observation. */
void checkEvent(const e::NuamTimeAndSale &value, const std::vector<std::string> &row) {
    CHECK(value.symbol == std::u16string(row[2].begin(), row[2].end()));
    CHECK(value.eventTime == std::stoll(row[3]));
    CHECK(value.getSource().id() == std::stoi(row[5]));
    CHECK(value.getIndex() == std::stoll(row[6]));
    CHECK(value.getTime() == std::stoll(row[7]));
    CHECK(value.getTimeNanos() == std::stoll(row[8]));
    CHECK(value.getTimeNanoPart() == std::stoi(row[9]));
    CHECK(value.getSequence() == std::stoi(row[10]));
    CHECK(value.getExchangeCode() == static_cast<char16_t>(std::stoi(row[11])));
    CHECK(value.getTradeId() == std::stoll(row[12]));
    CHECK(std::bit_cast<std::int64_t>(value.getPrice()) == std::stoll(row[13]));
    CHECK(std::bit_cast<std::int64_t>(value.getSizeAsDouble()) == std::stoll(row[14]));
    CHECK(value.getSize() == std::stoll(row[15]));
    CHECK(std::bit_cast<std::int64_t>(value.getBidPrice()) == std::stoll(row[16]));
    CHECK(std::bit_cast<std::int64_t>(value.getAskPrice()) == std::stoll(row[17]));
    CHECK(value.getExchangeSaleConditions() == text(row[18]));
    CHECK(value.flags == std::stoi(row[19]));
    CHECK(value.getTradeThroughExempt() == static_cast<char16_t>(std::stoi(row[20])));
    CHECK(e::marketEnumCode(value.getAggressorSide()) == std::stoi(row[21]));
    CHECK(value.isSpreadLeg() == boolean(row[22]));
    CHECK(value.isExtendedTradingHours() == boolean(row[23]));
    CHECK(value.isValidTick() == boolean(row[24]));
    CHECK(e::marketEnumCode(value.getType()) == std::stoi(row[25]));
    CHECK(value.isNew() == boolean(row[26]));
    CHECK(value.isCorrection() == boolean(row[27]));
    CHECK(value.isCancel() == boolean(row[28]));
    CHECK(value.getBuyer() == text(row[29]));
    CHECK(value.getSeller() == text(row[30]));
    CHECK(value.getMatchId() == std::stoll(row[31]));
}

/** Inspect the captured actual ADD and require the physical symbol and exact HISTORY bound. */
void checkAdd(std::span<const std::byte> captured, bool history, bool regional, bool custom) {
    p::FrameReader frames({1024 * 1024, 1024 * 1024 + 9});
    const auto message =
        std::to_underlying(history ? p::SubscriptionMessage::HISTORY_ADD : p::SubscriptionMessage::STREAM_ADD);
    bool found = false;

    while (!captured.empty()) {
        const auto frame = frames.feed(captured);
        REQUIRE(frame.status != p::FrameStatus::FAILED);
        REQUIRE(frame.accepted > 0);
        captured = captured.subspan(frame.accepted);

        if (!frame.frame || frame.frame->type != message)
            continue;

        REQUIRE_FALSE(found);
        found = true;
        auto payload = std::span(frame.frame->payload);
        wire::SymbolReader symbols({8192, 128, 128}, 0);
        const auto symbol = symbols.read(payload);
        REQUIRE(symbol.status == wire::SymbolStatus::OK);
        REQUIRE(symbol.value);
        CHECK(symbol.value->units == u"AAPL"); // Regional logical AAPL&Q uses physical base AAPL.
        CHECK(symbol.value->eventFlags == 0);
        payload = payload.subspan(symbol.consumed);
        const auto record = wire::readCompactInt(payload);
        REQUIRE(record.status == wire::Status::OK);
        const auto compositeId = history ? HISTORY_COMPOSITE_ID : STREAM_COMPOSITE_ID;
        CHECK(record.value == (custom ? 0 : compositeId + (regional ? REGIONAL_Q_OFFSET : 0)));

        payload = payload.subspan(record.consumed);

        if (history) {
            const auto bound = wire::readCompactLong(payload);
            REQUIRE(bound.status == wire::Status::OK);
            CHECK(bound.value == e::withTimeSequenceTime(0, 0));
            payload = payload.subspan(bound.consumed);
        }

        CHECK(payload.empty());
    }

    CHECK(found);
}

/** Send exact Java QTP after the public source demand is observed. */
void verify(const std::filesystem::path &root, bool history, bool regional, bool custom) {
    const auto stem = std::string(regional ? "regional-" : "") + (history ? "history" : "stream");
    const auto fixture = readFixture(root / (stem + ".tsv"));
    REQUIRE(fixture.rows.size() == (history ? 4 : 3));
    std::ifstream input(root / (stem + ".qtp"), std::ios::binary);
    REQUIRE(input);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const auto data = std::as_bytes(std::span(bytes));
    q::test::TcpPeerScript script;
    script.initialBytes = q::test::recovery::ready();
    script.replyBytes.assign(data.begin(), data.end());
    script.readThroughMessage =
        std::to_underlying(history ? p::SubscriptionMessage::HISTORY_ADD : p::SubscriptionMessage::STREAM_ADD);
    script.waitForClientClose = true;
    q::test::OwnedTcpPeer peer({{std::move(script)}});
    REQUIRE(peer.port() != 0);

    struct Observation {
        std::mutex mutex;
        std::condition_variable changed;
        std::vector<e::NuamTimeAndSale> received;
    };

    const auto observation = std::make_shared<Observation>();
    a::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(limits);
    a::EndpointOptions options;
    options.maximumAttempts = 1;
    options.contract = history ? a::DeliveryContract::HISTORY : a::DeliveryContract::STREAM;

    if (custom) {
        const a::EventRegistryLimits registryLimits;
        const auto schema = c::LocalSchema::create(std::span(&fixture.record, 1), registryLimits.schema,
                                                   c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);
        const std::array routes{a::EventRegistration{a::EventType::NUAM_TIME_AND_SALE, 0}};
        a::EventRegistryWork work;
        const auto registry = a::EventRegistry::create(*schema, routes, registryLimits, work);
        REQUIRE(registry);
        options.eventRegistry = *registry;
    }

    const auto endpoint = runtime.createEndpoint(options);
    const auto feed = endpoint->getFeed();
    const auto subscription = history ? feed->createTimeSeriesSubscription<e::NuamTimeAndSale>()
                                      : feed->createSubscription<e::NuamTimeAndSale>();

    if (history)
        subscription->setFromTime(0);

    subscription->addSymbols(regional ? "AAPL&Q" : "AAPL");
    subscription->addEventListener([observation](std::span<const e::NuamTimeAndSale> batch) {
        {
            const std::lock_guard lock(observation->mutex);
            observation->received.insert(observation->received.end(), batch.begin(), batch.end());
        }

        observation->changed.notify_all();
    });
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    {
        std::unique_lock lock(observation->mutex);
        const auto expected = history ? std::size_t{3} : fixture.rows.size();
        const bool delivered = observation->changed.wait_for(lock, 5s, [&] {
            return observation->received.size() >= expected;
        });
        INFO("endpoint=", static_cast<int>(endpoint->getState()), " stage=", endpoint->getDiagnostic().stage,
             " subscription=", static_cast<int>(subscription->getState()),
             " stage=", subscription->getDiagnostic().stage, " records=", observation->received.size());
        REQUIRE(delivered);
    }

    if (history) {
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        auto snapshot = subscription->getHistorySnapshot();
        const auto complete = [&] {
            return snapshot.state == a::HistoryObservationState::CURRENT && snapshot.ranges.size() == 1 &&
                   snapshot.ranges.front().status.completeDeliveredRange;
        };

        while (!complete() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
            snapshot = subscription->getHistorySnapshot();
        }

        REQUIRE(complete());
        CHECK(snapshot.ranges.front().status.requestedFrom == e::withTimeSequenceTime(0, 0));
    }

    const auto closed = endpoint->closeAndAwaitTermination();
    REQUIRE_FALSE(closed.deadlineExpired);
    CHECK(endpoint->getStatistics().final);
    CHECK(subscription->getStatistics().final);
    CHECK(closed.drained);
    const auto status = peer.snapshot();
    REQUIRE(status.sessions.front().matched);
    checkAdd(status.sessions.front().received, history, regional, custom);

    if (!history) {
        REQUIRE(observation->received.size() == fixture.rows.size());

        for (std::size_t index = 0; index < observation->received.size(); ++index) {
            checkEvent(observation->received[index], fixture.rows[index]);
            CHECK(observation->received[index].getEventFlags() == std::stoi(fixture.rows[index][4]));
        }

        return;
    }

    REQUIRE(observation->received.size() >= 3);
    checkEvent(observation->received.front(), fixture.rows.front());
    CHECK(observation->received.front().getEventFlags() == e::indexed_event_flags::SNAPSHOT_BEGIN);
    checkEvent(observation->received[1], fixture.rows[1]);
    // HISTORY may close TX_PENDING during initial projection once the source transaction is complete.
    CHECK((observation->received[1].getEventFlags() & ~e::indexed_event_flags::TX_PENDING) == 0);
    checkEvent(observation->received.back(), fixture.rows.back());
    CHECK(observation->received.back().getEventFlags() ==
          (e::indexed_event_flags::REMOVE_EVENT | e::indexed_event_flags::SNAPSHOT_END));
}
} // namespace

TEST_CASE("Public NuamTimeAndSale matches pinned QD STREAM and HISTORY") {
    const std::filesystem::path root(ORG_TTLDTOR_QDCXX_NUAM_TIME_AND_SALE_FIXTURES);
    SUBCASE("STREAM composite default registry") {
        verify(root, false, false, false);
    }
    SUBCASE("STREAM regional explicit registry") {
        verify(root, false, true, true);
    }
    SUBCASE("HISTORY composite default registry") {
        verify(root, true, false, false);
    }
    SUBCASE("HISTORY regional explicit registry") {
        verify(root, true, true, true);
    }
}

TEST_CASE("Nuam sale filters retain the default source identity") {
    const auto registry = d::defaultReceivingRegistry(a::DeliveryContract::HISTORY);
    REQUIRE(registry);
    const auto zero = a::SubscriptionSelector::indexed(u"AAPL", e::IndexedEventSource(0, std::nullopt));
    const auto other = a::SubscriptionSelector::indexed(u"AAPL", e::IndexedEventSource(1, std::nullopt));
    CHECK_FALSE(d::filterSubscriptionCoordinates(*registry, a::DeliveryContract::HISTORY,
                                                 a::EventType::NUAM_TIME_AND_SALE, zero, {})
                    .empty());
    CHECK(d::filterSubscriptionCoordinates(*registry, a::DeliveryContract::HISTORY, a::EventType::NUAM_TIME_AND_SALE,
                                           other, {})
              .empty());
}
