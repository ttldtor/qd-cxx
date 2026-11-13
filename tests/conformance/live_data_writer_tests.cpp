// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/protocol/subscription_writer.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace p = org::ttldtor::qdcxx::protocol;
using Type = c::SerialFieldType;
using Error = p::SubscriptionWriterError;

constexpr c::SchemaLimits SCHEMA_LIMITS{8, 32, 32, 128, 2048};
constexpr p::LayoutLimits LAYOUT_LIMITS{SCHEMA_LIMITS, 8, 32, 128, 2048};
constexpr p::SubscriptionWriterLimits OUTPUT_LIMITS{8192, 8201, 128, 128, {1024, 128}, 32768, 32768, 8};
constexpr std::int64_t TIME = 0x1234567889abcdef;
constexpr std::int32_t TIME_HIGH = 0x12345678;
constexpr std::int32_t TIME_LOW = -1985229329;
constexpr std::int32_t THIRD_VALUE = 915;

/** Supply a fresh explicit test turn.
 * @return Finite test-only credits.
 */
p::SubscriptionWriterWork turn() {
    return {100, 100000, 100000, 100000};
}

/** Validate and create a test writer with explicit schema provenance.
 * @param records Borrowed local records.
 * @param outgoing Borrowed directional layouts.
 * @param limits Explicit finite operation and storage bounds.
 * @return Usable independently owning writer.
 */
p::SubscriptionWriter writer(std::span<const c::LocalRecord> records, std::span<const p::OutgoingRecord> outgoing,
                             p::SubscriptionWriterLimits limits = OUTPUT_LIMITS) {
    auto schema = c::LocalSchema::create(records, SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    auto work = turn();
    auto result = p::SubscriptionWriter::create(*schema, outgoing, LAYOUT_LIMITS, limits, work);
    REQUIRE(result);

    return std::move(*result);
}

/** Read a pinned production Java description and DATA pair.
 * @param type Exact Java message identity.
 * @param hs Whether Java selected hs or its empty default set.
 * @return Both raw envelopes in captured order.
 */
std::vector<std::byte> oracle(std::int32_t type, bool hs) {
    const auto name = "composer-" + std::to_string(type) + (hs ? "-hs.qtp" : "-default.qtp");
    std::ifstream input(std::filesystem::path(QDCXX_LIVE_DATA_FLAGS_FIXTURES) / name, std::ios::binary);
    REQUIRE(input);
    std::vector<std::byte> result;

    for (std::istreambuf_iterator<char> it(input), end; it != end; ++it) {
        result.push_back(static_cast<std::byte>(*it));
    }

    return result;
}

TEST_CASE("outgoing DATA and type-2 are byte-exact for all Java live families and 64 flag combinations") {
    const c::LocalRecord local{
        0, u"Pair", true, {{8}, {8}, {8}}, {{u"F0", 0}, {u"F1", 1}, {u"F2", 2}}, c::IndexedTimePairInt32{0, 1}};
    const p::OutgoingRecord outgoing{0, 0, u"Pair", {{u"F0", 8, 0}, {u"F1", 8, 1}, {u"F2", 8, 2}}};
    auto encoder = writer(std::span(&local, 1), std::span(&outgoing, 1));
    const c::FieldValueView values[]{{Type::COMPACT_INT, TIME_HIGH, {}, {}},
                                     {Type::COMPACT_INT, TIME_LOW, {}, {}},
                                     {Type::COMPACT_INT, THIRD_VALUE, {}, {}}};
    auto work = turn();
    const std::int32_t senderId = 0;
    auto desc = encoder.describe(std::span(&senderId, 1), work);
    REQUIRE(desc);

    for (const bool hs : {false, true}) {
        std::array<p::LiveDataRecordView, 64> records{};

        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto flags = hs ? static_cast<std::int32_t>((i & 31) | ((i & 32) << 1)) : 0;
            records[i] = (flags & 2) != 0 ? p::LiveDataRecordView{0, u"FLAGS", {}, flags, TIME}
                                          : p::LiveDataRecordView{0, u"FLAGS", values, flags};
        }

        for (const auto type :
             {p::LiveDataMessage::TICKER_DATA, p::LiveDataMessage::STREAM_DATA, p::LiveDataMessage::HISTORY_DATA}) {
            const auto id = std::to_underlying(type);
            CAPTURE(id);
            CAPTURE(hs);
            auto output = encoder.composeData(type, records, hs, work);
            REQUIRE(output);
            const auto expected = oracle(id, hs);
            REQUIRE(expected.size() == desc->bytes().size() + output->bytes().size());
            CHECK(std::ranges::equal(desc->bytes(), std::span(expected).first(desc->bytes().size())));
            CHECK(std::ranges::equal(output->bytes(), std::span(expected).subspan(desc->bytes().size())));
        }
    }
}

TEST_CASE("DATA preserves directional sender layout order and exact nullable fields") {
    const c::LocalRecord local{
        0, u"A", false, {{4}, {10}, {137}, {9}}, {{u"n", 0}, {u"arr", 1}, {u"text", 2}, {u"raw", 3}}};
    const p::OutgoingRecord outgoing{5, 0, u"A", {{u"raw", 9, 3}, {u"text", 137, 2}, {u"arr", 10, 1}, {u"n", 4, 0}}};
    auto encoder = writer(std::span(&local, 1), std::span(&outgoing, 1));
    constexpr std::array<std::byte, 2> payload{std::byte{0}, std::byte{0xff}};
    const c::FieldValueView values[]{{Type::INT, -2, {}, {}},
                                     {Type::UTF_CHAR_ARRAY, 0, u"", {}},
                                     {Type::STRING, 0, {}, {}},
                                     {Type::BYTE_ARRAY, 0, {}, payload}};
    const p::LiveDataRecordView record{5, u"", values};
    auto work = turn();
    auto output = encoder.composeData(p::LiveDataMessage::STREAM_DATA, std::span(&record, 1), false, work);
    REQUIRE(output);
    constexpr unsigned expected[]{12, 15, 0xfe, 5, 2, 0, 0xff, 0x7f, 0, 0xff, 0xff, 0xff, 0xfe};
    REQUIRE(output->bytes().size() == std::size(expected));
    CHECK(std::ranges::equal(output->bytes(), expected, {}, [](std::byte b) {
        return std::to_integer<unsigned>(b);
    }));
}

TEST_CASE("DATA removal rejects a leading Event field after eliding VOID") {
    const c::LocalRecord local{0, u"A", false, {{0}, {8}, {8}}, {{u"Empty", 0}, {u"EventExtra", 1}, {u"Ordinary", 2}}};
    const p::OutgoingRecord leading{5, 0, u"A", {{u"Empty", 0, 0}, {u"EventExtra", 8, 1}, {u"Ordinary", 8, 2}}};
    const p::OutgoingRecord later{5, 0, u"A", {{u"Ordinary", 8, 2}, {u"Empty", 0, 0}, {u"EventExtra", 8, 1}}};
    auto leadingWriter = writer(std::span(&local, 1), std::span(&leading, 1));
    auto laterWriter = writer(std::span(&local, 1), std::span(&later, 1));
    const p::LiveDataRecordView removal{5, u"A", {}, 2, 0};
    auto work = turn();
    constexpr auto MESSAGE = p::LiveDataMessage::STREAM_DATA;
    auto rejected = leadingWriter.composeData(MESSAGE, std::span(&removal, 1), true, work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == Error::UNSUPPORTED_DATA_REMOVAL);
    CHECK(leadingWriter.retainedBytes() == 0);
    CHECK(laterWriter.composeData(MESSAGE, std::span(&removal, 1), true, work));
}

TEST_CASE("DATA rejection keeps prior output and work accounting intact") {
    const c::LocalRecord local{0, u"A", false, {{8}}, {{u"n", 0}}};
    const p::OutgoingRecord outgoing{5, 0, u"A", {{u"n", 8, 0}}};
    auto limits = OUTPUT_LIMITS;
    limits.outputs = 1;
    auto encoder = writer(std::span(&local, 1), std::span(&outgoing, 1), limits);
    const c::FieldValueView values[]{{Type::COMPACT_INT, 17, {}, {}}};
    auto record = p::LiveDataRecordView{5, u"test", values, 0x04};
    auto work = turn();
    constexpr auto MESSAGE = p::LiveDataMessage::HISTORY_DATA;
    auto reply = encoder.composeData(MESSAGE, std::span(&record, 1), false, work);
    REQUIRE_FALSE(reply);
    CHECK(reply.error() == Error::UNSUPPORTED_DATA_FLAGS);
    CHECK(work.operations == 99);
    CHECK(encoder.retainedBytes() == 0);

    record.eventFlags = 2;
    record.removalTime = 1;
    reply = encoder.composeData(MESSAGE, std::span(&record, 1), true, work);
    REQUIRE_FALSE(reply);
    CHECK(reply.error() == Error::INVALID_DATA);
    record.values = {};
    reply = encoder.composeData(MESSAGE, std::span(&record, 1), true, work);
    REQUIRE_FALSE(reply);
    CHECK(reply.error() == Error::INVALID_DATA);

    record.removalTime = 0;
    reply = encoder.composeData(MESSAGE, std::span(&record, 1), true, work);
    REQUIRE(reply);
    const auto retained = encoder.retainedBytes();
    REQUIRE(retained > 0);
    const auto rejected = encoder.composeData(MESSAGE, std::span(&record, 1), true, work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == Error::RESOURCE_LIMIT);
    CHECK(encoder.retainedBytes() == retained);
    REQUIRE(reply->consume(retained));
    CHECK(reply->pendingBytes().empty());
    CHECK(encoder.retainedBytes() == retained);
}
} // namespace
