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
#include "qd/events/time_and_sale.hpp"
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
constexpr std::int32_t STREAM_COMPOSITE_ID = 9;
constexpr std::int32_t HISTORY_COMPOSITE_ID = 12;
constexpr auto REGIONAL_Q_OFFSET = 1 + u'Q' - u'A';

struct Fixture {
    c::LocalRecord record{0, u"TimeAndSale", true, {}, {}, c::IndexedTimePairInt32{0, 1}};
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
            REQUIRE(cells.size() == 31);
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
void checkEvent(const e::TimeAndSale &value, const std::vector<std::string> &row) {
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
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<e::TimeAndSale> received;
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
        const std::array routes{a::EventRegistration{a::EventType::TIME_AND_SALE, 0}};
        a::EventRegistryWork work;
        const auto registry = a::EventRegistry::create(*schema, routes, registryLimits, work);
        REQUIRE(registry);
        options.eventRegistry = *registry;
    }

    const auto endpoint = runtime.createEndpoint(options);
    const auto feed = endpoint->getFeed();
    const auto subscription =
        history ? feed->createTimeSeriesSubscription<e::TimeAndSale>() : feed->createSubscription<e::TimeAndSale>();

    if (history)
        subscription->setFromTime(0);

    subscription->addSymbols(regional ? "AAPL&Q" : "AAPL");
    subscription->addEventListener([&](std::span<const e::TimeAndSale> batch) {
        {
            const std::lock_guard lock(mutex);
            received.insert(received.end(), batch.begin(), batch.end());
        }

        changed.notify_all();
    });
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    {
        std::unique_lock lock(mutex);
        const auto expected = history ? std::size_t{3} : fixture.rows.size();
        const bool delivered = changed.wait_for(lock, 5s, [&] {
            return received.size() >= expected;
        });
        INFO("endpoint=", static_cast<int>(endpoint->getState()), " stage=", endpoint->getDiagnostic().stage,
             " subscription=", static_cast<int>(subscription->getState()),
             " stage=", subscription->getDiagnostic().stage, " records=", received.size());
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
        REQUIRE(received.size() == fixture.rows.size());

        for (std::size_t index = 0; index < received.size(); ++index) {
            checkEvent(received[index], fixture.rows[index]);
            CHECK(received[index].getEventFlags() == std::stoi(fixture.rows[index][4]));
        }

        return;
    }

    REQUIRE(received.size() >= 3);
    checkEvent(received.front(), fixture.rows.front());
    CHECK(received.front().getEventFlags() == e::indexed_event_flags::SNAPSHOT_BEGIN);
    checkEvent(received[1], fixture.rows[1]);
    // HISTORY may close TX_PENDING during initial projection once the source transaction is complete.
    CHECK((received[1].getEventFlags() & ~e::indexed_event_flags::TX_PENDING) == 0);
    checkEvent(received.back(), fixture.rows.back());
    CHECK(received.back().getEventFlags() ==
          (e::indexed_event_flags::REMOVE_EVENT | e::indexed_event_flags::SNAPSHOT_END));
}
} // namespace

TEST_CASE("Public TimeAndSale matches pinned QD STREAM and HISTORY reports") {
    const std::filesystem::path root(ORG_TTLDTOR_QDCXX_TIME_AND_SALE_FIXTURES);
    SUBCASE("STREAM composite wide with default registry") {
        verify(root / "wide", false, false, false);
    }
    SUBCASE("STREAM regional wide with explicit registry") {
        verify(root / "wide", false, true, true);
    }
    SUBCASE("STREAM regional wide with default registry") {
        verify(root / "wide", false, true, false);
    }
    SUBCASE("STREAM regional tiny with optional fields") {
        verify(root / "tiny-optional", false, true, true);
    }
    SUBCASE("HISTORY composite wide with default registry") {
        verify(root / "wide", true, false, false);
    }
    SUBCASE("HISTORY regional wide with explicit registry") {
        verify(root / "wide", true, true, true);
    }
    SUBCASE("HISTORY regional wide with default registry") {
        verify(root / "wide", true, true, false);
    }
    SUBCASE("HISTORY regional tiny with optional fields") {
        verify(root / "tiny-optional", true, true, true);
    }
}

TEST_CASE("TimeAndSale default regional catalogs fit complete expanded peer descriptions") {
    a::RuntimeOptions runtime;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});

    for (const auto contract : {a::DeliveryContract::STREAM, a::DeliveryContract::HISTORY}) {
        const bool history = contract == a::DeliveryContract::HISTORY;
        a::EndpointOptions options;
        options.contract = contract;
        const auto registry = d::defaultReceivingRegistry(contract);
        REQUIRE(registry);
        const auto profile = d::prepareReceivingProfile(runtime, options, "127.0.0.1", 1234);
        const auto &budgets = profile.endpoint.settings().budgets;
        const auto firstRecord = history ? HISTORY_COMPOSITE_ID : STREAM_COMPOSITE_ID;
        constexpr std::size_t REGIONAL_AND_COMPOSITE_RECORDS = 27;
        const auto salesEnd = static_cast<std::size_t>(firstRecord) + REGIONAL_AND_COMPOSITE_RECORDS;
        const auto expectedRecords = salesEnd + (history ? 5 : 2);
        REQUIRE(profile.schema.records().size() == expectedRecords);
        REQUIRE(profile.blueprint.size() == expectedRecords);
        REQUIRE(registry->registrations().size() == expectedRecords + (history ? 1 : 0));
        CHECK(profile.eventPlans.size() == (history ? 16384 : 4096));

        for (std::size_t offset = 0; offset < REGIONAL_AND_COMPOSITE_RECORDS; ++offset) {
            const auto recordIndex = static_cast<std::size_t>(firstRecord) + offset;
            const auto &record = profile.schema.records()[recordIndex];
            const auto &route = registry->registrations()[recordIndex + (history ? 1 : 0)];
            const auto name =
                offset ? u"TimeAndSale&" + std::u16string(1, static_cast<char16_t>(u'A' + offset - 1)) : u"TimeAndSale";
            CHECK(record.name == name);
            CHECK(record.fields.size() == 10);
            CHECK(record.lookups.size() == 14);
            CHECK(record.hasTime);
            REQUIRE(record.indexedTimeLayout);
            CHECK(std::holds_alternative<c::IndexedTimePairInt32>(*record.indexedTimeLayout));
            CHECK(route.type == a::EventType::TIME_AND_SALE);
            CHECK(route.localRecordId == static_cast<std::int32_t>(recordIndex));
        }

        auto writerWork = budgets.writerPreparation;
        auto writer = p::SubscriptionWriter::create(profile.schema, profile.blueprint, budgets.layout, budgets.writer,
                                                    writerWork, profile.recoveryOptions.selectors);
        REQUIRE(writer);
        auto schema = profile.schema;
        auto work = budgets.protocolTurn;
        auto live = p::LiveSession::create(std::move(schema), std::move(*writer), budgets.session, work);
        REQUIRE(live);
        REQUIRE(live->feed(r::ready(), work).status == p::LiveStatus::EVENT);

        using Type = c::SerialFieldType;
        constexpr std::array<std::pair<std::u16string_view, Type>, 2> FIELD_TIME{
            {{u"EventTime", Type::TIME_SECONDS}, {u"EventSequence", Type::SEQUENCE}}};
        constexpr std::array<std::pair<std::u16string_view, Type>, 3> OPTIONAL_FIELDS{
            {{u"TimeNanoPart", Type::COMPACT_INT},
             {u"Buyer", Type::UTF_CHAR_ARRAY},
             {u"Seller", Type::UTF_CHAR_ARRAY}}};
        constexpr std::int32_t DESCRIBE_RECORDS = 2;
        constexpr std::size_t DESCRIPTIONS_PER_FRAME = 20;
        p::LiveSnapshot snapshot;

        for (std::size_t first = 0; first < expectedRecords; first += DESCRIPTIONS_PER_FRAME) {
            std::vector<std::byte> body;
            const auto end = std::min(expectedRecords, first + DESCRIPTIONS_PER_FRAME);

            for (auto index = first; index < end; ++index) {
                const auto &record = profile.blueprint[index];
                const bool sale = index >= static_cast<std::size_t>(firstRecord) && index < salesEnd;
                const bool optionSale = history && index == salesEnd;
                const auto optional = std::span(OPTIONAL_FIELDS)
                                          .first(sale         ? OPTIONAL_FIELDS.size()
                                                 : optionSale ? 1
                                                              : 0);
                const bool hasFieldTime = sale || optionSale;
                r::number(body, record.senderId);
                r::text(body, record.wireName);
                r::number(body, static_cast<std::int64_t>(record.fields.size() + optional.size() +
                                                          (hasFieldTime ? FIELD_TIME.size() : 0)));

                if (hasFieldTime) {
                    for (const auto &[name, type] : FIELD_TIME) {
                        r::text(body, name);
                        r::number(body, std::to_underlying(type));
                    }
                }

                for (const auto &field : record.fields) {
                    r::text(body, field.wireName);
                    r::number(body, field.serialTypeId);
                }

                for (const auto &[name, type] : optional) {
                    r::text(body, name);
                    r::number(body, std::to_underlying(type));
                }
            }

            const auto frame = r::frame(DESCRIBE_RECORDS, body);
            const auto result = live->feed(frame, work);
            INFO("stage=", live->diagnostic().stage, " detail=", live->diagnostic().detail,
                 " error=", static_cast<int>(live->diagnostic().error), " first=", first);
            REQUIRE(result.status == p::LiveStatus::EVENT);
            REQUIRE(result.accepted == frame.size());
            REQUIRE(result.event);
            snapshot = result.event->snapshot();
            CHECK(snapshot.descriptions().size() == end);
        }

        REQUIRE(snapshot.descriptions().size() == expectedRecords);
        CHECK(snapshot.descriptions()[salesEnd - 1].name == u"TimeAndSale&Z");
        CHECK(snapshot.descriptions()[salesEnd - 1].fields.size() == 15);

        if (history) {
            CHECK(snapshot.descriptions()[salesEnd].name == u"OptionSale");
            CHECK(snapshot.descriptions()[salesEnd].fields.size() == 17);
        }

        const auto regional = d::filterSubscriptionCoordinates(*registry, contract, a::EventType::TIME_AND_SALE,
                                                               a::SubscriptionSelector::plain(u"AAPL&Q"), {});
        REQUIRE(regional.size() == 1);
        CHECK(regional.front().record == u"TimeAndSale&Q");
        CHECK(regional.front().symbol == u"AAPL");
        const auto wildcard = d::filterSubscriptionCoordinates(*registry, contract, a::EventType::TIME_AND_SALE,
                                                               a::SubscriptionSelector::all(), {});
        CHECK(wildcard.size() == (history ? 0 : REGIONAL_AND_COMPOSITE_RECORDS));
    }
}
