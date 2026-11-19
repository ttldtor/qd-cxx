// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "../../src/api/publication_queue.hpp"
#include "connect_peer.hpp"
#include "qd/api/publisher.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/protocol/describe_records.hpp"
#include "qd/protocol/incoming_subscription.hpp"
#include "qd/protocol/live_session.hpp"
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
#include <atomic>
#include <bit>
#include <chrono>
#include <doctest/doctest.h>
#include <limits>
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

p::SubscriptionWriter writer(const c::LocalSchema &schema, int senderBase, c::SchemaLimits limits = SCHEMA_LIMITS) {
    std::vector<p::OutgoingRecord> layouts;

    for (const auto &record : schema.records()) {
        p::OutgoingRecord layout{senderBase + record.id, record.id, record.name, {}};

        for (const auto &lookup : record.lookups)
            layout.fields.push_back(
                {lookup.name, record.fields[lookup.fieldId].serialTypeId, static_cast<std::int32_t>(lookup.fieldId)});

        layouts.push_back(std::move(layout));
    }

    p::SubscriptionWriterWork work{256, 64 * 1024 * 1024, 64 * 1024 * 1024, 1024 * 1024};
    auto result = p::SubscriptionWriter::create(
        schema, layouts, {limits, limits.records, limits.fields, limits.lookups, limits.totalNameUnits},
        {MESSAGE_LIMIT, MESSAGE_LIMIT + 9, 32, 256, {8192, 1024}, 131072, 131072, 8}, work,
        c::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1);
    REQUIRE(result);

    return std::move(*result);
}

class Downstream {
    asio::io_context io_;
    tcp::socket socket_{io_};
    const c::LocalSchema &schema_;
    const c::SchemaLimits schemaLimits_;
    p::SubscriptionWriter writer_;
    p::FrameReader frames_{{MESSAGE_LIMIT, MESSAGE_LIMIT + 9}};
    std::optional<p::StreamDataContext> context_;
    std::vector<std::pair<int, p::StreamDataRecord>> observed_;
    std::vector<std::pair<std::size_t, std::size_t>> streamFrames_;

    void send(std::span<const std::byte> bytes) {
        asio::error_code error;
        const auto sent = asio::write(socket_, asio::buffer(bytes.data(), bytes.size()), error);
        REQUIRE_FALSE(error);
        REQUIRE(sent == bytes.size());
    }

    void consume(const p::Frame &frame) {
        if (frame.type == 2) {
            auto parsed = p::parseDescribeRecords(frame.payload, {MESSAGE_LIMIT,
                                                                  schemaLimits_.records,
                                                                  schemaLimits_.fields,
                                                                  schemaLimits_.fields,
                                                                  {8192, 1024},
                                                                  schemaLimits_.totalNameUnits});
            REQUIRE(parsed.status == p::DescriptionStatus::OK);
            auto context = p::StreamDataContext::create(
                schema_, parsed.descriptions,
                {schemaLimits_,
                 schemaLimits_.records,
                 {schemaLimits_.fields, schemaLimits_.nameUnits, schemaLimits_.totalNameUnits}});
            REQUIRE(context);
            context_ = std::move(*context);
        } else if (frame.type == 10 || frame.type == 15 || frame.type == 20) {
            REQUIRE(context_);

            if (frame.type == 15)
                streamFrames_.emplace_back(frame.payload.size(), frame.endOffset - frame.startOffset);

            const p::StreamDataLimits limits{
                MESSAGE_LIMIT, 1024,         8192, 1024, 8192,          4096,          {MESSAGE_LIMIT, 256, 256},
                {8192, 1024},  8192,         8192, 8192, MESSAGE_LIMIT, MESSAGE_LIMIT, MESSAGE_LIMIT,
                MESSAGE_LIMIT, MESSAGE_LIMIT};
            auto decoded = p::decodeStreamDataWithEventPolicy(frame.payload, *context_, limits,
                                                              p::DataEventPolicy::BINARY_CATALOG_V1);
            INFO("frame payload ", frame.payload.size(), " stage ", static_cast<int>(decoded.stage), " payload bytes ",
                 decoded.payloadEncodedBytes, " decoded records ", decoded.records.size());
            REQUIRE(decoded.status == p::StreamDataStatus::OK);

            for (auto &record : decoded.records)
                observed_.emplace_back(*frame.type, std::move(record));
        }
    }

    public:
    Downstream(const c::LocalSchema &schema, std::uint16_t port, int senderBase, c::SchemaLimits limits = SCHEMA_LIMITS)
        : schema_(schema), schemaLimits_(limits), writer_(writer(schema, senderBase, limits)) {
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

    const auto &streamFrames() const noexcept {
        return streamFrames_;
    }

    bool receivingData() const noexcept {
        return context_.has_value() && frames_.pending().size() > 32;
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

namespace {
/** Build a catalog using the standard Quote layout and a custom record with nullable fields. */
c::LocalSchema publicationSchema() {
    const auto defaults = q::events::defaultQuoteSchema(SCHEMA_LIMITS);
    REQUIRE(defaults);

    auto quote = defaults->records().front();
    std::erase_if(quote.lookups, [](const auto &entry) {
        const auto name = q::events::QUOTE_FIELD_NAMES[entry.fieldId];

        return !std::equal(entry.name.begin(), entry.name.end(), name.begin(), name.end());
    });
    auto indexed = testSchema();
    using Type = c::SerialFieldType;
    const std::array records{std::move(quote),
                             c::LocalRecord{1,
                                            u"CustomFlow",
                                            false,
                                            {{std::to_underlying(Type::UTF_CHAR_ARRAY)},
                                             {std::to_underlying(Type::BYTE_ARRAY)},
                                             {std::to_underlying(Type::LONG)}},
                                            {{u"Text", 0}, {u"Payload", 1}, {u"Sequence", 2}}},
                             indexed.records()[2]};
    auto schema = c::LocalSchema::create(records, SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);

    return std::move(*schema);
}

/** Read until the expected detached records have arrived or a finite deadline elapses. */
void awaitRecords(Downstream &peer, std::size_t count, const q::api::PublisherEndpoint &endpoint) {
    const auto deadline = Clock::now() + WAIT_TIME;

    while (peer.records().size() < count && Clock::now() < deadline) {
        peer.poll();
        std::this_thread::sleep_for(1ms);
    }

    const auto stats = endpoint.getStatistics();
    INFO("publisher stage=", endpoint.getDiagnostic().stage, " active=", stats.activePeers,
         " retired=", stats.retiredPeers.value, " ticker in=", stats.families[0].ingressRecords.value,
         " ticker out=", stats.families[0].sentFrames.value, " stream in=", stats.families[1].ingressRecords.value,
         " stream out=", stats.families[1].sentFrames.value, " history in=", stats.families[2].ingressRecords.value,
         " history out=", stats.families[2].sentFrames.value);
    REQUIRE(peer.records().size() == count);
}
} // namespace

TEST_CASE("local catalog STREAM batches deliver ordered owned records to two short-write peers") {
    using enum c::SubscriptionFamily;
    using enum p::SubscriptionMessage;
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto schema = testSchema();
    const auto names = fieldNames(schema);
    q::api::PublisherOptions options;
    options.catalog.emplace(schema, names);
    options.streamBatchRecords = 8;
    options.listener.maxConnections = 2;
    options.listener.maxWriteBytes = 7;
    bool byteMode = false;
    SUBCASE("legacy independent records") {
    }
    SUBCASE("private flat byte-mode batches") {
        options.streamBytes.emplace(8000, MESSAGE_LIMIT);
        byteMode = true;
    }
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, options);
    REQUIRE(endpoint);
    Downstream first(schema, (*endpoint)->port(), 100);
    Downstream second(schema, (*endpoint)->port(), 200);
    const auto deadline = Clock::now() + WAIT_TIME;

    for (auto *peer : {&first, &second}) {
        peer->subscribe(STREAM_ADD, 0);
        peer->subscribe(STREAM_ADD, 1, {}, u"MSFT");
        peer->subscribe(STREAM_ADD, 0, {}, peer == &first ? u"READY1" : u"READY2");
    }

    REQUIRE((*endpoint)->awaitDemand(STREAM, u"Alpha", u"READY1", true, deadline));
    REQUIRE((*endpoint)->awaitDemand(STREAM, u"Alpha", u"READY2", true, deadline));
    const auto publisher = (*endpoint)->getPublisher();
    using Type = c::SerialFieldType;
    std::array<c::FieldValueView, 8> values;
    std::vector<q::api::PublisherStreamRecordView> rows;
    std::u16string alpha = u"IBM";
    std::u16string beta = u"MSFT";

    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = {Type::COMPACT_INT, static_cast<std::int64_t>(100 + i), {}, {}};
        rows.push_back({schema.recordContext(static_cast<std::int32_t>(i % 2)), i % 2 == 0 ? alpha : beta,
                        std::span(&values[i], 1)});
    }

    values.back().type = Type::LONG;
    REQUIRE_FALSE(publisher.publishStreamRecords(rows));
    values.back().type = Type::COMPACT_INT;
    REQUIRE(publisher.publishStreamRecords(rows));
    REQUIRE(publisher.publishStreamRecords(rows));
    alpha.assign(u"BAD");
    beta.assign(u"BAD");
    values.fill({Type::COMPACT_INT, -1, {}, {}});

    const auto delivered = rows.size() * 2;

    for (auto *peer : {&first, &second}) {
        awaitRecords(*peer, delivered, **endpoint);

        for (std::size_t i = 0; i < delivered; ++i) {
            const auto rowIndex = i % rows.size();
            const auto &[family, record] = peer->records()[i];
            CHECK(family == 15);
            CHECK(record.localId == static_cast<std::int32_t>(rowIndex % 2));
            CHECK(record.symbol.units == (rowIndex % 2 == 0 ? u"IBM" : u"MSFT"));
            REQUIRE(record.typedValues.size() == 1);
            CHECK(record.typedValues[0].raw == static_cast<std::int64_t>(100 + rowIndex));
        }
    }

    (*endpoint)->closeAndAwaitTermination();
    const auto stats = (*endpoint)->getStatistics().families[1];

    if (!byteMode)
        CHECK(stats.ingressFrames.value == 2);

    CHECK(stats.ingressRecords.value == delivered);

    if (!byteMode)
        CHECK(stats.sentFrames.value == 4);

    const auto closed = publisher.publishStreamRecords(rows);
    REQUIRE_FALSE(closed);
    CHECK(closed.error().code == q::api::ErrorCode::CLOSED);
}

TEST_CASE("catalog STREAM splits an oversized frame and delivers its tail in order") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto schema = publicationSchema();
    const auto names = fieldNames(schema);
    q::api::PublisherOptions options;
    options.catalog.emplace(schema, names);
    options.streamBatchRecords = 8;
    options.listener.maxConnections = 1;
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, options);
    REQUIRE(endpoint);
    Downstream peer(schema, (*endpoint)->port(), 100);
    peer.subscribe(p::SubscriptionMessage::STREAM_ADD, 1);
    REQUIRE(
        (*endpoint)->awaitDemand(c::SubscriptionFamily::STREAM, u"CustomFlow", u"IBM", true, Clock::now() + WAIT_TIME));

    constexpr std::size_t PAYLOAD = MESSAGE_LIMIT / 8;
    const std::vector<std::byte> bytes(PAYLOAD, std::byte{0x53});
    using Type = c::SerialFieldType;
    std::array<std::array<c::FieldValueView, 3>, 8> values;
    std::array<q::api::PublisherStreamRecordView, 8> rows;

    for (std::size_t i = 0; i < rows.size(); ++i) {
        values[i] = {c::FieldValueView{Type::UTF_CHAR_ARRAY, 0, {}, {}},
                     c::FieldValueView{Type::BYTE_ARRAY, 0, {}, bytes},
                     c::FieldValueView{Type::LONG, static_cast<std::int64_t>(i), {}, {}}};
        rows[i] = {schema.recordContext(1), u"IBM", values[i]};
    }

    const auto publisher = (*endpoint)->getPublisher();
    REQUIRE(publisher.publishStreamRecords(rows));
    awaitRecords(peer, rows.size(), **endpoint);
    (*endpoint)->closeAndAwaitTermination();

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto &fields = peer.records()[i].second.typedValues;
        REQUIRE(fields.size() == 3);
        REQUIRE(fields[1].bytes);
        CHECK(*fields[1].bytes == bytes);
        CHECK(fields[2].raw == static_cast<std::int64_t>(i));
    }

    const auto stats = (*endpoint)->getStatistics().families[1];
    CHECK(stats.ingressFrames.value == 1);
    CHECK(stats.ingressRecords.value == 8);
    CHECK(stats.sentFrames.value == 2);
}

TEST_CASE("catalog STREAM byte limits admit whole calls and preserve mixed record order") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto schema = publicationSchema();
    const auto names = fieldNames(schema);
    q::api::PublisherOptions options;
    options.catalog.emplace(schema, names);
    options.listener.maxConnections = 1;
    options.listener.maxWriteBytes = 128;
    options.recordLimits.rowBytes = 1024;
    options.recordLimits.pendingBytes = 12 * 1024;
    options.streamBytes.emplace(128, 1024);
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, options);
    REQUIRE(endpoint);
    Downstream peer(schema, (*endpoint)->port(), 100);
    peer.subscribe(p::SubscriptionMessage::STREAM_ADD, 1);
    REQUIRE(
        (*endpoint)->awaitDemand(c::SubscriptionFamily::STREAM, u"CustomFlow", u"IBM", true, Clock::now() + WAIT_TIME));

    using Type = c::SerialFieldType;
    constexpr std::size_t ROW_COUNT = 12;
    const std::vector<std::byte> small(256, std::byte{0x31});
    const std::vector<std::byte> large(512, std::byte{0x32});
    std::array<std::array<c::FieldValueView, 3>, ROW_COUNT> values;
    std::array<q::api::PublisherStreamRecordView, ROW_COUNT> rows;

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto &bytes = i % 2 == 0 ? small : large;
        values[i] = {c::FieldValueView{Type::UTF_CHAR_ARRAY, 0, {}, {}},
                     c::FieldValueView{Type::BYTE_ARRAY, 0, {}, bytes},
                     c::FieldValueView{Type::LONG, static_cast<std::int64_t>(i), {}, {}}};
        rows[i] = {schema.recordContext(1), u"IBM", values[i]};
    }

    const auto publisher = (*endpoint)->getPublisher();
    const std::vector<std::byte> oversized(1024, std::byte{0x53});
    const auto saved = values.back()[1];
    values.back()[1] = {Type::BYTE_ARRAY, 0, {}, oversized};
    const auto rejected = publisher.publishStreamRecords(rows);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code == q::api::ErrorCode::RESOURCE_LIMIT);
    values.back()[1] = saved;
    const std::vector<q::api::PublisherStreamRecordView> tooMany(64, rows.back());
    const auto full = publisher.publishStreamRecords(tooMany);
    REQUIRE_FALSE(full);
    CHECK(full.error().stage == "publisher-publication-capacity");
    REQUIRE(publisher.publishStreamRecords(rows));
    awaitRecords(peer, ROW_COUNT, **endpoint);
    (*endpoint)->closeAndAwaitTermination();

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto &fields = peer.records()[i].second.typedValues;
        REQUIRE(fields.size() == 3);
        REQUIRE(fields[1].bytes);
        CHECK(*fields[1].bytes == (i % 2 == 0 ? small : large));
        CHECK(fields[2].raw == static_cast<std::int64_t>(i));
    }

    const auto stats = (*endpoint)->getStatistics().families[1];
    CHECK(stats.ingressRecords.value == ROW_COUNT);
    CHECK(stats.sentFrames.value == ROW_COUNT);
    REQUIRE(peer.streamFrames().size() == ROW_COUNT);

    for (const auto &[payloadBytes, frameBytes] : peer.streamFrames()) {
        CHECK(payloadBytes > 128);
        CHECK(frameBytes <= 1024);
    }
}

TEST_CASE("catalog STREAM oversized wire record retires only its peer and counts the failed frame") {
    using enum c::SubscriptionFamily;
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto schema = publicationSchema();
    const auto names = fieldNames(schema);
    q::api::PublisherOptions options;
    options.catalog.emplace(schema, names);
    options.listener.maxConnections = 2;
    // The first row may exceed the detached selection target and still reach the wire size check.
    options.streamBytes.emplace(64, 128, 64);
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, options);
    REQUIRE(endpoint);
    Downstream first(schema, (*endpoint)->port(), 100);
    Downstream second(schema, (*endpoint)->port(), 200);
    first.subscribe(p::SubscriptionMessage::STREAM_ADD, 1);
    second.subscribe(p::SubscriptionMessage::STREAM_ADD, 1, {}, u"FAST");
    const auto deadline = Clock::now() + WAIT_TIME;
    REQUIRE((*endpoint)->awaitDemand(STREAM, u"CustomFlow", u"IBM", true, deadline));
    REQUIRE((*endpoint)->awaitDemand(STREAM, u"CustomFlow", u"FAST", true, deadline));

    using Type = c::SerialFieldType;
    const std::vector<std::byte> bytes(512, std::byte{0x53});
    const std::array large{c::FieldValueView{Type::UTF_CHAR_ARRAY, 0, {}, {}},
                           c::FieldValueView{Type::BYTE_ARRAY, 0, {}, bytes}, c::FieldValueView{Type::LONG, 1, {}, {}}};
    const std::array small{large[0], c::FieldValueView{Type::BYTE_ARRAY, 0, {}, {}}, large[2]};
    const auto context = schema.recordContext(1);
    const auto publisher = (*endpoint)->getPublisher();
    REQUIRE(publisher.publishRecord(STREAM, context, u"IBM", large));

    while ((*endpoint)->getStatistics().families[1].failedFrames.value == 0 && Clock::now() < deadline)
        std::this_thread::sleep_for(1ms);

    const auto failed = (*endpoint)->getStatistics().families[1].failedFrames;
    CHECK(failed.availability == c::StatisticAvailability::AVAILABLE);
    REQUIRE(failed.value == 1);
    REQUIRE(publisher.publishRecord(STREAM, context, u"FAST", small));
    awaitRecords(second, 1, **endpoint);
    CHECK((*endpoint)->getDiagnostic().code == q::api::ErrorCode::NONE);
}

TEST_CASE("flat STREAM delivery window counts a whole batch before its first row") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto schema = publicationSchema();
    const auto names = fieldNames(schema);
    q::api::PublisherOptions options;
    options.catalog.emplace(schema, names);
    options.pendingQuotes = 32;
    options.listener.maxConnections = 1;
    options.listener.maxWriteBytes = 1024;
    options.recordLimits.rowBytes = 16 * 1024;
    options.recordLimits.pendingBytes = 256 * 1024;
    options.streamBytes.emplace(8000, MESSAGE_LIMIT, 64 * 1024);
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, options);
    INFO((endpoint ? std::string_view{} : endpoint.error().stage));
    REQUIRE(endpoint);
    Downstream peer(schema, (*endpoint)->port(), 100);
    peer.subscribe(p::SubscriptionMessage::STREAM_ADD, 1);
    REQUIRE(
        (*endpoint)->awaitDemand(c::SubscriptionFamily::STREAM, u"CustomFlow", u"IBM", true, Clock::now() + WAIT_TIME));
    constexpr std::size_t ROWS = 4;
    constexpr std::size_t BATCHES = 4;
    const std::vector<std::byte> bytes(12 * 1024, std::byte{0x37});
    std::array<std::array<c::FieldValueView, 3>, ROWS> fields;
    std::array<q::api::PublisherStreamRecordView, ROWS> rows;
    const auto publisher = (*endpoint)->getPublisher();

    for (std::size_t batch = 0; batch < BATCHES; ++batch) {
        for (std::size_t i = 0; i < ROWS; ++i) {
            fields[i] = {
                c::FieldValueView{c::SerialFieldType::UTF_CHAR_ARRAY, 0, {}, {}},
                c::FieldValueView{c::SerialFieldType::BYTE_ARRAY, 0, {}, bytes},
                c::FieldValueView{c::SerialFieldType::LONG, static_cast<std::int64_t>(batch * ROWS + i), {}, {}}};
            rows[i] = {schema.recordContext(1), u"IBM", fields[i]};
        }

        REQUIRE(publisher.publishStreamRecords(rows));

        if (batch == 0) {
            const auto deadline = Clock::now() + WAIT_TIME;

            while ((*endpoint)->getStatistics().families[1].deliveryRecordOwners < ROWS && Clock::now() < deadline)
                std::this_thread::yield();

            REQUIRE((*endpoint)->getStatistics().families[1].deliveryRecordOwners >= ROWS);
        }
    }

    awaitRecords(peer, ROWS * BATCHES, **endpoint);

    for (std::size_t i = 0; i < peer.records().size(); ++i) {
        const auto &values = peer.records()[i].second.typedValues;
        CHECK(values[2].raw == static_cast<std::int64_t>(i));
        REQUIRE(values[1].bytes);
        CHECK(*values[1].bytes == bytes);
    }

    CHECK((*endpoint)->getStatistics().retiredPeers.value == 0);
    CHECK((*endpoint)->getStatistics().families[1].failedFrames.value == 0);
    (*endpoint)->closeAndAwaitTermination();
}

TEST_CASE("catalog STREAM sends more than 512 compact records per byte-bounded frame with partial writes") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto schema = testSchema();
    const auto names = fieldNames(schema);
    q::api::PublisherOptions options;
    options.catalog.emplace(schema, names);
    constexpr std::size_t ROW_COUNT = 1024;
    options.pendingQuotes = ROW_COUNT;
    options.listener.maxConnections = 1;
    options.listener.maxWriteBytes = 128;
    constexpr std::size_t TARGET_PAYLOAD = 2500;
    constexpr std::size_t MAX_FRAME = 2560;
    options.streamBytes.emplace(TARGET_PAYLOAD, MAX_FRAME, 2 * 1024 * 1024);
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, options);
    REQUIRE(endpoint);
    Downstream peer(schema, (*endpoint)->port(), 100);
    peer.subscribe(p::SubscriptionMessage::STREAM_ADD, 0);
    REQUIRE((*endpoint)->awaitDemand(c::SubscriptionFamily::STREAM, u"Alpha", u"IBM", true, Clock::now() + WAIT_TIME));

    std::array<c::FieldValueView, ROW_COUNT> values;
    std::array<q::api::PublisherStreamRecordView, ROW_COUNT> rows;

    for (std::size_t i = 0; i < rows.size(); ++i) {
        values[i] = {c::SerialFieldType::COMPACT_INT, static_cast<std::int64_t>(i), {}, {}};
        rows[i] = {schema.recordContext(0), u"IBM", std::span(&values[i], 1)};
    }

    const auto publisher = (*endpoint)->getPublisher();
    REQUIRE(publisher.publishStreamRecords(rows));
    awaitRecords(peer, ROW_COUNT, **endpoint);

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto &fields = peer.records()[i].second.typedValues;
        REQUIRE(fields.size() == 1);
        CHECK(fields[0].raw == static_cast<std::int64_t>(i));
    }

    (*endpoint)->closeAndAwaitTermination();
    const auto stats = (*endpoint)->getStatistics();
    CHECK(stats.retiredPeers.value == 0);
    CHECK(stats.families[1].ingressFrames.value == 1);
    CHECK(stats.families[1].ingressRecords.value == ROW_COUNT);
    CHECK(stats.families[1].failedFrames.value == 0);
    CHECK(stats.families[1].maxSentFrameRecords > 512);
    CHECK(stats.families[1].maxSentFrameBytes <= MAX_FRAME);
    REQUIRE_FALSE(peer.streamFrames().empty());
    CHECK(peer.streamFrames().front().first >= TARGET_PAYLOAD);
    CHECK(peer.streamFrames().front().second <= MAX_FRAME);
}

TEST_CASE("catalog STREAM combines input windows after a partially written frame") {
    using enum c::SubscriptionFamily;
    using Type = c::SerialFieldType;
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto schema = publicationSchema();
    const auto names = fieldNames(schema);
    constexpr std::size_t INPUT_SLOTS = 8;
    constexpr std::size_t INPUT_GROUPS = 4;
    q::api::PublisherOptions options;
    options.catalog.emplace(schema, names);
    options.pendingQuotes = INPUT_SLOTS;
    options.listener.maxConnections = 1;
    options.listener.maxWriteBytes = 64;
    options.streamBytes.emplace(512, 16 * 1024);
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, options);
    REQUIRE(endpoint);
    Downstream peer(schema, (*endpoint)->port(), 100);
    peer.subscribe(p::SubscriptionMessage::STREAM_ADD, 1);
    const auto deadline = Clock::now() + WAIT_TIME;
    REQUIRE((*endpoint)->awaitDemand(STREAM, u"CustomFlow", u"IBM", true, deadline));

    const auto publisher = (*endpoint)->getPublisher();
    const auto context = schema.recordContext(1);
    const std::vector<std::byte> payload(8192, std::byte{0x53});
    const std::array first{c::FieldValueView{Type::UTF_CHAR_ARRAY, 0, {}, {}},
                           c::FieldValueView{Type::BYTE_ARRAY, 0, {}, payload},
                           c::FieldValueView{Type::LONG, 0, {}, {}}};
    REQUIRE(publisher.publishRecord(STREAM, context, u"IBM", first));

    while (!peer.receivingData() && Clock::now() < deadline) {
        peer.poll();
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(peer.receivingData());
    std::array<std::array<c::FieldValueView, 3>, INPUT_SLOTS> values;
    std::array<q::api::PublisherStreamRecordView, INPUT_SLOTS> rows;

    for (std::size_t group = 0; group < INPUT_GROUPS; ++group) {
        for (std::size_t row = 0; row < rows.size(); ++row) {
            const auto sequence = static_cast<std::int64_t>(group * rows.size() + row + 1);
            values[row] = {first[0], c::FieldValueView{Type::BYTE_ARRAY, 0, {}, {}},
                           c::FieldValueView{Type::LONG, sequence, {}, {}}};
            rows[row] = {context, u"IBM", values[row]};
        }

        for (;;) {
            const auto admitted = publisher.publishStreamRecords(rows);

            if (admitted)
                break;

            REQUIRE(admitted.error().stage == "publisher-pending-records");
            REQUIRE(Clock::now() < deadline);
            std::this_thread::yield();
        }
    }

    awaitRecords(peer, INPUT_GROUPS * INPUT_SLOTS + 1, **endpoint);

    for (std::size_t row = 0; row < peer.records().size(); ++row)
        CHECK(peer.records()[row].second.typedValues[2].raw == static_cast<std::int64_t>(row));

    (*endpoint)->closeAndAwaitTermination();
    const auto stats = (*endpoint)->getStatistics();
    CHECK(stats.families[1].maxSentFrameRecords > INPUT_SLOTS);
    CHECK(stats.families[1].failedFrames.value == 0);
    CHECK(stats.retiredPeers.value == 0);
}

TEST_CASE("revocation of a non-first row in a partial STREAM frame closes only its peer") {
    using enum c::SubscriptionFamily;
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto schema = publicationSchema();
    const auto names = fieldNames(schema);
    q::api::PublisherOptions options;
    options.catalog.emplace(schema, names);
    options.streamBatchRecords = 8;
    SUBCASE("legacy batch limit") {
    }
    SUBCASE("byte-limited output") {
        options.streamBytes.emplace(20000, 65536);
    }
    options.listener.maxWriteBytes = 64;
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, options);
    REQUIRE(endpoint);
    Downstream slow(schema, (*endpoint)->port(), 100);
    slow.subscribe(p::SubscriptionMessage::STREAM_ADD, 1);
    slow.subscribe(p::SubscriptionMessage::STREAM_ADD, 1, {}, u"MSFT");
    REQUIRE((*endpoint)->awaitDemand(STREAM, u"CustomFlow", u"MSFT", true, Clock::now() + WAIT_TIME));
    const auto peers = (*endpoint)->getPeers();
    REQUIRE(peers.size() == 1);
    Downstream fast(schema, (*endpoint)->port(), 200);
    fast.subscribe(p::SubscriptionMessage::STREAM_ADD, 1, {}, u"FAST");
    REQUIRE((*endpoint)->awaitDemand(STREAM, u"CustomFlow", u"FAST", true, Clock::now() + WAIT_TIME));

    const std::vector<std::byte> bytes(8192, std::byte{0x53});
    using Type = c::SerialFieldType;
    const std::array large{c::FieldValueView{Type::UTF_CHAR_ARRAY, 0, {}, {}},
                           c::FieldValueView{Type::BYTE_ARRAY, 0, {}, bytes}, c::FieldValueView{Type::LONG, 0, {}, {}}};
    const std::array small{large[0], c::FieldValueView{Type::BYTE_ARRAY, 0, {}, {}}, large[2]};
    const auto context = schema.recordContext(1);
    const std::array rows{q::api::PublisherStreamRecordView{context, u"IBM", large},
                          q::api::PublisherStreamRecordView{context, u"MSFT", large}};
    const auto publisher = (*endpoint)->getPublisher();
    REQUIRE(publisher.publishStreamRecords(rows));
    const auto deadline = Clock::now() + WAIT_TIME;

    while (!slow.receivingData() && Clock::now() < deadline) {
        slow.poll();
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(slow.receivingData());
    REQUIRE(slow.records().empty());
    REQUIRE(publisher.publishRecord(STREAM, context, u"FAST", small));
    awaitRecords(fast, 1, **endpoint);

    q::api::EventRegistryWork work;
    const auto registry = q::api::EventRegistry::builtIn({}, work);
    REQUIRE(registry);
    const auto policy = q::api::FeedFilter::Builder(*registry).withCategory(u"IBM", u"IBM").build();
    const auto command = (*endpoint)->replacePeerDemand(peers.front().id, {{}, policy, {}});
    REQUIRE(command);
    const auto result = command->waitUntil(Clock::now() + WAIT_TIME);
    REQUIRE(result);
    CHECK(result->outcome == q::api::PublisherPolicyOutcome::PEER_CLOSED);
    CHECK(result->diagnostic.stage == "publisher-policy-wire-boundary");
    REQUIRE(publisher.publishRecord(STREAM, context, u"FAST", small));
    awaitRecords(fast, 2, **endpoint);
    CHECK((*endpoint)->getDiagnostic().code == q::api::ErrorCode::NONE);
}

TEST_CASE("local catalog publishes exact copied records through TICKER and STREAM") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    auto schema = publicationSchema();
    const auto quoteContext = schema.recordContext(0);
    const auto customContext = schema.recordContext(1);
    const auto receiver = publicationSchema();
    q::api::PublisherOptions options;
    {
        auto names = fieldNames(schema);
        options.catalog.emplace(schema, names);
        auto created = q::api::PublisherEndpoint::create(*bootstrap, options);
        INFO((created ? std::string_view{} : created.error().stage));
        REQUIRE(created);
        auto endpoint = std::move(*created);
        options.catalog.reset();
        // The local catalog, names and each submitted payload may be destroyed by the caller.
        schema = publicationSchema();
        names.clear();

        const auto publisher = endpoint->getPublisher();
        Downstream first(receiver, endpoint->port(), 91);
        first.subscribe(p::SubscriptionMessage::TICKER_ADD, 0);
        first.subscribe(p::SubscriptionMessage::STREAM_ADD, 1);
        const auto demandDeadline = Clock::now() + WAIT_TIME;
        REQUIRE(endpoint->awaitDemand(c::SubscriptionFamily::TICKER, u"Quote", u"IBM", true, demandDeadline));
        REQUIRE(endpoint->awaitDemand(c::SubscriptionFamily::STREAM, u"CustomFlow", u"IBM", true, demandDeadline));

        std::vector<c::FieldValueView> quote;

        for (const auto &field : receiver.records()[0].fields)
            quote.push_back({static_cast<c::SerialFieldType>(field.serialTypeId), 0, {}, {}});

        for (const auto bid : {501, -907, 1201}) {
            quote[2].raw = bid;
            REQUIRE(publisher.publishRecord(c::SubscriptionFamily::TICKER, quoteContext, u"IBM", quote));
        }

        {
            std::u16string symbol = u"IBM";
            std::u16string units{u'Q', 0, 0xd800};
            std::array bytes{std::byte{0}, std::byte{0xff}};
            using Type = c::SerialFieldType;
            const std::array fields{c::FieldValueView{Type::UTF_CHAR_ARRAY, 0, units, {}},
                                    c::FieldValueView{Type::BYTE_ARRAY, 0, {}, std::span<const std::byte>(bytes)},
                                    c::FieldValueView{Type::LONG, -42, {}, {}}};
            REQUIRE(publisher.publishRecord(c::SubscriptionFamily::STREAM, customContext, symbol, fields));
            REQUIRE(publisher.publishRecord(c::SubscriptionFamily::STREAM, customContext, symbol, fields));
            symbol.assign(u"BAD");
            units.clear();
            bytes.fill(std::byte{0x17});
        }
        using Type = c::SerialFieldType;
        const std::array empty{c::FieldValueView{Type::UTF_CHAR_ARRAY, 0, std::u16string_view{}, {}},
                               c::FieldValueView{Type::BYTE_ARRAY, 0, {}, std::span<const std::byte>{}},
                               c::FieldValueView{Type::LONG, 99, {}, {}}};
        const std::array null{c::FieldValueView{Type::UTF_CHAR_ARRAY, 0, {}, {}},
                              c::FieldValueView{Type::BYTE_ARRAY, 0, {}, {}},
                              c::FieldValueView{Type::LONG, 100, {}, {}}};
        REQUIRE(publisher.publishRecord(c::SubscriptionFamily::STREAM, customContext, u"IBM", empty));
        REQUIRE(publisher.publishRecord(c::SubscriptionFamily::STREAM, customContext, u"IBM", null));

        const auto deadline = Clock::now() + WAIT_TIME;

        while (endpoint->getStatistics().pendingPublications != 0 && Clock::now() < deadline)
            std::this_thread::sleep_for(1ms);

        REQUIRE(endpoint->getStatistics().pendingPublications == 0);

        Downstream later(receiver, endpoint->port(), 52);
        later.subscribe(p::SubscriptionMessage::TICKER_ADD, 0);
        first.poll();
        awaitRecords(later, 1, *endpoint);
        CHECK(later.records().front().first == 10);
        REQUIRE(later.records().front().second.typedValues.size() == quote.size());
        CHECK(later.records().front().second.typedValues[2].raw == 1201);

        const auto streamDeadline = Clock::now() + WAIT_TIME;

        while (Clock::now() < streamDeadline) {
            first.poll();

            if (std::ranges::count(first.records(), 15, &std::pair<int, p::StreamDataRecord>::first) == 4)
                break;

            std::this_thread::sleep_for(1ms);
        }

        std::vector<const p::StreamDataRecord *> stream;

        for (const auto &[message, record] : first.records()) {
            if (message == 15)
                stream.push_back(&record);
        }

        REQUIRE(stream.size() == 4);

        for (const auto *row : stream) {
            CHECK(row->symbol.units == u"IBM");
            CHECK(row->localId == 1);
            REQUIRE(row->typedValues.size() == 3);
        }

        const std::u16string expectedUnits{u'Q', 0, 0xd800};
        const std::vector expectedBytes{std::byte{0}, std::byte{0xff}};
        CHECK(stream[0]->typedValues[0].units == expectedUnits);
        CHECK(stream[0]->typedValues[1].bytes == expectedBytes);
        CHECK(stream[0]->typedValues[2].raw == -42);
        CHECK(stream[0]->typedValues == stream[1]->typedValues);
        CHECK(stream[2]->typedValues[0].units == std::u16string{});
        CHECK(stream[2]->typedValues[1].bytes == std::vector<std::byte>{});
        CHECK(stream[2]->typedValues[2].raw == 99);
        CHECK_FALSE(stream[3]->typedValues[0].units);
        CHECK_FALSE(stream[3]->typedValues[1].bytes);
        CHECK(stream[3]->typedValues[2].raw == 100);

        REQUIRE_FALSE(publisher.publishRecord(c::SubscriptionFamily::HISTORY, customContext, u"IBM", null));
        REQUIRE_FALSE(publisher.publishRecord(c::SubscriptionFamily::STREAM, schema.recordContext(1), u"IBM", null));
        REQUIRE_FALSE(publisher.publishRecord(c::SubscriptionFamily::STREAM, customContext, u"*", null));
        REQUIRE_FALSE(
            publisher.publishRecord(c::SubscriptionFamily::STREAM, customContext, u"IBM", std::span(null).first(1)));
        auto malformed = null;
        malformed[2].type = Type::COMPACT_INT;
        REQUIRE_FALSE(publisher.publishRecord(c::SubscriptionFamily::STREAM, customContext, u"IBM", malformed));
        CHECK(endpoint->getStatistics().pendingPublications == 0);

        endpoint.reset();
        const auto closed = publisher.publishRecord(c::SubscriptionFamily::STREAM, customContext, u"IBM", null);
        REQUIRE_FALSE(closed);
        CHECK(closed.error().code == q::api::ErrorCode::CLOSED);
    }
}

TEST_CASE("local catalog publishes indexed snapshots and transactions to two QTP clients") {
    using Family = c::SubscriptionFamily;
    using Type = c::SerialFieldType;
    namespace flags = q::events::indexed_event_flags;
    constexpr auto SNAPSHOT_REMOVAL = flags::SNAPSHOT_END | flags::REMOVE_EVENT;
    constexpr auto FIRST_INDEX = REMOVAL_TIME + 3;
    constexpr auto NEXT_INDEX = REMOVAL_TIME + 2;
    constexpr auto HISTORY_DATA = std::to_underlying(p::LiveDataMessage::HISTORY_DATA);
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto schema = publicationSchema();
    const auto receiver = publicationSchema();
    const auto names = fieldNames(schema);
    q::api::PublisherOptions options;
    options.catalog.emplace(schema, names);
    auto created = q::api::PublisherEndpoint::create(*bootstrap, options);
    REQUIRE(created);
    const auto endpoint = std::move(*created);
    const auto publisher = endpoint->getPublisher();
    const auto indexed = schema.recordContext(2);
    const auto unindexed = schema.recordContext(1);

    Downstream first(receiver, endpoint->port(), 91);
    Downstream second(receiver, endpoint->port(), 52);
    first.subscribe(p::SubscriptionMessage::HISTORY_ADD, 2, REMOVAL_TIME);
    REQUIRE(endpoint->awaitDemand(Family::HISTORY, u"Indexed", u"IBM", true, Clock::now() + WAIT_TIME));

    second.subscribe(p::SubscriptionMessage::HISTORY_ADD, 2, REMOVAL_TIME);
    second.subscribe(p::SubscriptionMessage::HISTORY_ADD, 2, REMOVAL_TIME, u"BARRIER");
    REQUIRE(endpoint->awaitDemand(Family::HISTORY, u"Indexed", u"BARRIER", true, Clock::now() + WAIT_TIME));

    const std::array initial{c::FieldValueView{Type::LONG, FIRST_INDEX}, c::FieldValueView{Type::COMPACT_INT, 303}};
    const std::array update{c::FieldValueView{Type::LONG, NEXT_INDEX}, c::FieldValueView{Type::COMPACT_INT, 404}};
    auto reserved = initial;
    reserved[0].raw = std::numeric_limits<std::int64_t>::max();

    CHECK_FALSE(publisher.publishRecord(Family::HISTORY, indexed, u"IBM", initial, {0x20}));
    CHECK_FALSE(publisher.publishRecord(Family::HISTORY, indexed, u"IBM", initial, {0, REMOVAL_TIME}));
    CHECK_FALSE(publisher.publishRecord(Family::HISTORY, indexed, u"IBM", {}, {flags::REMOVE_EVENT}));
    CHECK_FALSE(publisher.publishRecord(Family::HISTORY, indexed, u"IBM", initial, {flags::REMOVE_EVENT, FIRST_INDEX}));
    CHECK_FALSE(publisher.publishRecord(Family::HISTORY, indexed, u"IBM", reserved));
    CHECK_FALSE(publisher.publishRecord(Family::HISTORY, unindexed, u"IBM", {}, {flags::REMOVE_EVENT, REMOVAL_TIME}));
    CHECK_FALSE(publisher.publishRecord(Family::STREAM, indexed, u"IBM", initial, {flags::TX_PENDING}));
    CHECK(endpoint->getStatistics().pendingPublications == 0);
    CHECK(endpoint->getStatistics().families[2].ingressRecords.value == 0);

    REQUIRE(publisher.publishRecord(Family::HISTORY, indexed, u"IBM", initial, {flags::SNAPSHOT_BEGIN}));
    REQUIRE(publisher.publishRecord(Family::HISTORY, indexed, u"IBM", {}, {SNAPSHOT_REMOVAL, REMOVAL_TIME}));
    awaitRecords(first, 2, *endpoint);
    awaitRecords(second, 2, *endpoint);

    REQUIRE(publisher.publishRecord(Family::HISTORY, indexed, u"IBM", update, {flags::TX_PENDING}));
    CHECK_FALSE(publisher.publishRecord(Family::HISTORY, indexed, u"IBM", {}, {flags::REMOVE_EVENT}));
    REQUIRE(publisher.publishRecord(Family::HISTORY, indexed, u"IBM", {}, {flags::REMOVE_EVENT, FIRST_INDEX}));
    awaitRecords(first, 4, *endpoint);
    awaitRecords(second, 4, *endpoint);

    for (const auto *peer : {&first, &second}) {
        const auto &rows = peer->records();

        for (const auto &[contract, record] : rows) {
            CHECK(contract == HISTORY_DATA);
            CHECK(record.context.matches(receiver.recordContext(2)));
            CHECK(record.symbol.units == u"IBM");
        }

        const auto verifyValue = [&](std::size_t at, std::int32_t flag, const auto &expected) {
            const auto &record = rows[at].second;
            CHECK(record.symbol.eventFlags == flag);
            CHECK_FALSE(record.removalTime);
            REQUIRE(record.typedValues.size() == expected.size());
            CHECK(record.typedValues[0].raw == expected[0].raw);
            CHECK(record.typedValues[1].raw == expected[1].raw);
        };
        verifyValue(0, flags::SNAPSHOT_BEGIN, initial);
        CHECK(rows[1].second.symbol.eventFlags == SNAPSHOT_REMOVAL);
        CHECK(rows[1].second.removalTime == REMOVAL_TIME);
        verifyValue(2, flags::TX_PENDING, update);
        CHECK(rows[3].second.symbol.eventFlags == flags::REMOVE_EVENT);
        CHECK(rows[3].second.removalTime == FIRST_INDEX);
    }

    CHECK(endpoint->getStatistics().families[2].ingressRecords.value == 4);
    endpoint->closeAndAwaitTermination();
    CHECK(endpoint->getDiagnostic().code == q::api::ErrorCode::NONE);
}

TEST_CASE("local indexed catalog excludes outbound layouts that cannot encode HISTORY removals") {
    using Family = c::SubscriptionFamily;
    const auto base = publicationSchema();
    std::vector<c::LocalRecord> records(base.records().begin(), base.records().end());
    records[2].lookups[0].name = u"EventIndex";
    auto schema = c::LocalSchema::create(records, SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto names = fieldNames(*schema);
    q::api::PublisherOptions options;
    options.catalog.emplace(*schema, names);
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto created = q::api::PublisherEndpoint::create(*bootstrap, options);
    REQUIRE(created);

    const auto indexed = schema->recordContext(2);
    const auto publisher = (*created)->getPublisher();
    const std::array fields{c::FieldValueView{c::SerialFieldType::LONG, REMOVAL_TIME},
                            c::FieldValueView{c::SerialFieldType::COMPACT_INT, 303}};
    CHECK_FALSE(publisher.publishRecord(Family::HISTORY, indexed, u"IBM", fields));
    CHECK(publisher.publishRecord(Family::STREAM, indexed, u"IBM", fields));
}

TEST_CASE("local record limits reject before admission and the producer linearizes with close") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto schema = publicationSchema();
    const auto names = fieldNames(schema);
    const auto context = schema.recordContext(1);
    using Type = c::SerialFieldType;
    const std::array empty{c::FieldValueView{Type::UTF_CHAR_ARRAY, 0, {}, {}},
                           c::FieldValueView{Type::BYTE_ARRAY, 0, {}, {}}, c::FieldValueView{Type::LONG, 15, {}, {}}};
    using enum c::SubscriptionFamily;
    q::api::PublisherOptions options;
    options.catalog.emplace(schema, names);
    constexpr auto BASE_BYTES = sizeof(c::FieldValueView) * empty.size() + sizeof(char16_t) * 3;
    constexpr auto EXTRA_PAYLOAD_BYTES = 2 * alignof(std::max_align_t);
    namespace d = c::detail;
    d::RecordByteBudget physical(1024 * 1024);
    auto catalog = schema;
    auto binding = d::RecordBlockBinding::create(std::move(catalog), {}, physical);
    REQUIRE(binding);
    const std::array input{d::RecordBlockInput{context, u"IBM", empty, {}}};
    const auto measured = d::SharedRecordBlock::measure(**binding, input);
    REQUIRE(measured);
    const auto nodeBytes =
        d::recordControlBytes<q::api::detail::Publication>(d::RecordByteReservation{}, d::RecordByteReservation{},
                                                           std::shared_ptr<const d::BoundRecordBatch>{}, std::size_t{});
    options.recordLimits = {BASE_BYTES + EXTRA_PAYLOAD_BYTES, (*binding)->retainedCapacityBytes() +
                                                                  measured->backingAllocationBytes +
                                                                  measured->controlAllocationBytes + nodeBytes};
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, options);
    INFO((endpoint ? std::string_view{} : endpoint.error().stage));
    REQUIRE(endpoint);
    const auto publisher = (*endpoint)->getPublisher();

    std::array<std::byte, EXTRA_PAYLOAD_BYTES> bytes{};
    auto canFitRow = empty;
    canFitRow[1].bytes = bytes;
    const std::array largerInput{d::RecordBlockInput{context, u"IBM", canFitRow, {}}};
    const auto largerMeasured = d::SharedRecordBlock::measure(**binding, largerInput);
    REQUIRE(largerMeasured);
    REQUIRE(largerMeasured->backingAllocationBytes > measured->backingAllocationBytes);
    const auto full = publisher.publishRecord(STREAM, context, u"IBM", canFitRow);
    REQUIRE_FALSE(full);
    CHECK(full.error().stage == "publisher-publication-capacity");

    std::array<std::byte, EXTRA_PAYLOAD_BYTES + 1> oversized{};
    auto overLimit = empty;
    overLimit[1].bytes = oversized;
    const auto large = publisher.publishRecord(STREAM, context, u"IBM", overLimit);
    REQUIRE_FALSE(large);
    CHECK(large.error().stage == "publisher-record-limit");
    CHECK((*endpoint)->getStatistics().pendingPublications == 0);
    REQUIRE(publisher.publishRecord(STREAM, context, u"IBM", empty));

    std::atomic<bool> started{false};
    std::atomic<bool> closed{false};
    std::jthread producer([&](std::stop_token stop) {
        started.store(true, std::memory_order_release);

        while (!stop.stop_requested()) {
            const auto result = publisher.publishRecord(STREAM, context, u"IBM", empty);

            if (!result && result.error().code == q::api::ErrorCode::CLOSED) {
                closed.store(true, std::memory_order_release);

                return;
            }

            std::this_thread::yield();
        }
    });
    const auto deadline = Clock::now() + WAIT_TIME;

    while (!started.load(std::memory_order_acquire) && Clock::now() < deadline)
        std::this_thread::yield();

    REQUIRE(started.load(std::memory_order_acquire));
    endpoint->reset();

    while (!closed.load(std::memory_order_acquire) && Clock::now() < deadline)
        std::this_thread::yield();

    CHECK(closed.load(std::memory_order_acquire));
}

TEST_CASE("concurrent catalog publications keep exact nullable fields FIFO and indivisible commit order") {
    using enum c::SubscriptionFamily;
    using Type = c::SerialFieldType;
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto schema = publicationSchema();
    q::api::PublisherOptions options;
    const auto names = fieldNames(schema);
    options.catalog.emplace(schema, names);
    options.pendingQuotes = 32;
    options.streamBytes.emplace(1200, 16384);
    options.listener.maxConnections = 1;
    options.listener.maxWriteBytes = 37;
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, options);
    REQUIRE(endpoint);
    Downstream peer(schema, (*endpoint)->port(), 100);
    peer.subscribe(p::SubscriptionMessage::STREAM_ADD, 1);
    REQUIRE((*endpoint)->awaitDemand(STREAM, u"CustomFlow", u"IBM", true, Clock::now() + WAIT_TIME));
    const auto publisher = (*endpoint)->getPublisher();
    constexpr std::size_t PRODUCERS = 3;
    constexpr std::size_t CALLS = 12;
    constexpr std::size_t ROWS = 4;
    constexpr std::int64_t PRODUCER_STRIDE = 100000;
    constexpr std::int64_t CALL_STRIDE = 100;
    const auto deadline = Clock::now() + WAIT_TIME;
    std::atomic<bool> start{false};
    std::atomic<std::size_t> finished{0};
    std::atomic<bool> failed{false};
    std::array<std::jthread, PRODUCERS> producers;

    for (std::size_t producer = 0; producer < PRODUCERS; ++producer) {
        producers[producer] = std::jthread([&, producer] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (std::size_t call = 0; call < CALLS; ++call) {
                std::array<std::array<char16_t, 3>, ROWS> text;
                std::array<std::array<std::byte, 4>, ROWS> bytes;
                std::array<std::array<c::FieldValueView, 3>, ROWS> values;
                std::array<q::api::PublisherStreamRecordView, ROWS> rows;

                for (std::size_t row = 0; row < ROWS; ++row) {
                    text[row] = {static_cast<char16_t>(producer + 1), 0, static_cast<char16_t>(0xd800 + call)};
                    bytes[row] = {static_cast<std::byte>(producer), static_cast<std::byte>(call),
                                  static_cast<std::byte>(row), std::byte{0xff}};
                    const auto sequence = static_cast<std::int64_t>(producer) * PRODUCER_STRIDE +
                                          static_cast<std::int64_t>(call) * CALL_STRIDE +
                                          static_cast<std::int64_t>(row);
                    values[row] = {c::FieldValueView{Type::UTF_CHAR_ARRAY, 0, {}, {}},
                                   c::FieldValueView{Type::BYTE_ARRAY, 0, {}, {}},
                                   c::FieldValueView{Type::LONG, sequence, {}, {}}};

                    if (row != 2) {
                        values[row][0].units = std::u16string_view(text[row].data(), row == 1 ? 0 : text[row].size());
                        values[row][1].bytes =
                            std::span<const std::byte>(bytes[row]).first(row == 1 ? 0 : bytes[row].size());
                    }

                    rows[row] = {schema.recordContext(1), u"IBM", values[row]};
                }

                for (;;) {
                    const auto result = publisher.publishStreamRecords(rows);

                    if (result) {
                        break;
                    }

                    if (result.error().code != q::api::ErrorCode::RESOURCE_LIMIT || Clock::now() >= deadline) {
                        failed.store(true, std::memory_order_release);
                        finished.fetch_add(1, std::memory_order_release);

                        return;
                    }

                    std::this_thread::yield();
                }

                for (auto &row : text) {
                    row.fill(u'X');
                }

                for (auto &row : bytes) {
                    row.fill(std::byte{0x11});
                }
            }

            finished.fetch_add(1, std::memory_order_release);
        });
    }

    start.store(true, std::memory_order_release);

    while (
        (finished.load(std::memory_order_acquire) != PRODUCERS || peer.records().size() < PRODUCERS * CALLS * ROWS) &&
        Clock::now() < deadline) {
        peer.poll();
        std::this_thread::yield();
    }

    for (auto &producer : producers) {
        producer.join();
    }

    REQUIRE_FALSE(failed.load());
    awaitRecords(peer, PRODUCERS * CALLS * ROWS, **endpoint);
    std::array<std::size_t, PRODUCERS> nextCall{};

    for (std::size_t first = 0; first < peer.records().size(); first += ROWS) {
        const auto sequence = peer.records()[first].second.typedValues[2].raw;
        const auto producer = static_cast<std::size_t>(sequence / PRODUCER_STRIDE);
        const auto call = static_cast<std::size_t>((sequence % PRODUCER_STRIDE) / CALL_STRIDE);
        REQUIRE(producer < PRODUCERS);
        CHECK(call == nextCall[producer]++);

        for (std::size_t row = 0; row < ROWS; ++row) {
            const auto &record = peer.records()[first + row].second;
            CHECK(record.symbol.units == u"IBM");
            REQUIRE(record.typedValues.size() == 3);
            const auto &values = record.typedValues;
            CHECK(values[2].raw == sequence + static_cast<std::int64_t>(row));
            CHECK(values[0].units.has_value() == (row != 2));
            CHECK(values[1].bytes.has_value() == (row != 2));

            if (row == 1) {
                CHECK(values[0].units->empty());
                CHECK(values[1].bytes->empty());
            } else if (row != 2) {
                const std::u16string expected{static_cast<char16_t>(producer + 1), 0,
                                              static_cast<char16_t>(0xd800 + call)};
                CHECK(*values[0].units == expected);
                const std::array expectedBytes{static_cast<std::byte>(producer), static_cast<std::byte>(call),
                                               static_cast<std::byte>(row), std::byte{0xff}};
                CHECK(std::ranges::equal(*values[1].bytes, expectedBytes));
            }
        }
    }

    for (const auto calls : nextCall) {
        CHECK(calls == CALLS);
    }

    (*endpoint)->closeAndAwaitTermination();
    CHECK((*endpoint)->getDiagnostic().code == q::api::ErrorCode::NONE);
}

TEST_CASE("large immutable catalog is reserved once while tiny STREAM publications use an independent window") {
    using Type = c::SerialFieldType;
    constexpr c::SchemaLimits limits{16, 1024, 1024, 64, 32768};
    std::vector<c::LocalRecord> declarations;

    for (std::int32_t id = 0; id < 16; ++id) {
        auto name = std::u16string(u"Record") + static_cast<char16_t>(u'A' + id);
        c::LocalRecord record{id, std::move(name), false, {}, {}};
        const std::size_t fields = id == 0 ? 1 : 64;

        for (std::size_t field = 0; field < fields; ++field) {
            record.fields.push_back({std::to_underlying(Type::COMPACT_INT)});
            auto fieldName = std::u16string(u"LongFieldName") + static_cast<char16_t>(u'A' + field);
            record.lookups.push_back({std::move(fieldName), field});
        }

        declarations.push_back(std::move(record));
    }

    auto catalog = c::LocalSchema::create(declarations, limits, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(catalog);
    const auto schema = std::move(*catalog);
    constexpr std::size_t WINDOW = 512;
    REQUIRE(schema.storageBytes() > 32 * WINDOW);
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    q::api::PublisherOptions options;
    const auto names = fieldNames(schema);
    options.catalog.emplace(schema, names);
    options.pendingQuotes = 32;
    options.recordLimits.rowBytes = 1024;
    options.recordLimits.pendingBytes = schema.storageBytes() + 16 * 1024;
    options.streamBytes.emplace(512, 16384, WINDOW);
    options.listener.maxConnections = 1;
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, options);
    INFO((endpoint ? std::string_view{} : endpoint.error().stage));
    REQUIRE(endpoint);
    Downstream peer(schema, (*endpoint)->port(), 100, limits);
    peer.subscribe(p::SubscriptionMessage::STREAM_ADD, 0);
    REQUIRE(
        (*endpoint)->awaitDemand(c::SubscriptionFamily::STREAM, u"RecordA", u"IBM", true, Clock::now() + WAIT_TIME));
    const auto publisher = (*endpoint)->getPublisher();
    constexpr std::size_t CALLS = 80;

    for (std::size_t call = 0; call < CALLS; ++call) {
        const std::array fields{c::FieldValueView{Type::COMPACT_INT, static_cast<std::int64_t>(call), {}, {}}};
        const std::array rows{q::api::PublisherStreamRecordView{schema.recordContext(0), u"IBM", fields}};
        const auto deadline = Clock::now() + WAIT_TIME;

        for (;;) {
            const auto admitted = publisher.publishStreamRecords(rows);

            if (admitted) {
                break;
            }

            INFO(admitted.error().stage);
            REQUIRE(admitted.error().stage == "publisher-pending-records");
            REQUIRE(Clock::now() < deadline);
            peer.poll();
            std::this_thread::yield();
        }

        peer.poll();
    }

    awaitRecords(peer, CALLS, **endpoint);

    for (std::size_t row = 0; row < CALLS; ++row) {
        CHECK(peer.records()[row].second.typedValues[0].raw == static_cast<std::int64_t>(row));
    }

    (*endpoint)->closeAndAwaitTermination();
    CHECK((*endpoint)->getDiagnostic().code == q::api::ErrorCode::NONE);
    CHECK((*endpoint)->getStatistics().pendingPublications == 0);
}

TEST_CASE("whole input larger than a payload window is copied only at bounded collector selections") {
    using Type = c::SerialFieldType;
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto schema = publicationSchema();
    const auto names = fieldNames(schema);
    q::api::PublisherOptions options;
    options.catalog.emplace(schema, names);
    options.pendingQuotes = 64;
    options.recordLimits.rowBytes = 1024;
    options.streamBytes.emplace(128, 1024, 128);
    options.listener.maxConnections = 1;
    options.listener.maxWriteBytes = 1024;
    options.listener.noDelay = true;
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, options);
    REQUIRE(endpoint);
    Downstream peer(schema, (*endpoint)->port(), 100);
    peer.subscribe(p::SubscriptionMessage::STREAM_ADD, 1);
    REQUIRE(
        (*endpoint)->awaitDemand(c::SubscriptionFamily::STREAM, u"CustomFlow", u"IBM", true, Clock::now() + WAIT_TIME));
    constexpr std::size_t ROWS = 64;
    std::vector<std::byte> payload(256, std::byte{0x5c});
    std::array<std::array<c::FieldValueView, 3>, ROWS> values;
    std::array<q::api::PublisherStreamRecordView, ROWS> rows;

    for (std::size_t row = 0; row < ROWS; ++row) {
        values[row] = {c::FieldValueView{Type::UTF_CHAR_ARRAY, 0, {}, {}},
                       c::FieldValueView{Type::BYTE_ARRAY, 0, {}, payload},
                       c::FieldValueView{Type::LONG, -static_cast<std::int64_t>(row), {}, {}}};
        rows[row] = {schema.recordContext(1), u"IBM", values[row]};
    }

    REQUIRE((*endpoint)->getPublisher().publishStreamRecords(rows));
    std::ranges::fill(payload, std::byte{0x44});
    awaitRecords(peer, ROWS, **endpoint);

    for (std::size_t row = 0; row < ROWS; ++row) {
        const auto &fields = peer.records()[row].second.typedValues;
        REQUIRE(fields.size() == 3);
        CHECK_FALSE(fields[0].units);
        REQUIRE(fields[1].bytes);
        CHECK(fields[1].bytes->size() == payload.size());
        CHECK(std::ranges::all_of(*fields[1].bytes, [](std::byte byte) {
            return byte == std::byte{0x5c};
        }));
        CHECK(fields[2].raw == -static_cast<std::int64_t>(row));
    }

    (*endpoint)->closeAndAwaitTermination();
    CHECK((*endpoint)->getDiagnostic().code == q::api::ErrorCode::NONE);
    CHECK((*endpoint)->getStatistics().pendingPublications == 0);
    CHECK((*endpoint)->getStatistics().retiredPeers.value == 0);
}
