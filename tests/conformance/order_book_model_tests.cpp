// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/order_book_model.hpp"

#include "qd/events/event_flags.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

namespace {

namespace q = org::ttldtor::qdcxx;
namespace api = q::api;
namespace events = q::events;
namespace indexed = q::events::indexed_event_flags;

constexpr unsigned ORDER_SOURCE_SHIFT = 32;
constexpr unsigned SPECIAL_SOURCE_SHIFT = 48;
constexpr unsigned EXCHANGE_SHIFT = 32;
constexpr unsigned SIDE_FLAG_SHIFT = 2;
constexpr unsigned EXCHANGE_FLAG_SHIFT = 4;
constexpr std::int64_t WIDE_DECIMAL_SIGNIFICAND_SCALE = 256;
constexpr std::int64_t WIDE_DECIMAL_INTEGER_RANK = 128;

/** Compose small integral test values as exact rank-128 decimals.
 * @param value Small integer with representable scaled significand.
 * @return Exact wide decimal.
 */
q::core::WideDecimal decimal(std::int64_t value) {
    return {value * WIDE_DECIMAL_SIGNIFICAND_SCALE + WIDE_DECIMAL_INTEGER_RANK};
}

/** Compose a normal-source packed identity.
 * @param source Nonnegative test source code.
 * @param index Exact low32 wire index.
 * @return Full source and record index.
 */
std::int64_t orderIndex(std::int32_t source, std::uint32_t index) {
    return (static_cast<std::int64_t>(source) << ORDER_SOURCE_SHIFT) | index;
}

/** Compose an order on the common IBM symbol.
 * @param index Full signed identity.
 * @param price Exact integral business price.
 * @param size Exact integral business size.
 * @param side Business side.
 * @param scope Business scope.
 * @param timeSequence Packed event ordering key.
 * @param exchange Business exchange code.
 * @return Independent test order.
 */
events::Order order(std::int64_t index, std::int64_t price, std::int64_t size, events::Side side = events::Side::BUY,
                    events::Scope scope = events::Scope::ORDER, std::int64_t timeSequence = 0, char16_t exchange = 0) {
    events::Order result;
    result.symbol = u"IBM";
    result.index = index;
    result.price = decimal(price);
    result.size = decimal(size);
    result.timeSequence = timeSequence;
    result.flags = std::to_underlying(scope) | (std::to_underlying(side) << SIDE_FLAG_SHIFT) |
                   (static_cast<std::int32_t>(exchange) << EXCHANGE_FLAG_SHIFT);

    return result;
}

/** Project exact identities for a concise order assertion.
 * @param orders One detached side.
 * @return Identities in that side's published order.
 */
std::vector<std::int64_t> indices(const std::vector<events::Order> &orders) {
    std::vector<std::int64_t> result;

    for (const auto &value : orders) {
        result.push_back(value.getIndex());
    }

    return result;
}

} // namespace

TEST_CASE("book sorts both sides across sources using business price and reference tie breaks") {
    api::OrderBookModel book;
    const auto source = events::OrderSource::NTV.id();
    const auto external = [source](std::uint32_t index) {
        return orderIndex(source, index);
    };
    const auto buy = events::Side::BUY;
    const auto sell = events::Side::SELL;
    const auto individual = events::Scope::ORDER;
    const auto aggregate = events::Scope::AGGREGATE;
    const auto regional = events::Scope::REGIONAL;
    auto missingPrice = order(14, 0, 1);
    missingPrice.price = {0};
    auto makerA = order(11, 100, 20, buy, aggregate, 2, u'Q');
    makerA.marketMaker = u"A";
    auto makerB = order(10, 100, 20, buy, aggregate, 2, u'Q');
    makerB.marketMaker = u"B";
    const std::array local{
        order(1, 100, 100, buy, individual, 5),
        order(2, 100, 1, buy, individual, 2),
        order(3, 100, 10, buy, aggregate, 1),
        order(4, 100, 20, buy, aggregate, 2, u'Q'),
        order(5, 100, 20, buy, regional, 2, u'Z'),
        order(6, 100, 20, buy, aggregate, 2, u'A'),
        order(7, 100, 20, buy, aggregate, 1, u'Q'),
        order(8, 100, 0, buy, events::Scope::COMPOSITE),
        order(9, 101, 1, sell),
        makerB,
        makerA,
        order(12, 110, 0, buy, aggregate),
        order(13, 104, 1, events::Side::UNDEFINED),
        missingPrice,
    };
    const std::array other{
        order(external(1), 105, 1),
        order(external(2), 100, 1, buy, individual, 2),
        order(external(3), 99, 1, sell),
    };
    auto foreign = order(3, 1000, 1);
    foreign.symbol = u"OTHER";

    REQUIRE(book.processEvents(u"IBM", 0, local));
    REQUIRE(book.processEvents(u"IBM", source, other));
    REQUIRE(book.processEvents(u"OTHER", 0, std::span(&foreign, 1)));
    const auto snapshot = book.getSnapshot(u"IBM");
    REQUIRE(snapshot);
    CHECK(indices(snapshot->buyOrders) ==
          std::vector<std::int64_t>{external(1), 7, 5, 6, 4, 11, 10, 3, 8, 2, external(2), 1, 14});
    CHECK(indices(snapshot->sellOrders) == std::vector<std::int64_t>{external(3), 9, 13});
}

TEST_CASE("book publishes only completed source changes and leaves detached views intact") {
    api::OrderBookModel book;
    const auto source = events::OrderSource::NTV.id();
    const auto original = order(1, 90, 1);
    auto removed = original;
    removed.eventFlags = indexed::TX_PENDING | indexed::REMOVE_EVENT;

    REQUIRE(book.processEvents(u"IBM", 0, std::span(&original, 1)));
    REQUIRE(book.processEvents(u"IBM", 0, std::span(&removed, 1)));
    const auto external = order(orderIndex(source, 1), 99, 2);
    REQUIRE(book.processEvents(u"IBM", source, std::span(&external, 1)));

    const auto before = book.getSnapshot(u"IBM");
    REQUIRE(before);
    CHECK(indices(before->buyOrders) == std::vector<std::int64_t>{external.index, original.index});
    CHECK(before->sellOrders.empty());

    const auto sell = order(2, 101, 3, events::Side::SELL);
    auto wrong = order(0, 200, 1);
    wrong.symbol = u"OTHER";
    const std::array invalid{sell, wrong};
    const auto refused = book.processEvents(u"IBM", 0, invalid);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == api::IndexedEventModelError::WRONG_KEY);
    const auto stillPending = book.getSnapshot(u"IBM");
    REQUIRE(stillPending);
    CHECK(indices(stillPending->buyOrders) == indices(before->buyOrders));
    CHECK(stillPending->sellOrders.empty());

    REQUIRE(book.processEvents(u"IBM", 0, std::span(&sell, 1)));
    const auto after = book.getSnapshot(u"IBM");
    REQUIRE(after);
    CHECK(indices(after->buyOrders) == std::vector<std::int64_t>{external.index});
    CHECK(indices(after->sellOrders) == std::vector<std::int64_t>{sell.index});

    auto zero = external;
    zero.size = decimal(0);
    REQUIRE(book.processEvents(u"IBM", source, std::span(&zero, 1)));
    const auto noExternal = book.getSnapshot(u"IBM");
    REQUIRE(noExternal);
    CHECK(noExternal->buyOrders.empty());
    book.clear();
    const auto cleared = book.getSnapshot(u"IBM");
    REQUIRE(cleared);
    CHECK(cleared->sellOrders.empty());
    CHECK(indices(before->buyOrders) == std::vector<std::int64_t>{external.index, original.index});
    CHECK(indices(after->sellOrders) == std::vector<std::int64_t>{sell.index});
}

TEST_CASE("legacy end marker requires an empty exchange for special sources") {
    api::OrderBookModel book;
    const auto source = events::OrderSource::REGIONAL.id();
    const auto base = static_cast<std::int64_t>(source) << SPECIAL_SOURCE_SHIFT;
    const auto exchange = static_cast<std::int64_t>(u'Q') << EXCHANGE_SHIFT;
    const auto old = order(base | 5, 91, 1);
    auto start = order(base | exchange | 3, 100, 1);
    start.eventFlags = indexed::SNAPSHOT_BEGIN;
    auto regionalZero = order(base | exchange, 100, 0);
    regionalZero.eventFlags = indexed::REMOVE_EVENT;

    REQUIRE(book.processEvents(u"IBM", source, std::span(&old, 1)));
    const std::array partial{start, regionalZero};
    REQUIRE(book.processEvents(u"IBM", source, partial));
    const auto oldCut = book.getSnapshot(u"IBM");
    REQUIRE(oldCut);
    CHECK(indices(oldCut->buyOrders) == std::vector<std::int64_t>{old.index});

    auto end = order(base, 0, 0);
    end.eventFlags = indexed::REMOVE_EVENT | indexed::TX_PENDING;
    REQUIRE(book.processEvents(u"IBM", source, std::span(&end, 1)));
    CHECK(end.eventFlags == (indexed::REMOVE_EVENT | indexed::TX_PENDING));
    const auto held = book.getSnapshot(u"IBM");
    REQUIRE(held);
    CHECK(indices(held->buyOrders) == std::vector<std::int64_t>{old.index});

    const auto finish = order(base | exchange | 2, 101, 1);
    REQUIRE(book.processEvents(u"IBM", source, std::span(&finish, 1)));
    const auto replacement = book.getSnapshot(u"IBM");
    REQUIRE(replacement);
    CHECK(indices(replacement->buyOrders) == std::vector<std::int64_t>{finish.index, start.index});

    const auto generic = events::OrderSource::NTV.id();
    auto genericStart = order(orderIndex(generic, 3), 104, 1);
    genericStart.eventFlags = indexed::SNAPSHOT_BEGIN;
    auto genericEnd = order(orderIndex(generic, 0), 0, 0);
    genericEnd.eventFlags = indexed::REMOVE_EVENT;
    const std::array genericSnapshot{genericStart, genericEnd};
    REQUIRE(book.processEvents(u"IBM", generic, genericSnapshot));
    const auto combined = book.getSnapshot(u"IBM");
    REQUIRE(combined);
    CHECK(indices(combined->buyOrders) == std::vector<std::int64_t>{genericStart.index, finish.index, start.index});
}

TEST_CASE("book bounds reject a batch without changing either side or pending events") {
    api::OrderBookModel book(
        {.partitions = 1, .symbolUnits = 3, .inputEvents = 2, .pendingEvents = 1, .retainedEvents = 1});
    const auto first = order(1, 90, 1);
    REQUIRE(book.processEvents(u"IBM", 0, std::span(&first, 1)));

    auto remove = first;
    remove.eventFlags = indexed::REMOVE_EVENT | indexed::TX_PENDING;
    REQUIRE(book.processEvents(u"IBM", 0, std::span(&remove, 1)));
    const auto second = order(2, 99, 2, events::Side::SELL);
    const auto third = order(3, 95, 1);
    const std::array oversized{second, third};
    const auto refused = book.processEvents(u"IBM", 0, oversized);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == api::IndexedEventModelError::RESOURCE_LIMIT);

    const auto before = book.getSnapshot(u"IBM");
    REQUIRE(before);
    CHECK(indices(before->buyOrders) == std::vector<std::int64_t>{first.index});
    CHECK(before->sellOrders.empty());

    REQUIRE(book.processEvents(u"IBM", 0, std::span(&second, 1)));
    const auto after = book.getSnapshot(u"IBM");
    REQUIRE(after);
    CHECK(after->buyOrders.empty());
    CHECK(indices(after->sellOrders) == std::vector<std::int64_t>{second.index});

    const auto longSymbol = book.getSnapshot(u"LONG");
    REQUIRE_FALSE(longSymbol);
    CHECK(longSymbol.error() == api::IndexedEventModelError::RESOURCE_LIMIT);
}
