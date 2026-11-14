// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "connect_peer.hpp"
#include "qd/api/publisher.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/protocol/describe_records.hpp"
#include "qd/protocol/incoming_subscription.hpp"
#include "qd/protocol/stream_data.hpp"
#include "qd/protocol/subscription_writer.hpp"
#include "qd/transport/tcp_runtime.hpp"
#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"

#include <algorithm>
#include <array>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>
#include <bit>
#include <chrono>
#include <doctest/doctest.h>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace p = q::protocol;
namespace c = q::core;
namespace r = q::test::recovery;
using Clock = std::chrono::steady_clock;
using asio::ip::tcp;
using namespace std::chrono_literals;

constexpr c::SchemaLimits SCHEMA_LIMITS{8, 32, 64, 64, 1024};
constexpr auto MESSAGE_LIMIT = std::size_t{65536};
constexpr auto REMOVAL_TIME = std::bit_cast<std::int64_t>(std::uint64_t{0x81234567fedcba98});
constexpr std::int32_t REMOVE_EVENT = 2;
constexpr auto WAIT_TIME = 5s;

c::LocalSchema testSchema() {
    std::vector<c::LocalRecord> records;

    for (std::int32_t id = 0; id < 3; ++id) {
        c::LocalRecord record{id, id == 0 ? u"Alpha" : id == 1 ? u"Beta" : u"Indexed", id == 2, {}, {}};

        if (id == 2) {
            record.fields.push_back({std::to_underlying(c::SerialFieldType::LONG)});
            record.lookups.push_back({u"Index", 0});
            record.indexedTimeLayout = c::IndexedTimeSingleInt64{0};
        }

        record.fields.push_back({std::to_underlying(c::SerialFieldType::COMPACT_INT)});
        record.lookups.push_back({u"Value", record.fields.size() - 1});
        records.push_back(std::move(record));
    }

    auto schema = c::LocalSchema::create(records, SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);

    return std::move(*schema);
}

std::vector<std::vector<std::u16string>> fieldNames(const c::LocalSchema &schema) {
    std::vector<std::vector<std::u16string>> result;

    for (const auto &record : schema.records()) {
        auto &names = result.emplace_back();

        for (const auto &field : record.lookups)
            names.push_back(field.name);
    }

    return result;
}

std::vector<std::byte> hello(bool source) {
    std::vector<std::byte> body{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    r::number(body, 1);
    r::text(body, u"opt");
    r::text(body, u"hs");

    constexpr std::array<std::int32_t, 3> data{10, 15, 20};
    constexpr std::array<std::u16string_view, 3> dataNames{u"TICKER_DATA", u"STREAM_DATA", u"HISTORY_DATA"};
    constexpr std::array<std::int32_t, 6> requests{11, 12, 16, 17, 21, 22};
    constexpr std::array<std::u16string_view, 6> requestNames{
        u"TICKER_ADD_SUBSCRIPTION",    u"TICKER_REMOVE_SUBSCRIPTION", u"STREAM_ADD_SUBSCRIPTION",
        u"STREAM_REMOVE_SUBSCRIPTION", u"HISTORY_ADD_SUBSCRIPTION",   u"HISTORY_REMOVE_SUBSCRIPTION"};
    const auto append = [&](auto ids, auto names) {
        r::number(body, ids.size());

        for (std::size_t i = 0; i < ids.size(); ++i) {
            r::number(body, ids[i]);
            r::text(body, names[i]);
            r::number(body, 0);
        }
    };

    if (source) {
        append(data, dataNames);
        append(requests, requestNames);
    } else {
        append(requests, requestNames);
        append(data, dataNames);
    }

    return r::frame(1, body);
}

std::vector<std::byte> sourcePrefix(const c::LocalSchema &schema) {
    auto result = hello(true);
    auto fixture = q::connect_tests::fixturePeerPrefix(schema, c::SubscriptionFamily::TICKER, true);
    p::FrameReader reader({MESSAGE_LIMIT, MESSAGE_LIMIT + 9});
    const auto descriptor = reader.feed(fixture);
    REQUIRE(descriptor.frame);
    REQUIRE(descriptor.frame->type == 1);
    r::append(result, std::span(fixture).subspan(descriptor.accepted));

    return result;
}

p::SubscriptionWriter writer(const c::LocalSchema &schema, int senderBase) {
    std::vector<p::OutgoingRecord> layouts;

    for (const auto &record : schema.records()) {
        p::OutgoingRecord layout{senderBase + record.id, record.id, record.name, {}};

        for (const auto &lookup : record.lookups)
            layout.fields.push_back(
                {lookup.name, record.fields[lookup.fieldId].serialTypeId, static_cast<std::int32_t>(lookup.fieldId)});

        layouts.push_back(std::move(layout));
    }

    p::SubscriptionWriterWork work{256, 1024 * 1024, 1024 * 1024, 1024 * 1024};
    auto result =
        p::SubscriptionWriter::create(schema, layouts, {SCHEMA_LIMITS, 8, 32, 64, 1024},
                                      {MESSAGE_LIMIT, MESSAGE_LIMIT + 9, 32, 256, {8192, 1024}, 131072, 131072, 8},
                                      work, c::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1);
    REQUIRE(result);

    return std::move(*result);
}

class Downstream {
    asio::io_context io_;
    tcp::socket socket_{io_};
    const c::LocalSchema &schema_;
    p::SubscriptionWriter writer_;
    p::FrameReader frames_{{MESSAGE_LIMIT, MESSAGE_LIMIT + 9}};
    std::optional<p::StreamDataContext> context_;
    std::vector<std::pair<int, p::StreamDataRecord>> observed_;

    void send(std::span<const std::byte> bytes) {
        asio::error_code error;
        const auto sent = asio::write(socket_, asio::buffer(bytes.data(), bytes.size()), error);
        REQUIRE_FALSE(error);
        REQUIRE(sent == bytes.size());
    }

    void consume(const p::Frame &frame) {
        if (frame.type == 2) {
            auto parsed = p::parseDescribeRecords(frame.payload, {MESSAGE_LIMIT, 8, 16, 32, {8192, 1024}, 1024});
            REQUIRE(parsed.status == p::DescriptionStatus::OK);
            auto context =
                p::StreamDataContext::create(schema_, parsed.descriptions, {SCHEMA_LIMITS, 8, {64, 1024, 8192}});
            REQUIRE(context);
            context_ = std::move(*context);
        } else if (frame.type == 10 || frame.type == 15 || frame.type == 20) {
            REQUIRE(context_);
            const p::StreamDataLimits limits{MESSAGE_LIMIT, 16,   64,   16,   64,   1024, {8192, 256, 256},
                                             {8192, 1024},  8192, 8192, 8192, 8192, 8192, 8192,
                                             8192,          8192};
            auto decoded = p::decodeStreamDataWithEventPolicy(frame.payload, *context_, limits,
                                                              p::DataEventPolicy::BINARY_CATALOG_V1);
            REQUIRE(decoded.status == p::StreamDataStatus::OK);

            for (auto &record : decoded.records)
                observed_.emplace_back(*frame.type, std::move(record));
        }
    }

    public:
    Downstream(const c::LocalSchema &schema, std::uint16_t port, int senderBase)
        : schema_(schema), writer_(writer(schema, senderBase)) {
        asio::error_code error;
        socket_.connect({asio::ip::address_v4::loopback(), port}, error);
        REQUIRE_FALSE(error);
        send(hello(false));
        const std::array ids{senderBase, senderBase + 1, senderBase + 2};
        p::SubscriptionWriterWork work{256, 1024 * 1024, 1024 * 1024, 1024 * 1024};
        auto description = writer_.describe(ids, work);
        REQUIRE(description);
        send(description->bytes());
        socket_.non_blocking(true, error);
        REQUIRE_FALSE(error);
    }

    void subscribe(p::SubscriptionMessage message, std::int32_t id, std::optional<std::int64_t> bound = {},
                   std::u16string_view symbol = u"IBM") {
        const c::SubscriptionRequest request{{schema_.recordContext(id), symbol}, bound};
        p::SubscriptionWriterWork work{256, 1024 * 1024, 1024 * 1024, 1024 * 1024};
        auto output = writer_.compose(message, std::span(&request, 1), work);
        REQUIRE(output);
        send(output->bytes());
    }

    void poll() {
        std::array<std::byte, 8192> input{};
        asio::error_code error;
        const auto size = socket_.read_some(asio::buffer(input.data(), input.size()), error);

        if (error == asio::error::would_block || error == asio::error::try_again)
            return;

        REQUIRE_FALSE(error);
        std::span pending(input.data(), size);

        while (!pending.empty()) {
            auto result = frames_.feed(pending);
            REQUIRE(result.status != p::FrameStatus::FAILED);
            REQUIRE(result.accepted > 0);
            pending = pending.subspan(result.accepted);

            if (result.frame)
                consume(*result.frame);
        }
    }

    const auto &records() const noexcept {
        return observed_;
    }
};

std::vector<std::pair<int, p::IncomingSubscriptionEntry>> sourceDemand(std::span<const std::byte> bytes,
                                                                       const c::LocalSchema &schema) {
    p::FrameReader frames({MESSAGE_LIMIT, MESSAGE_LIMIT + 9});
    const std::array mapping{p::IncomingSubscriptionRecord{0, 0}, p::IncomingSubscriptionRecord{1, 1},
                             p::IncomingSubscriptionRecord{2, 2}};
    p::IncomingSubscriptionWork work{64, 1048576, 1048576, 1048576};
    std::vector<std::pair<int, p::IncomingSubscriptionEntry>> result;

    while (!bytes.empty()) {
        auto frame = frames.feed(bytes);
        REQUIRE(frame.status != p::FrameStatus::FAILED);
        REQUIRE(frame.accepted > 0);
        bytes = bytes.subspan(frame.accepted);

        if (frame.frame && (frame.frame->type == 11 || frame.frame->type == 16 || frame.frame->type == 21)) {
            auto batch = p::decodeIncomingSubscription(*frame.frame, schema, mapping,
                                                       {MESSAGE_LIMIT, 8, 8, 32, 256, 8192}, work);
            REQUIRE(batch);

            for (auto &entry : batch->entries) {
                if (entry.symbol == u"IBM")
                    result.emplace_back(*frame.frame->type, std::move(entry));
            }
        }
    }

    return result;
}

bool hasDemand(const std::vector<std::pair<int, p::IncomingSubscriptionEntry>> &entries, int message,
               const c::RecordContext &record) {
    return std::ranges::any_of(entries, [&](const auto &entry) {
        return entry.first == message && entry.second.context.matches(record) && entry.second.symbol == u"IBM";
    });
}
} // namespace

TEST_CASE("full catalog relay preserves record identities and demand over TCP") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = testSchema();
    auto names = fieldNames(schema);
    q::test::TcpPeerConfig config;
    config.lifetime = 30s;
    auto &sourceScript = config.sessions.emplace_back();
    sourceScript.initialBytes = sourcePrefix(schema);
    sourceScript.readThroughMessage = 21;
    sourceScript.waitForControllerReply = true;
    sourceScript.waitForClientClose = true;

    q::connect_tests::Record alpha{u"IBM", {{c::SerialFieldType::COMPACT_INT, 101}}, 0, {}, 0};
    q::connect_tests::Record beta{u"IBM", {{c::SerialFieldType::COMPACT_INT, 202}}, 0, {}, 1};
    namespace flags = q::events::indexed_event_flags;
    q::connect_tests::Record indexed{
        u"IBM",
        {{c::SerialFieldType::LONG, REMOVAL_TIME + 1}, {c::SerialFieldType::COMPACT_INT, 303}},
        flags::SNAPSHOT_BEGIN | flags::TX_PENDING,
        {},
        2};
    q::connect_tests::Record removed{u"IBM", {}, flags::SNAPSHOT_END | REMOVE_EVENT, REMOVAL_TIME, 2};
    const std::array ticker{alpha, beta};
    sourceScript.replyBytes = q::connect_tests::fixturePeerData(ticker, c::SubscriptionFamily::TICKER, true);
    r::append(sourceScript.replyBytes,
              q::connect_tests::fixturePeerData(std::span(&beta, 1), c::SubscriptionFamily::STREAM, true));
    r::append(sourceScript.replyBytes,
              q::connect_tests::fixturePeerData(std::span(&indexed, 1), c::SubscriptionFamily::HISTORY, true));
    r::append(sourceScript.replyBytes,
              q::connect_tests::fixturePeerData(std::span(&removed, 1), c::SubscriptionFamily::HISTORY, true));
    q::test::OwnedTcpPeer source(std::move(config));
    REQUIRE(source.port() != 0);

    q::api::PublisherOptions options;
    options.source = q::api::PublisherSourceOptions{"127.0.0.1", source.port(), 2s};
    options.catalog.emplace(schema, names);
    auto relay = q::api::PublisherEndpoint::create(*bootstrap, options);

    if (!relay) {
        INFO(relay.error().stage, " ", static_cast<int>(relay.error().code), " detail ", relay.error().detailCode);
        REQUIRE(relay);
    }

    Downstream first(schema, (*relay)->port(), 100);
    Downstream second(schema, (*relay)->port(), 200);
    first.subscribe(p::SubscriptionMessage::TICKER_ADD, 0);
    first.subscribe(p::SubscriptionMessage::TICKER_ADD, 1);
    first.subscribe(p::SubscriptionMessage::STREAM_ADD, 1);
    second.subscribe(p::SubscriptionMessage::TICKER_ADD, 0);
    second.subscribe(p::SubscriptionMessage::STREAM_ADD, 1);

    const auto demandDeadline = Clock::now() + WAIT_TIME;
    std::vector<std::pair<int, p::IncomingSubscriptionEntry>> upstream;
    q::api::PublisherSourceStatus firstDisconnect;

    do {
        const auto status = (*relay)->getSourceStatus();

        if (status.generation && !status.connected) {
            firstDisconnect = status;
            break;
        }

        upstream = sourceDemand(source.snapshot().sessions[0].received, schema);

        if (hasDemand(upstream, 11, schema.recordContext(0)) && hasDemand(upstream, 11, schema.recordContext(1)) &&
            hasDemand(upstream, 16, schema.recordContext(1)))
            break;

        std::this_thread::sleep_for(1ms);
    } while (Clock::now() < demandDeadline);

    const auto relayStatus = (*relay)->getSourceStatus();
    const auto relayDiagnostic = (*relay)->getDiagnostic();
    const auto sourceStatus = source.snapshot();
    INFO("first disconnect stage ", firstDisconnect.diagnostic.stage, " code ",
         static_cast<int>(firstDisconnect.diagnostic.code), " detail ", firstDisconnect.diagnostic.detailCode);
    INFO("source generation ", relayStatus.generation, " connected ", relayStatus.connected, " source stage ",
         relayStatus.diagnostic.stage, " code ", static_cast<int>(relayStatus.diagnostic.code), " detail ",
         relayStatus.diagnostic.detailCode, " relay stage ", relayDiagnostic.stage, " code ",
         static_cast<int>(relayDiagnostic.code), " detail ", relayDiagnostic.detailCode, " peer error ",
         static_cast<int>(sourceStatus.error), " captured bytes ", sourceStatus.sessions[0].received.size());
    REQUIRE(hasDemand(upstream, 11, schema.recordContext(0)));
    REQUIRE(hasDemand(upstream, 11, schema.recordContext(1)));
    REQUIRE(hasDemand(upstream, 16, schema.recordContext(1)));
    CHECK(upstream.size() == 3);

    first.subscribe(p::SubscriptionMessage::HISTORY_ADD, 2, REMOVAL_TIME);
    second.subscribe(p::SubscriptionMessage::HISTORY_ADD, 2, REMOVAL_TIME);
    // Each barrier follows that client's HISTORY command on the same TCP connection.
    first.subscribe(p::SubscriptionMessage::TICKER_ADD, 0, {}, u"READY1");
    second.subscribe(p::SubscriptionMessage::TICKER_ADD, 0, {}, u"READY2");
    REQUIRE((*relay)->awaitDemand(c::SubscriptionFamily::TICKER, u"READY1", true, Clock::now() + WAIT_TIME));
    REQUIRE((*relay)->awaitDemand(c::SubscriptionFamily::TICKER, u"READY2", true, Clock::now() + WAIT_TIME));
    REQUIRE((*relay)->awaitDemand(c::SubscriptionFamily::STREAM, schema.records()[1].name, u"IBM", true,
                                  Clock::now() + WAIT_TIME));
    REQUIRE((*relay)->awaitDemand(c::SubscriptionFamily::HISTORY, schema.records()[2].name, u"IBM", true,
                                  Clock::now() + WAIT_TIME));
    first.subscribe(p::SubscriptionMessage::TICKER_REMOVE, 0, {}, u"READY1");
    second.subscribe(p::SubscriptionMessage::TICKER_REMOVE, 0, {}, u"READY2");
    const auto historyDeadline = Clock::now() + WAIT_TIME;

    while (!source.snapshot().sessions[0].matched && Clock::now() < historyDeadline)
        std::this_thread::sleep_for(1ms);

    const auto capture = source.snapshot();
    REQUIRE(capture.sessions[0].matched);
    upstream = sourceDemand(capture.sessions[0].received, schema);
    REQUIRE(upstream.size() == 4);
    CHECK(hasDemand(upstream, 21, schema.recordContext(2)));
    CHECK(upstream.back().second.historyBound == REMOVAL_TIME);
    source.allowReply(0);

    const auto deliveryDeadline = Clock::now() + WAIT_TIME;

    do {
        first.poll();
        second.poll();

        if (first.records().size() >= 5 && second.records().size() >= 4)
            break;

        std::this_thread::sleep_for(1ms);
    } while (Clock::now() < deliveryDeadline);

    const auto check = [&](const Downstream &peer, bool includesBetaTicker) {
        const auto &rows = peer.records();
        const auto find = [&](int family, int id, std::int64_t value) {
            return std::ranges::count_if(rows, [&](const auto &item) {
                return item.first == family && item.second.localId == id && item.second.symbol.units == u"IBM" &&
                       !item.second.removalTime && !item.second.typedValues.empty() &&
                       item.second.typedValues.back().raw == value;
            });
        };
        CHECK(find(10, 0, 101) == 1);
        CHECK(find(10, 1, 202) == (includesBetaTicker ? 1 : 0));
        CHECK(find(15, 1, 202) == 1);
        CHECK(find(20, 2, 303) == 1);
        const auto start = std::ranges::find_if(rows, [](const auto &item) {
            return item.first == 20 && item.second.localId == 2 && !item.second.removalTime;
        });
        const auto end = std::ranges::find_if(rows, [](const auto &item) {
            return item.first == 20 && item.second.localId == 2 && item.second.removalTime == REMOVAL_TIME;
        });
        REQUIRE(start != rows.end());
        REQUIRE(end != rows.end());
        CHECK(start < end);
        CHECK((start->second.symbol.eventFlags & flags::SNAPSHOT_BEGIN) != 0);
        CHECK((end->second.symbol.eventFlags & (flags::SNAPSHOT_END | REMOVE_EVENT)) ==
              (flags::SNAPSHOT_END | REMOVE_EVENT));
        CHECK(std::ranges::count_if(rows, [](const auto &item) {
                  return item.first == 20 && item.second.localId == 2 && item.second.symbol.units == u"IBM" &&
                         item.second.removalTime == REMOVAL_TIME;
              }) == 1);
    };
    check(first, true);
    check(second, false);
    (*relay)->closeAndAwaitTermination();
    CHECK((*relay)->getDiagnostic().code == q::api::ErrorCode::NONE);
}

TEST_CASE("catalog relay policy replaces physical record demand for one live peer") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = testSchema();
    const auto names = fieldNames(schema);
    q::test::TcpPeerConfig config;
    config.lifetime = 20s;
    auto &script = config.sessions.emplace_back();
    script.initialBytes = sourcePrefix(schema);
    script.readThroughMessage = 16;
    script.waitForClientClose = true;
    q::test::OwnedTcpPeer source(std::move(config));
    q::api::EventRegistryWork work;
    const auto registry = q::api::EventRegistry::builtIn({}, work);
    REQUIRE(registry);
    const auto filter = [&](std::u16string expression) {
        return q::api::FeedFilter::Builder(*registry).withCategory(u"allow", std::move(expression)).build();
    };
    q::api::PublisherOptions options;
    options.source = q::api::PublisherSourceOptions{"127.0.0.1", source.port(), 2s};
    options.catalog.emplace(schema, names);
    options.demand.ticker = filter(u":Alpha");
    auto relay = q::api::PublisherEndpoint::create(*bootstrap, options);
    REQUIRE(relay);
    Downstream downstream(schema, (*relay)->port(), 100);
    using enum c::SubscriptionFamily;
    using enum p::SubscriptionMessage;
    downstream.subscribe(TICKER_ADD, 0);
    downstream.subscribe(TICKER_ADD, 1);
    downstream.subscribe(STREAM_ADD, 1);
    REQUIRE((*relay)->awaitDemand(TICKER, u"Alpha", u"IBM", true, Clock::now() + WAIT_TIME));
    REQUIRE((*relay)->awaitDemand(STREAM, u"Beta", u"IBM", true, Clock::now() + WAIT_TIME));
    CHECK((*relay)->getDemand(TICKER).size() == 1);
    REQUIRE((*relay)->getPeers().size() == 1);
    const auto id = (*relay)->getPeers().front().id;
    const auto changed = (*relay)->replacePeerDemand(id, {filter(u":Beta"), {}, {}});
    REQUIRE(changed);
    const auto applied = changed->waitUntil(Clock::now() + WAIT_TIME);
    REQUIRE(applied);
    CHECK(applied->outcome == q::api::PublisherPolicyOutcome::APPLIED);
    CHECK(applied->revision == 1);
    const auto demand = (*relay)->getDemand(TICKER);
    REQUIRE(demand.size() == 1);
    CHECK(demand.front().recordName == u"Beta");
    REQUIRE((*relay)->getDemand(STREAM).size() == 1);

    const auto deadline = Clock::now() + WAIT_TIME;
    bool observed = false;

    while (Clock::now() < deadline) {
        const auto captured = source.snapshot();
        const auto upstream = sourceDemand(captured.sessions[0].received, schema);

        if (hasDemand(upstream, 11, schema.recordContext(1))) {
            observed = true;
            break;
        }

        std::this_thread::sleep_for(1ms);
    }

    CHECK(observed);
}
