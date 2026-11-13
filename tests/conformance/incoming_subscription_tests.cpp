// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/protocol/incoming_subscription.hpp"

#include <doctest/doctest.h>

#include <array>
#include <limits>
#include <utility>

namespace {
namespace p = org::ttldtor::qdcxx::protocol;
namespace c = org::ttldtor::qdcxx::core;
using Error = p::IncomingSubscriptionError;
using Message = p::SubscriptionMessage;

/// Synthetic peer IDs deliberately differ from local positions.
constexpr std::array MAPPING = {p::IncomingSubscriptionRecord{4, std::nullopt}, p::IncomingSubscriptionRecord{6, 1},
                                p::IncomingSubscriptionRecord{9, 0}};
/// Finite allowances for the fixtures and local schema in this file.
constexpr p::IncomingSubscriptionLimits LIMITS{64, 2, 3, 8, 8, 24};
/// The exact wire time vector used by Java's BinaryQTPComposer.
constexpr std::array BOUNDS = {std::numeric_limits<std::int64_t>::min(), std::int64_t{-65}, std::int64_t{0},
                               std::int64_t{64}, std::numeric_limits<std::int64_t>::max()};

/** Provide finite credits independent of other tests.
 * @return A fresh budget for multiple frame attempts.
 */
p::IncomingSubscriptionWork turn() {
    return {100, 1000, 1000, 1000};
}

/** Build two local targets, one with explicit indexed time metadata.
 * @return Validated local schema with distinct provenance.
 */
c::LocalSchema schema() {
    const c::LocalRecord records[] = {
        {0, u"Quote", false, {}, {}},
        {1, u"Order", true, {{8}, {8}}, {{u"hi", 0}, {u"lo", 1}}, c::IndexedTimePairInt32{0, 1}}};
    auto result = c::LocalSchema::create(records, {2, 2, 2, 8, 16}, c::LocalProfile::BINARY_FIELDS_V1);

    REQUIRE(result);

    return std::move(*result);
}

/** Convert lowercase fixture hex into raw frame bytes.
 * @param hex Pairs of lowercase hex digits without separators.
 * @return Exact bytes in input order.
 */
std::vector<std::byte> bytes(std::string_view hex) {
    const std::string_view digits = "0123456789abcdef";
    std::vector<std::byte> result;

    for (std::size_t i = 0; i < hex.size(); i += 2) {
        result.push_back(static_cast<std::byte>((digits.find(hex[i]) << 4) | digits.find(hex[i + 1])));
    }

    return result;
}

/** Construct a malformed or synthetic complete frame without involving the framer.
 * @param message Explicit outer subscription identity.
 * @param hex Lowercase payload hex only.
 * @return Owned envelope with the supplied payload.
 */
p::Frame frame(Message message, std::string_view hex) {
    return {p::FrameKind::MESSAGE, static_cast<std::int32_t>(message), bytes(hex), 0, 0};
}
} // namespace

TEST_CASE("Java subscription frames publish only on completion at every byte split") {
    const std::pair<Message, std::string_view> captures[] = {
        {Message::TICKER_ADD, "0c0b244d06ff06ff06ff06ff06"},
        {Message::TICKER_REMOVE, "0c0c244d06ff06ff06ff06ff06"},
        {Message::STREAM_ADD, "0c10244d06ff06ff06ff06ff06"},
        {Message::STREAM_REMOVE, "0c11244d06ff06ff06ff06ff06"},
        {Message::HISTORY_ADD, "2315244d06ff8000000000000000ff06bfbfff0600ff068040ff06ff7fffffffffffffff"},
        {Message::HISTORY_REMOVE, "0c16244d06ff06ff06ff06ff06"}};
    const auto local = schema();

    for (const auto &[message, hex] : captures) {
        const auto wire = bytes(hex);
        const std::span<const std::byte> input(wire);

        for (std::size_t split = 0; split < input.size(); ++split) {
            p::FrameReader reader({64, 73});
            const auto partial = reader.feed(input.first(split));

            REQUIRE(partial.status == p::FrameStatus::NEED_INPUT);
            CHECK(partial.accepted == split);
            CHECK_FALSE(partial.frame);

            const auto ready = reader.feed(input.subspan(split));

            REQUIRE(ready.status == p::FrameStatus::FRAME_READY);
            REQUIRE(ready.frame);
            CHECK(ready.accepted == input.size() - split);

            auto work = turn();
            const auto decoded = p::decodeIncomingSubscription(*ready.frame, local, MAPPING, LIMITS, work);

            REQUIRE(decoded);
            CHECK(decoded->message == message);
            REQUIRE(decoded->entries.size() == BOUNDS.size());

            for (std::size_t i = 0; i < BOUNDS.size(); ++i) {
                const auto &entry = decoded->entries[i];

                CHECK(entry.senderId == 6);
                CHECK(entry.context.matches(local.recordContext(1)));
                CHECK(entry.symbol == u"IBM");

                if (message == Message::HISTORY_ADD) {
                    CHECK(entry.historyBound == BOUNDS[i]);
                } else {
                    CHECK_FALSE(entry.historyBound);
                }
            }
        }
    }
}

TEST_CASE("Syntax and mapping failures reject an entire subscription frame") {
    const auto local = schema();
    auto work = turn();

    const std::pair<std::string_view, Error> streamFailures[] = {
        {"244d06ff", Error::TRUNCATED_PAYLOAD},    {"244d06c0", Error::INVALID_SYMBOL},
        {"244d06ff07", Error::UNKNOWN_RECORD},     {"244d06ff7f", Error::UNKNOWN_RECORD},
        {"244d06fd012a06", Error::INVALID_SYMBOL}, {"ff06", Error::INVALID_SYMBOL}};

    for (const auto &[hex, error] : streamFailures) {
        CHECK(p::decodeIncomingSubscription(frame(Message::STREAM_ADD, hex), local, MAPPING, LIMITS, work) ==
              std::unexpected(error));
    }

    CHECK(p::decodeIncomingSubscription(frame(Message::HISTORY_ADD, "244d0600ff06ff"), local, MAPPING, LIMITS, work) ==
          std::unexpected(Error::TRUNCATED_PAYLOAD));
    CHECK(p::decodeIncomingSubscription(frame(Message::HISTORY_REMOVE, "244d06ff09"), local, MAPPING, LIMITS, work) ==
          std::unexpected(Error::INVALID_HISTORY));
    CHECK(p::decodeIncomingSubscription(frame(Message::STREAM_ADD, "244d06"), local, {}, LIMITS, work) ==
          std::unexpected(Error::UNKNOWN_RECORD));
    CHECK(p::decodeIncomingSubscription(frame(static_cast<Message>(15), ""), local, MAPPING, LIMITS, work) ==
          std::unexpected(Error::INVALID_MESSAGE));

    const std::array duplicate = {MAPPING[1], p::IncomingSubscriptionRecord{6, 0}};
    const std::array invalid = {p::IncomingSubscriptionRecord{6, 2}};

    CHECK(p::decodeIncomingSubscription(frame(Message::TICKER_ADD, ""), local, duplicate, LIMITS, work) ==
          std::unexpected(Error::INVALID_MAPPING));
    CHECK(p::decodeIncomingSubscription(frame(Message::TICKER_ADD, ""), local, invalid, LIMITS, work) ==
          std::unexpected(Error::INVALID_MAPPING));

    const auto next = p::decodeIncomingSubscription(frame(Message::STREAM_ADD, "fe09"), local, MAPPING, LIMITS, work);

    REQUIRE(next);
    REQUIRE(next->entries.size() == 1);
    CHECK(next->entries[0].symbol.empty());
}

TEST_CASE("Framed ingress preserves the last completed batch when the next complete frame is invalid") {
    const auto local = schema();
    const auto wire = bytes("0410244d060510244d06ff");
    const std::span<const std::byte> input(wire);
    p::FrameReader reader({64, 73});
    auto work = turn();
    const auto first = reader.feed(input);

    REQUIRE(first.frame);
    CHECK(first.accepted == 5);

    const auto completed = p::decodeIncomingSubscription(*first.frame, local, MAPPING, LIMITS, work);
    const auto second = reader.feed(input.subspan(first.accepted));

    REQUIRE(completed);
    REQUIRE(second.frame);
    CHECK(second.accepted == input.size() - first.accepted);
    CHECK(p::decodeIncomingSubscription(*second.frame, local, MAPPING, LIMITS, work) ==
          std::unexpected(Error::TRUNCATED_PAYLOAD));
    REQUIRE(completed->entries.size() == 1);
    CHECK(completed->entries[0].symbol == u"IBM");
    CHECK(reader.finish() == p::FrameStatus::FINISHED);
}

TEST_CASE("Ignored records still consume a full tuple and establish within-frame repetition") {
    auto local = schema();
    auto work = turn();
    auto input = frame(Message::HISTORY_ADD, "f901244d047fff0600");
    const auto context = local.recordContext(1);
    const auto result = p::decodeIncomingSubscription(input, local, MAPPING, LIMITS, work);

    REQUIRE(result);

    input.payload.clear();
    local = schema();

    REQUIRE(result->entries.size() == 1);
    CHECK(result->entries.front().symbol == u"IBM");
    CHECK(result->entries.front().historyBound == 0);
    CHECK(result->entries.front().context.matches(context));
    CHECK_FALSE(result->entries.front().context.matches(local.recordContext(1)));
}

TEST_CASE("Storage and cumulative work limits apply to tuples that never publish") {
    const auto local = schema();
    const auto input = frame(Message::STREAM_REMOVE, "244d04ff06");
    auto limits = LIMITS;
    auto work = turn();

    for (const auto member :
         {&p::IncomingSubscriptionLimits::payloadBytes, &p::IncomingSubscriptionLimits::schemaRecords,
          &p::IncomingSubscriptionLimits::senderRecords, &p::IncomingSubscriptionLimits::entries,
          &p::IncomingSubscriptionLimits::symbolUnits, &p::IncomingSubscriptionLimits::totalSymbolUnits}) {
        limits.*member = 1;

        CHECK(p::decodeIncomingSubscription(input, local, MAPPING, limits, work) ==
              std::unexpected(Error::RESOURCE_LIMIT));

        limits = LIMITS;
    }

    limits.totalSymbolUnits = 5;

    CHECK(p::decodeIncomingSubscription(input, local, MAPPING, limits, work) == std::unexpected(Error::RESOURCE_LIMIT));

    work = {2, 10, 20, 15};

    CHECK(p::decodeIncomingSubscription(input, local, MAPPING, LIMITS, work) == std::unexpected(Error::WORK_LIMIT));
    CHECK(work.frames == 1);
    CHECK(work.inputBytes == 5);
    CHECK(work.textUnits == 0);
    CHECK(p::decodeIncomingSubscription(input, local, MAPPING, LIMITS, work) == std::unexpected(Error::WORK_LIMIT));
    CHECK(work.frames == 0);
    CHECK(work.inputBytes == 0);

    work = {1, 64, MAPPING.size(), 100};

    CHECK(p::decodeIncomingSubscription(input, local, MAPPING, LIMITS, work) == std::unexpected(Error::WORK_LIMIT));
    CHECK(work.visits == 0);
}
