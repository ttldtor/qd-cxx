// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "connect_peer.hpp"
#include "qd/api/feed.hpp"
#include "qd/encoding/symbol_reader.hpp"
#include "qd/events/event_flags.hpp"
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
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace a = q::api;
namespace c = q::core;
namespace e = q::events;
namespace peer = q::connect_tests;
using namespace std::chrono_literals;

constexpr std::int32_t TICKER_CANDLE_RECORD_ID = 42;
constexpr std::int32_t HISTORY_CANDLE_RECORD_ID = 10;
constexpr std::int64_t RANGE_START_MS = 1000;
constexpr std::int64_t RANGE_START_PACKED = std::int64_t{1} << 32;
constexpr std::int64_t FIRST_CANDLE_INDEX = (std::int64_t{2} << 32) | 7;

class Received {
    std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<e::Candle> values_;

    public:
    void append(std::span<const e::Candle> events) {
        {
            const std::lock_guard lock(mutex_);
            values_.insert(values_.end(), events.begin(), events.end());
        }
        changed_.notify_all();
    }

    std::vector<e::Candle> waitFor(std::size_t size) {
        std::unique_lock lock(mutex_);
        static_cast<void>(changed_.wait_for(lock, 5s, [&] {
            return values_.size() >= size;
        }));

        return values_;
    }
};

peer::Record candleRecord(const c::LocalRecord &record, std::int32_t flags) {
    peer::Record result{u"AAPL{=m}", {}, flags, {}, 0, {}};

    for (const auto &field : record.fields)
        result.fields.push_back({static_cast<c::SerialFieldType>(field.serialTypeId), 0});

    result.fields[0].raw = 2;
    result.fields[1].raw = 7;

    return result;
}

void checkIdentity(const e::Candle &event, const e::CandleSymbol &symbol) {
    REQUIRE(event.getEventSymbol());
    CHECK(*event.getEventSymbol() == symbol);
    CHECK(event.getIndex() == FIRST_CANDLE_INDEX);
    CHECK(event.getTime() == 2000);
    CHECK(event.getSequence() == 7);
}

void verifyPublicFeed(bool history) {
    using namespace e::indexed_event_flags;
    const auto schema = e::defaultCandleSchema({1, 13, 13, 128, 1024});
    REQUIRE(schema);
    const auto symbol = e::CandleSymbol::valueOf(std::u16string_view(u"AAPL{=m}"));
    const auto first = std::make_shared<Received>();
    const auto replay = std::make_shared<Received>();
    a::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(runtimeOptions);
    a::EndpointOptions options;
    options.contract = history ? a::DeliveryContract::HISTORY : a::DeliveryContract::TICKER;
    options.maximumAttempts = 1;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    auto subscribe = [&](const std::shared_ptr<Received> &values) {
        auto subscription =
            history ? feed->createTimeSeriesSubscription<e::Candle>() : feed->createSubscription<e::Candle>();

        if (history)
            subscription->setFromTime(RANGE_START_MS);

        subscription->addEventListener([values](auto batch) {
            values->append(batch);
        });
        subscription->addSymbols(symbol);

        return subscription;
    };
    const auto subscription = subscribe(first);
    std::vector<peer::Record> data{candleRecord(schema->records()[0], history ? SNAPSHOT_BEGIN : 0)};
    peer::PeerScript script;
    script.family = history ? c::SubscriptionFamily::HISTORY : c::SubscriptionFamily::TICKER;
    script.subscriptionRecordId = history ? HISTORY_CANDLE_RECORD_ID : TICKER_CANDLE_RECORD_ID;
    script.canonicalSymbols = true;

    if (history) {
        script.historyBound = RANGE_START_PACKED;
        data.push_back({u"AAPL{=m}", {}, REMOVE_EVENT | SNAPSHOT_END, RANGE_START_PACKED, 0, {}});
    }

    const std::array<std::u16string, 1> expected{u"AAPL{=m}"};

    if (history) {
        const auto id = script.subscriptionRecordId;
        script.followupAdd = peer::ExpectedSubscription{{u"MSFT{=m}"}, {id}};
        auto correction = candleRecord(schema->records()[0], 0);
        correction.fields[2].raw = (std::int64_t{5} << 8) | 128;
        script.afterFollowupAdd = std::vector{std::move(correction)};
        script.removeSequence = std::vector{peer::ExpectedSubscription{{expected[0], u"MSFT{=m}"}, {id, id}, true}};
    } else {
        script.removeSequence = std::vector{peer::ExpectedSubscription{{expected[0]}, {script.subscriptionRecordId}}};
    }

    script.afterFinalRemove = std::vector<peer::Record>{};
    peer::Peer source(*schema, data, expected, 32, peer::PeerMode::HOLD, script);
    REQUIRE(source.port() != 0);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    const auto values = first->waitFor(data.size());
    INFO("endpoint stage=", endpoint->getDiagnostic().stage,
         " subscription stage=", subscription->getDiagnostic().stage);
    REQUIRE(values.size() == data.size());
    checkIdentity(values.front(), symbol);
    CHECK(values.front().eventFlags == (history ? SNAPSHOT_BEGIN : 0));

    if (history) {
        CHECK(values.back().getIndex() == RANGE_START_PACKED);
        CHECK(values.back().eventFlags == (REMOVE_EVENT | SNAPSHOT_END));
        const auto cutoff = std::chrono::steady_clock::now() + 3s;
        auto snapshot = subscription->getHistorySnapshot();
        const auto complete = [&] {
            return snapshot.state == a::HistoryObservationState::CURRENT && snapshot.ranges.size() == 1 &&
                   snapshot.ranges[0].status.completeDeliveredRange;
        };

        while (!complete() && std::chrono::steady_clock::now() < cutoff) {
            std::this_thread::sleep_for(5ms);
            snapshot = subscription->getHistorySnapshot();
        }

        REQUIRE(snapshot.state == a::HistoryObservationState::CURRENT);
        REQUIRE(snapshot.ranges.size() == 1);
        CHECK(snapshot.ranges[0].status.completeDeliveredRange);
        CHECK(snapshot.ranges[0].status.requestedFrom == RANGE_START_PACKED);
    }

    const auto cached = subscribe(replay);
    const auto replayed = replay->waitFor(data.size());
    REQUIRE(replayed.size() == data.size());
    checkIdentity(replayed.front(), symbol);

    if (history) {
        CHECK(replayed.front().eventFlags == SNAPSHOT_BEGIN);
        CHECK(replayed.back().getIndex() == RANGE_START_PACKED);
        CHECK(replayed.back().eventFlags == (REMOVE_EVENT | SNAPSHOT_END));
    }

    const auto finalCount = data.size() + static_cast<std::size_t>(history);

    if (history) {
        subscription->addSymbols("MSFT{=m}");

        for (const auto &values : {first->waitFor(finalCount), replay->waitFor(finalCount)}) {
            REQUIRE(values.size() == finalCount);
            checkIdentity(values.back(), symbol);
            CHECK(values.back().getCount() == 5);
            CHECK(values.back().eventFlags == 0);
        }
    }

    subscription->setSymbols({});
    cached->removeSymbols(symbol);
    const auto cutoff = std::chrono::steady_clock::now() + 3s;

    while (source.removes() == 0 && !source.failed() && std::chrono::steady_clock::now() < cutoff)
        std::this_thread::sleep_for(5ms);

    CHECK(source.removes() == 1);
    CHECK(source.adds() == (history ? 2 : 1));
    CHECK_FALSE(source.failed());
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK_FALSE(closed.deadlineExpired);
    CHECK(subscription->getStatistics().final);
    CHECK(cached->getStatistics().final);
    CHECK(subscription->getStatistics().deliveredEvents.value == finalCount);
    CHECK(cached->getStatistics().deliveredEvents.value == finalCount);
    runtime.closeAndAwaitTermination();
}
} // namespace

TEST_CASE("Candle public TICKER uses an exact physical ADD and replays its last value") {
    verifyPublicFeed(false);
}

TEST_CASE("Candle public HISTORY uses an exact time range and replays a complete snapshot") {
    verifyPublicFeed(true);
}

TEST_CASE("Candle public registration refuses the STREAM contract") {
    a::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(runtimeOptions);
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::STREAM;
    auto endpoint = runtime.createEndpoint(options);
    CHECK_THROWS_AS(static_cast<void>(endpoint->getFeed()->createSubscription<e::Candle>()), a::Error);
    CHECK(endpoint->closeAndAwaitTermination().drained);
    runtime.closeAndAwaitTermination();
}

TEST_CASE("Candle validates an indexed UTF_CHAR removal against the exact raw coordinate") {
    constexpr c::SchemaLimits limits{1, 13, 13, 128, 1024};
    const auto standard = e::defaultCandleSchema(limits);
    REQUIRE(standard);
    auto record = standard->records()[0];
    constexpr auto type = c::SerialFieldType::UTF_CHAR;
    record.fields[0].serialTypeId = std::to_underlying(type);
    record.fields[1].serialTypeId = std::to_underlying(type);
    const auto schema = c::LocalSchema::create(std::span(&record, 1), limits, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapping = e::PreparedCandle::prepare(*schema, 0, e::CandleRecordKind::CANDLE,
                                                    a::EventRegistration{a::EventType::CANDLE, 0}.mappingLimits);
    REQUIRE(mapping);
    std::vector<c::FieldValueView> fields;

    for (const auto &field : record.fields)
        fields.push_back({static_cast<c::SerialFieldType>(field.serialTypeId), 0, {}, {}});

    fields[0].raw = -1;
    fields[1].raw = -1;
    const auto symbol = e::CandleSymbol::valueOf(std::u16string_view(u"IBM{=m}"));
    const e::BorrowedRecordValueView input{schema->recordContext(0), fields};
    constexpr auto flags = e::indexed_event_flags::REMOVE_EVENT | e::indexed_event_flags::SNAPSHOT_END;
    CHECK_FALSE(mapping->mapView(symbol, input, 128, 0, flags));
    const auto event = mapping->mapIndexedRemovalView(symbol, input, 128, 0, flags, -1);
    REQUIRE(event);
    CHECK(event->getIndex() == -1);
    CHECK(event->eventFlags == flags);
    CHECK_FALSE(mapping->mapIndexedRemovalView(symbol, input, 128, 0, flags, -2));
    CHECK_FALSE(mapping->mapIndexedRemovalView(symbol, input, 128, 0, 0, -1));
}

TEST_CASE("Candle restores a fixed bid minute and distinguishes its TradeHistory tick") {
    constexpr c::SchemaLimits limits{2, 20, 20, 128, 1024};
    const auto defaults = e::defaultCandleSchema(limits);
    REQUIRE(defaults);
    using Type = c::SerialFieldType;
    const auto type = [](Type field) {
        return static_cast<std::int32_t>(field);
    };
    auto fixedRecord = defaults->records().front();
    fixedRecord.name = u"Candle{=m,price=bid}";
    const c::LocalRecord tradeRecord{
        1,
        u"TradeHistory",
        true,
        {{type(Type::TIME_SECONDS)},
         {type(Type::SEQUENCE)},
         {type(Type::UTF_CHAR)},
         {type(Type::WIDE_DECIMAL)},
         {type(Type::WIDE_DECIMAL)},
         {type(Type::WIDE_DECIMAL)},
         {type(Type::WIDE_DECIMAL)}},
        {{u"Time", 0}, {u"Sequence", 1}, {u"Exchange", 2}, {u"Price", 3}, {u"Size", 4}, {u"Bid", 5}, {u"Ask", 6}},
        c::IndexedTimePairInt32{0, 1}};
    const std::array records{std::move(fixedRecord), tradeRecord};
    const auto schema = c::LocalSchema::create(records, limits, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const std::array registrations{a::EventRegistration{a::EventType::CANDLE, 0},
                                   a::EventRegistration{a::EventType::CANDLE, 1}};
    a::EventRegistryWork work;
    const auto registry = a::EventRegistry::create(*schema, registrations, {}, work);
    REQUIRE(registry);

    const auto minute = e::CandleSymbol::valueOf(std::u16string_view(u"IBM&Q{=m,price=bid}"));
    const auto tick = e::CandleSymbol::valueOf(std::u16string_view(u"IBM&Q"));
    const auto received = std::make_shared<Received>();
    a::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(runtimeOptions);
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::TICKER;
    options.maximumAttempts = 1;
    options.eventRegistry = *registry;
    const auto endpoint = runtime.createEndpoint(options);
    const auto subscription = endpoint->getFeed()->createSubscription<e::Candle>();
    subscription->addEventListener([received](auto batch) {
        received->append(batch);
    });
    subscription->addSymbols({minute, tick});
    const auto beforeUnsupported = subscription->getSnapshot();
    CHECK_THROWS_AS(subscription->addSymbols("IBM&Q{=h}"), a::Error);
    const auto afterUnsupported = subscription->getSnapshot();
    CHECK(afterUnsupported.revision == beforeUnsupported.revision);
    CHECK(afterUnsupported.symbols == beforeUnsupported.symbols);

    const auto wideInteger = [](std::int64_t value) {
        return (value << 8) | 128;
    };
    auto fixed = candleRecord(records[0], 0);
    fixed.symbol = u"IBM&Q";
    fixed.fields[2].raw = wideInteger(11);
    fixed.fields[6].raw = wideInteger(101);
    peer::Record trade{u"IBM&Q",
                       {{Type::TIME_SECONDS, 3},
                        {Type::SEQUENCE, 8},
                        {Type::UTF_CHAR, u'Q'},
                        {Type::WIDE_DECIMAL, wideInteger(102)},
                        {Type::WIDE_DECIMAL, wideInteger(5)},
                        {Type::WIDE_DECIMAL, 0},
                        {Type::WIDE_DECIMAL, 0}},
                       0,
                       {},
                       1,
                       {}};
    const std::array data{std::move(fixed), std::move(trade)};
    const std::array<std::u16string, 2> physical{u"IBM&Q", u"IBM&Q"};
    peer::PeerScript script;
    script.family = c::SubscriptionFamily::TICKER;
    script.canonicalSymbols = true;
    script.describeAllRecords = true;
    script.unorderedAdds = true;
    script.subscriptionRecordIds = {0, 1};
    script.removeSequence = std::vector{peer::ExpectedSubscription{{u"IBM&Q", u"IBM&Q"}, {0, 1}, true}};
    script.afterFinalRemove = std::vector<peer::Record>{};
    peer::Peer source(*schema, data, physical, 32, peer::PeerMode::HOLD, script);
    REQUIRE(source.port() != 0);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    const auto values = received->waitFor(2);
    REQUIRE(values.size() == 2);

    unsigned minuteCount = 0;
    unsigned tickCount = 0;

    for (const auto &event : values) {
        const auto symbol = event.getEventSymbol();
        REQUIRE(symbol);

        if (*symbol == minute) {
            ++minuteCount;
            CHECK(event.getCount() == 11);
            CHECK(event.getClose() == 101);
            CHECK(event.getTime() == 2000);
        } else {
            REQUIRE(*symbol == tick);
            ++tickCount;
            CHECK(event.getCount() == 1);
            CHECK(event.getOpen() == 102);
            CHECK(event.getHigh() == 102);
            CHECK(event.getLow() == 102);
            CHECK(event.getClose() == 102);
            CHECK(event.getVolumeAsDouble() == 5);
            CHECK(event.getTime() == 3000);
            CHECK(event.getSequence() == 8);
        }
    }

    CHECK(minuteCount == 1);
    CHECK(tickCount == 1);
    subscription->setSymbols({});
    const auto cutoff = std::chrono::steady_clock::now() + 3s;

    while (source.removes() == 0 && !source.failed() && std::chrono::steady_clock::now() < cutoff)
        std::this_thread::sleep_for(5ms);

    CHECK(source.removes() == 1);
    CHECK(source.adds() == 1);
    CHECK_FALSE(source.failed());
    CHECK(endpoint->closeAndAwaitTermination().drained);
    runtime.closeAndAwaitTermination();
}

namespace {
std::vector<std::byte> candleFixtureBytes(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input);
    const std::vector<char> raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const auto view = std::as_bytes(std::span(raw));

    return {view.begin(), view.end()};
}

void checkCandleAdd(std::span<const std::byte> captured) {
    namespace p = q::protocol;
    namespace wire = q::encoding;
    p::FrameReader frames({1024 * 1024, 1024 * 1024 + 9});
    unsigned count = 0;

    while (!captured.empty()) {
        const auto parsed = frames.feed(captured);
        REQUIRE(parsed.status != p::FrameStatus::FAILED);
        REQUIRE(parsed.accepted != 0);
        captured = captured.subspan(parsed.accepted);

        if (!parsed.frame || parsed.frame->type != std::to_underlying(p::SubscriptionMessage::HISTORY_ADD))
            continue;

        ++count;
        auto payload = std::span(parsed.frame->payload);
        wire::SymbolReader symbols({8192, 128, 128}, 0);
        const auto symbol = symbols.read(payload);
        REQUIRE(symbol.status == wire::SymbolStatus::OK);
        REQUIRE(symbol.value);
        CHECK(symbol.value->units == u"AAPL{=m}");
        CHECK(symbol.value->eventFlags == 0);
        payload = payload.subspan(symbol.consumed);
        const auto record = wire::readCompactInt(payload);
        REQUIRE(record.status == wire::Status::OK);
        CHECK(record.value == HISTORY_CANDLE_RECORD_ID);
        payload = payload.subspan(record.consumed);
        const auto bound = wire::readCompactLong(payload);
        REQUIRE(bound.status == wire::Status::OK);
        CHECK(bound.value == RANGE_START_PACKED);
        CHECK(payload.subspan(bound.consumed).empty());
    }

    CHECK(count == 1);
}

struct CandleOracleRow {
    std::u16string symbol;
    std::int64_t index;
    std::int64_t time;
    std::int32_t sequence;
    std::int64_t count;
    std::int32_t flags;
    std::int64_t closeBits;
};

std::map<std::string, std::vector<CandleOracleRow>> candleOracle(const std::filesystem::path &path) {
    std::ifstream input(path);
    REQUIRE(input);
    std::map<std::string, std::vector<CandleOracleRow>> result;

    for (std::string line; std::getline(input, line);) {
        if (!line.starts_with("output\t"))
            continue;

        std::istringstream source(line);
        std::vector<std::string> fields;

        for (std::string cell; std::getline(source, cell, '\t');)
            fields.push_back(std::move(cell));

        REQUIRE(fields.size() == 13);
        result[fields[1]].push_back({std::u16string(fields[7].begin(), fields[7].end()), std::stoll(fields[8]),
                                     std::stoll(fields[9]), std::stoi(fields[10]), std::stoll(fields[11]),
                                     std::stoi(fields[6]), std::stoll(fields[12])});
    }

    return result;
}

void checkCandle(const e::Candle &actual, const CandleOracleRow &expected) {
    REQUIRE(actual.getEventSymbol());
    CHECK(actual.getEventSymbol()->toString() == expected.symbol);
    CHECK(actual.getIndex() == expected.index);
    CHECK(actual.getTime() == expected.time);
    CHECK(actual.getSequence() == expected.sequence);
    CHECK(actual.getCount() == expected.count);
    CHECK(actual.eventFlags == expected.flags);
    CHECK(std::bit_cast<std::int64_t>(actual.getClose()) == expected.closeBits);
}

void expectCandleCoverage(const std::shared_ptr<a::Subscription<e::Candle>> &subscription, bool complete) {
    const auto deadline = std::chrono::steady_clock::now() + 4s;

    do {
        const auto snapshot = subscription->getHistorySnapshot();

        if (snapshot.state == a::HistoryObservationState::CURRENT && snapshot.ranges.size() == 1) {
            const auto &range = snapshot.ranges.front().status;

            if (range.source && range.completeDeliveredRange == complete) {
                CHECK(range.requestedFrom == RANGE_START_PACKED);
                CHECK(range.source->snapshot == (complete ? q::collectors::HistorySnapshotState::COMPLETE
                                                          : q::collectors::HistorySnapshotState::RECEIVING));

                return;
            }
        }

        std::this_thread::sleep_for(5ms);
    } while (std::chrono::steady_clock::now() < deadline);

    FAIL("Candle HISTORY did not publish the expected qualified coverage");
}
} // namespace

TEST_CASE("Candle public HISTORY matches the pinned Java source and local replay") {
    const std::filesystem::path fixture(QDCXX_CANDLE_HISTORY_FIXTURES);
    const auto oracle = candleOracle(fixture / "oracle.tsv");
    q::test::TcpPeerScript script;
    script.initialBytes = q::test::recovery::ready();
    script.readThroughMessage = std::to_underlying(q::protocol::SubscriptionMessage::HISTORY_ADD);
    script.waitForClientClose = true;

    for (const auto *name : {"source-begin.qtp", "source-end.qtp", "source-correction.qtp"})
        script.stagedBytes.push_back(candleFixtureBytes(fixture / name));

    q::test::OwnedTcpPeer source({{std::move(script)}, 4 * 1024 * 1024, 30s});
    REQUIRE(source.port() != 0);
    a::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(runtimeOptions);
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    options.maximumAttempts = 1;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    const auto subscribe = [&](const std::shared_ptr<Received> &received) {
        auto subscription = feed->createTimeSeriesSubscription<e::Candle>();
        subscription->setFromTime(RANGE_START_MS);
        subscription->addEventListener([received](auto batch) {
            received->append(batch);
        });
        subscription->addSymbols("AAPL{=m}");

        return subscription;
    };
    auto first = std::make_shared<Received>();
    const auto initial = subscribe(first);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    auto wire = source.snapshot();

    while (!wire.sessions[0].matched && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
        wire = source.snapshot();
    }

    REQUIRE(wire.sessions[0].matched);
    checkCandleAdd(wire.sessions[0].received);
    source.allowStages(0, 1);
    auto firstValues = first->waitFor(1);
    REQUIRE(firstValues.size() == 1);
    checkCandle(firstValues[0], oracle.at("first-partial").at(0));
    expectCandleCoverage(initial, false);

    source.allowStages(0, 2);
    firstValues = first->waitFor(2);
    REQUIRE(firstValues.size() == 2);
    checkCandle(firstValues[1], oracle.at("first-completed").at(0));
    expectCandleCoverage(initial, true);
    auto second = std::make_shared<Received>();
    const auto replay = subscribe(second);
    auto secondValues = second->waitFor(2);
    REQUIRE(secondValues.size() == 2);

    for (std::size_t i = 0; i < secondValues.size(); ++i)
        checkCandle(secondValues[i], oracle.at("second-replay").at(i));

    source.allowStages(0, 3);
    firstValues = first->waitFor(3);
    secondValues = second->waitFor(3);
    REQUIRE(firstValues.size() == 3);
    REQUIRE(secondValues.size() == 3);
    checkCandle(firstValues[2], oracle.at("first-correction").at(0));
    checkCandle(secondValues[2], oracle.at("second-correction").at(0));
    expectCandleCoverage(initial, true);
    expectCandleCoverage(replay, true);
    auto third = std::make_shared<Received>();
    const auto replayCorrected = subscribe(third);
    const auto thirdValues = third->waitFor(2);
    REQUIRE(thirdValues.size() == 2);

    for (std::size_t i = 0; i < thirdValues.size(); ++i)
        checkCandle(thirdValues[i], oracle.at("third-replay").at(i));

    CHECK(endpoint->closeAndAwaitTermination().drained);
    runtime.closeAndAwaitTermination();
    const auto closed = std::chrono::steady_clock::now() + 3s;
    wire = source.snapshot();

    while (!wire.finished && std::chrono::steady_clock::now() < closed) {
        std::this_thread::sleep_for(5ms);
        wire = source.snapshot();
    }

    CHECK(wire.finished);
    CHECK(wire.error == q::test::TcpPeerError::NONE);
    checkCandleAdd(wire.sessions[0].received);
    CHECK(initial->getStatistics().deliveredEvents.value == 3);
    CHECK(replay->getStatistics().deliveredEvents.value == 3);
    CHECK(replayCorrected->getStatistics().deliveredEvents.value == 2);
}
