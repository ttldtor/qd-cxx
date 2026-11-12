// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/order_book_model.hpp"

#include "qd/events/event_flags.hpp"
#include "qd/events/time_helpers.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>
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

TEST_CASE("scope filtering recomputes fallback per exact source and applies per-side depth and lots") {
    api::OrderBookModel book;
    const auto source = events::OrderSource::NTV.id();
    const auto buy = events::Side::BUY;
    const auto sell = events::Side::SELL;
    const auto composite = events::Scope::COMPOSITE;
    const auto regional = events::Scope::REGIONAL;
    const auto aggregate = events::Scope::AGGREGATE;
    const auto individual = events::Scope::ORDER;
    auto makerA = order(4, 102, 2, buy, aggregate, 0, u'Q');
    makerA.marketMaker = u"A";
    auto makerB = order(5, 101, 3, buy, aggregate, 0, u'Q');
    makerB.marketMaker = u"B";
    auto detail = order(orderIndex(source, 1), 103, 4, buy, individual, 0, u'Q');
    detail.marketMaker = u"A";
    const std::array local{
        order(1, 105, 0, buy, composite),
        order(2, 104, 1, buy, regional, 0, u'Q'),
        order(3, 100, 7, buy, regional, 0, u'Z'),
        makerA,
        makerB,
        order(6, 110, 2, sell, composite),
    };
    const std::array external{
        detail,
        order(orderIndex(source, 2), 111, 1, sell, individual, 0, u'Q'),
    };

    REQUIRE(book.processEvents(u"IBM", 0, local));
    REQUIRE(book.processEvents(u"IBM", source, external));

    const auto all = book.getFilteredSnapshot(u"IBM", {});
    REQUIRE(all);
    CHECK(indices(all->buyOrders) == std::vector<std::int64_t>{detail.index, 5, 3});
    CHECK(indices(all->sellOrders) == std::vector<std::int64_t>{external[1].index});

    const auto compositeOnly = book.getFilteredSnapshot(u"IBM", {.filter = api::OrderBookModelFilter::COMPOSITE});
    REQUIRE(compositeOnly);
    CHECK(indices(compositeOnly->buyOrders) == std::vector<std::int64_t>{1});
    CHECK(indices(compositeOnly->sellOrders) == std::vector<std::int64_t>{6});

    const auto regionalOnly = book.getFilteredSnapshot(u"IBM", {.filter = api::OrderBookModelFilter::REGIONAL});
    REQUIRE(regionalOnly);
    CHECK(indices(regionalOnly->buyOrders) == std::vector<std::int64_t>{2, 3});
    CHECK(regionalOnly->sellOrders.empty());

    const auto aggregateOnly = book.getFilteredSnapshot(u"IBM", {.filter = api::OrderBookModelFilter::AGGREGATE});
    REQUIRE(aggregateOnly);
    CHECK(indices(aggregateOnly->buyOrders) == std::vector<std::int64_t>{4, 5});

    const auto orderOnly = book.getFilteredSnapshot(u"IBM", {.filter = api::OrderBookModelFilter::ORDER});
    REQUIRE(orderOnly);
    CHECK(indices(orderOnly->buyOrders) == std::vector<std::int64_t>{detail.index});

    const auto quotes = book.getFilteredSnapshot(u"IBM", {.filter = api::OrderBookModelFilter::COMPOSITE_REGIONAL});
    REQUIRE(quotes);
    CHECK(indices(quotes->buyOrders) == std::vector<std::int64_t>{2, 3});
    CHECK(indices(quotes->sellOrders) == std::vector<std::int64_t>{6});

    const auto summaries =
        book.getFilteredSnapshot(u"IBM", {.filter = api::OrderBookModelFilter::COMPOSITE_REGIONAL_AGGREGATE});
    REQUIRE(summaries);
    CHECK(indices(summaries->buyOrders) == std::vector<std::int64_t>{4, 5, 3});
    CHECK(indices(summaries->sellOrders) == std::vector<std::int64_t>{6});

    const auto localTop = book.getFilteredSnapshot(u"IBM", {.sourceId = 0, .depth = 1, .lotSize = 100});
    REQUIRE(localTop);
    REQUIRE(localTop->buyOrders.size() == 1);
    REQUIRE(localTop->sellOrders.size() == 1);
    CHECK(localTop->buyOrders.front().getIndex() == 4);
    CHECK(localTop->buyOrders.front().getSizeAsDouble() == 200);
    CHECK(localTop->buyOrders.front().size.raw == makerA.size.raw);
    CHECK(localTop->sellOrders.front().getSizeAsDouble() == 200);

    const auto allScaled = book.getFilteredSnapshot(u"IBM", {.depth = 1, .lotSize = 100});
    REQUIRE(allScaled);
    REQUIRE(allScaled->buyOrders.size() == 1);
    CHECK(allScaled->buyOrders.front().getIndex() == detail.index);
    CHECK(allScaled->buyOrders.front().getSizeAsDouble() == 4);

    const auto none = book.getFilteredSnapshot(u"IBM", {.depth = 0});
    REQUIRE(none);
    CHECK(none->buyOrders.empty());
    CHECK(none->sellOrders.empty());

    const auto invalidLot = book.getFilteredSnapshot(u"IBM", {.lotSize = 0});
    REQUIRE_FALSE(invalidLot);
    CHECK(invalidLot.error() == api::IndexedEventModelError::INVALID_OPTIONS);
    const auto invalidFilter = book.getFilteredSnapshot(u"IBM", {.filter = static_cast<api::OrderBookModelFilter>(99)});
    REQUIRE_FALSE(invalidFilter);
    CHECK(invalidFilter.error() == api::IndexedEventModelError::INVALID_OPTIONS);

    const auto retained = book.getFilteredSnapshot(u"IBM", {.sourceId = 0, .depth = 1});
    REQUIRE(retained);
    REQUIRE(retained->buyOrders.size() == 1);
    CHECK(retained->buyOrders.front().getSizeAsDouble() == 2);
}

TEST_CASE("corrections publish only completed aggregates and keep tombstones until fresh or removed") {
    api::OrderBookModel book({}, {.policy = api::OrderBookCorrectionPolicy::SAME_SYMBOL});
    const auto source = events::OrderSource::NTV.id();
    auto old = order(1, 100, 2, events::Side::BUY, events::Scope::AGGREGATE);
    old.timeSequence = events::withTimeSequenceTime(0, 1000);
    auto newer = order(orderIndex(source, 1), 100, 3, events::Side::BUY, events::Scope::AGGREGATE);
    newer.timeSequence = events::withTimeSequenceTime(0, 1100);
    newer.eventFlags = indexed::TX_PENDING;

    REQUIRE(book.processEvents(u"IBM", 0, std::span(&old, 1)));
    REQUIRE(book.processEvents(u"IBM", source, std::span(&newer, 1)));
    const auto held = book.getSnapshot(u"IBM");
    REQUIRE(held);
    CHECK(indices(held->buyOrders) == std::vector<std::int64_t>{old.index});

    auto end = order(orderIndex(source, 2), 0, 0);
    end.eventFlags = indexed::REMOVE_EVENT;
    REQUIRE(book.processEvents(u"IBM", source, std::span(&end, 1)));
    const auto corrected = book.getSnapshot(u"IBM");
    REQUIRE(corrected);
    CHECK(indices(corrected->buyOrders) == std::vector<std::int64_t>{newer.index});
    const auto exactSource = book.getFilteredSnapshot(u"IBM", {.sourceId = 0});
    REQUIRE(exactSource);
    CHECK(exactSource->buyOrders.empty());

    auto moved = std::move(book);
    const auto empty = book.getSnapshot(u"IBM");
    REQUIRE(empty);
    CHECK(empty->buyOrders.empty());
    newer.eventFlags = indexed::REMOVE_EVENT;
    REQUIRE(moved.processEvents(u"IBM", source, std::span(&newer, 1)));
    REQUIRE(moved.processEvents(u"IBM", 0, std::span(&old, 1)));
    const auto suppressed = moved.getSnapshot(u"IBM");
    REQUIRE(suppressed);
    CHECK(suppressed->buyOrders.empty());

    old.timeSequence = events::withTimeSequenceTime(0, 1200);
    REQUIRE(moved.processEvents(u"IBM", 0, std::span(&old, 1)));
    const auto refreshed = moved.getSnapshot(u"IBM");
    REQUIRE(refreshed);
    CHECK(indices(refreshed->buyOrders) == std::vector<std::int64_t>{old.index});
    CHECK(indices(held->buyOrders) == std::vector<std::int64_t>{old.index});
    CHECK(indices(corrected->buyOrders) == std::vector<std::int64_t>{newer.index});
}

TEST_CASE("corrections honor exact crossing and age boundaries and reject late stale values") {
    using namespace std::chrono_literals;
    api::OrderBookModel book(
        {}, {.policy = api::OrderBookCorrectionPolicy::SAME_SYMBOL, .keepAge = 500ms, .flipAge = 100ms});
    const auto timed = [](std::int64_t index, std::int64_t price, events::Side side, std::int64_t time) {
        return order(index, price, 1, side, events::Scope::AGGREGATE, events::withTimeSequenceTime(0, time));
    };
    const auto bid = timed(1, 105, events::Side::BUY, 1000);
    const auto justEarly = timed(2, 103, events::Side::SELL, 1099);
    const auto exactFlip = timed(3, 104, events::Side::SELL, 1100);
    const auto exactKeep = timed(4, 90, events::Side::BUY, 1599);
    const auto stale = timed(5, 95, events::Side::BUY, 1000);

    REQUIRE(book.processEvents(u"IBM", 0, std::span(&bid, 1)));
    REQUIRE(book.processEvents(u"IBM", 0, std::span(&justEarly, 1)));
    const auto crossedBefore = book.getSnapshot(u"IBM");
    REQUIRE(crossedBefore);
    CHECK(indices(crossedBefore->buyOrders) == std::vector<std::int64_t>{1});
    CHECK(indices(crossedBefore->sellOrders) == std::vector<std::int64_t>{2});

    REQUIRE(book.processEvents(u"IBM", 0, std::span(&exactFlip, 1)));
    const auto flipped = book.getSnapshot(u"IBM");
    REQUIRE(flipped);
    CHECK(flipped->buyOrders.empty());
    CHECK(indices(flipped->sellOrders) == std::vector<std::int64_t>{2, 3});

    REQUIRE(book.processEvents(u"IBM", 0, std::span(&exactKeep, 1)));
    REQUIRE(book.processEvents(u"IBM", 0, std::span(&stale, 1)));
    const auto aged = book.getSnapshot(u"IBM");
    REQUIRE(aged);
    CHECK(indices(aged->buyOrders) == std::vector<std::int64_t>{4});
    CHECK(indices(aged->sellOrders) == std::vector<std::int64_t>{3});

    CHECK_THROWS_AS((api::OrderBookModel({}, {.keepAge = -1ms})), std::invalid_argument);
}

TEST_CASE("correction budget refusal rolls back indexed values and an existing unfinished transaction") {
    api::OrderBookModel book({}, {.policy = api::OrderBookCorrectionPolicy::SAME_SYMBOL, .entries = 1});
    const auto first = order(1, 100, 1, events::Side::BUY, events::Scope::AGGREGATE);
    REQUIRE(book.processEvents(u"IBM", 0, std::span(&first, 1)));

    auto remove = first;
    remove.eventFlags = indexed::REMOVE_EVENT | indexed::TX_PENDING;
    REQUIRE(book.processEvents(u"IBM", 0, std::span(&remove, 1)));
    const auto second = order(2, 102, 1, events::Side::BUY, events::Scope::AGGREGATE);
    const auto third = order(3, 104, 1, events::Side::SELL, events::Scope::AGGREGATE);
    const std::array overBudget{second, third};
    const auto refused = book.processEvents(u"IBM", 0, overBudget);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == api::IndexedEventModelError::RESOURCE_LIMIT);
    const auto before = book.getSnapshot(u"IBM");
    REQUIRE(before);
    CHECK(indices(before->buyOrders) == std::vector<std::int64_t>{1});
    CHECK(before->sellOrders.empty());

    REQUIRE(book.processEvents(u"IBM", 0, std::span(&second, 1)));
    const auto after = book.getSnapshot(u"IBM");
    REQUIRE(after);
    CHECK(indices(after->buyOrders) == std::vector<std::int64_t>{2});
    CHECK(after->sellOrders.empty());
}
