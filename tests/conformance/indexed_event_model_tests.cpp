// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/indexed_event_model.hpp"

#include "qd/events/candle.hpp"
#include "qd/events/order.hpp"
#include "qd/events/otc_markets_order.hpp"

#include <array>
#include <doctest/doctest.h>
#include <expected>
#include <limits>
#include <utility>

namespace {

namespace api = org::ttldtor::qdcxx::api;
namespace events = org::ttldtor::qdcxx::events;
namespace flags = events::indexed_event_flags;

template <class T>
void checkRefusal(const std::expected<T, api::IndexedEventModelError> &result, api::IndexedEventModelError reason) {
    REQUIRE_FALSE(result);
    CHECK(result.error() == reason);
}

events::Order order(std::u16string_view symbol, const events::OrderSource &source, std::int64_t index,
                    std::int32_t eventFlags = 0, std::int64_t orderId = 0) {
    events::Order event;
    event.symbol = symbol;
    event.index = events::changeOrderIndexSource(index, source);
    event.eventFlags = eventFlags;
    event.orderId = orderId;

    return event;
}

TEST_CASE("indexed model publishes only finished transactions and separates exact symbol and source") {
    api::IndexedEventModel<events::Order> model;
    const auto &primary = events::OrderSource::DEFAULT;
    const auto &secondary = events::OrderSource::NTV;
    const auto opening = std::array{order(u"IBM", primary, 7, flags::TX_PENDING, 51)};
    const auto staged = model.processEvents(u"IBM", primary.id(), opening);
    REQUIRE(staged);
    CHECK(staged->empty());
    REQUIRE(model.getSnapshot(u"IBM", primary.id())->empty());

    const auto otherSource = std::array{order(u"IBM", secondary, 7, 0, 61)};
    const auto otherSymbol = std::array{order(u"IBM&Q", primary, 7, 0, 71)};
    REQUIRE(model.processEvents(u"IBM", secondary.id(), otherSource));
    REQUIRE(model.processEvents(u"IBM&Q", primary.id(), otherSymbol));
    REQUIRE(model.getSnapshot(u"IBM", primary.id())->empty());

    const auto tail = std::array{order(u"IBM", primary, 7, 0, 52), order(u"IBM", primary, 9, 0, 53)};
    const auto completed = model.processEvents(u"IBM", primary.id(), tail);
    REQUIRE(completed);
    REQUIRE(completed->size() == 1);
    CHECK(completed->front().kind == api::IndexedEventChangeKind::UPDATES);
    const auto &events = completed->front().events;
    REQUIRE(events.size() == 3);
    CHECK(events[0].orderId == 51);
    CHECK(events[0].eventFlags == flags::TX_PENDING);
    CHECK(events[1].orderId == 52);
    CHECK(events[2].orderId == 53);

    const auto snapshot = model.getSnapshot(u"IBM", primary.id());
    REQUIRE(snapshot);
    REQUIRE(snapshot->size() == 2);
    CHECK(snapshot->front().orderId == 52);
    CHECK(snapshot->front().eventFlags == 0);
    CHECK(model.getSnapshot(u"IBM", secondary.id())->at(0).orderId == 61);
    CHECK(model.getSnapshot(u"IBM&Q", primary.id())->at(0).orderId == 71);
    auto moved = std::move(model);
    CHECK(model.getSnapshot(u"IBM", primary.id())->empty());
    CHECK(moved.getSnapshot(u"IBM", primary.id())->at(0).orderId == 52);
    moved.clear();
    CHECK(moved.getSnapshot(u"IBM", primary.id())->empty());
    CHECK(snapshot->at(0).orderId == 52);
    CHECK(events[0].orderId == 51);
}

TEST_CASE("snipped snapshots wait for transaction completion and preserve replacement and insertion semantics") {
    api::IndexedEventModel<events::Order> model;
    const auto &source = events::OrderSource::DEFAULT;
    const auto original = std::array{order(u"A", source, 99, flags::SNAPSHOT_END, 99)};
    const auto strayEnd = model.processEvents(u"A", source.id(), original);
    REQUIRE(strayEnd);
    REQUIRE(strayEnd->size() == 1);
    CHECK(strayEnd->front().kind == api::IndexedEventChangeKind::UPDATES);

    const auto snapshot = std::array{order(u"A", source, 9, flags::SNAPSHOT_BEGIN, 91),
                                     order(u"A", source, 3, 0, 31),
                                     order(u"A", source, 9, 0, 92),
                                     order(u"A", source, 3, flags::REMOVE_EVENT),
                                     order(u"A", source, 8, 0, 81),
                                     order(u"A", source, 3, flags::SNAPSHOT_SNIP | flags::TX_PENDING, 32)};
    const auto staged = model.processEvents(u"A", source.id(), snapshot);
    REQUIRE(staged);
    CHECK(staged->empty());
    CHECK(model.getSnapshot(u"A", source.id())->at(0).orderId == 99);

    const auto closing = std::array{order(u"A", source, 5, 0, 51), order(u"A", source, 9, flags::REMOVE_EVENT)};
    const auto changes = model.processEvents(u"A", source.id(), closing);
    REQUIRE(changes);
    REQUIRE(changes->size() == 2);
    CHECK(changes->at(0).kind == api::IndexedEventChangeKind::SNIPPED_SNAPSHOT);
    const auto &values = changes->at(0).events;
    REQUIRE(values.size() == 4);
    CHECK(values[0].orderId == 92);
    CHECK(values[1].orderId == 81);
    CHECK(values[2].orderId == 32);
    CHECK(values[3].orderId == 51);

    for (const auto &value : values) {
        CHECK(value.eventFlags == 0);
    }

    CHECK(changes->at(1).kind == api::IndexedEventChangeKind::UPDATES);
    REQUIRE(changes->at(1).events.size() == 1);
    CHECK(changes->at(1).events[0].eventFlags == flags::REMOVE_EVENT);
    const auto current = model.getSnapshot(u"A", source.id());
    REQUIRE(current);
    REQUIRE(current->size() == 3);
    CHECK(current->at(0).index == 3);
    CHECK(current->at(1).index == 5);
    CHECK(current->at(2).index == 8);
}

TEST_CASE("snapshot begin supersedes unfinished snapshots and transactions without a partial clear") {
    api::IndexedEventModel<events::Order> model;
    const auto &source = events::OrderSource::DEFAULT;
    const auto original = std::array{order(u"A", source, 11, 0, 11)};
    REQUIRE(model.processEvents(u"A", source.id(), original));
    const auto unfinished =
        std::array{order(u"A", source, 1, flags::TX_PENDING, 1), order(u"A", source, 2, flags::SNAPSHOT_BEGIN, 2),
                   order(u"A", source, 3, flags::SNAPSHOT_END | flags::TX_PENDING, 3)};
    REQUIRE(model.processEvents(u"A", source.id(), unfinished)->empty());
    CHECK(model.getSnapshot(u"A", source.id())->at(0).index == 11);

    const auto emptySnapshot =
        std::array{order(u"A", source, 4, flags::SNAPSHOT_BEGIN | flags::SNAPSHOT_END | flags::REMOVE_EVENT)};
    const auto changes = model.processEvents(u"A", source.id(), emptySnapshot);
    REQUIRE(changes);
    REQUIRE(changes->size() == 1);
    CHECK(changes->front().kind == api::IndexedEventChangeKind::SNAPSHOT);
    CHECK(changes->front().events.empty());
    CHECK(model.getSnapshot(u"A", source.id())->empty());
}

TEST_CASE("bounded admission rejects mixed keys and restores unfinished and visible state after late failure") {
    api::IndexedEventModelLimits limits;
    limits.partitions = 2;
    limits.symbolUnits = 1;
    limits.inputEvents = 3;
    limits.pendingEvents = 1;
    limits.retainedEvents = 2;
    api::IndexedEventModel<events::Order> model(limits);
    const auto &source = events::OrderSource::DEFAULT;
    const auto original = std::array{order(u"A", source, 1, 0, 11), order(u"A", source, 1, flags::TX_PENDING, 12)};
    REQUIRE(model.processEvents(u"A", source.id(), original));
    const auto other = std::array{order(u"B", source, 1, 0, 21)};
    REQUIRE(model.processEvents(u"B", source.id(), other));

    const auto wrongKey = std::array{order(u"A", source, 1, 0, 13), order(u"B", source, 1, 0, 22)};
    checkRefusal(model.processEvents(u"A", source.id(), wrongKey), api::IndexedEventModelError::WRONG_KEY);
    const auto &otherSource = events::OrderSource::NTV;
    const auto wrongSource = std::array{order(u"A", otherSource, 1)};
    checkRefusal(model.processEvents(u"A", source.id(), wrongSource), api::IndexedEventModelError::WRONG_KEY);

    const auto overflow = std::array{order(u"A", source, 1, 0, 14), order(u"A", source, 2, 0, 15)};
    checkRefusal(model.processEvents(u"A", source.id(), overflow), api::IndexedEventModelError::RESOURCE_LIMIT);
    CHECK(model.getSnapshot(u"A", source.id())->at(0).orderId == 11);
    CHECK(model.getSnapshot(u"B", source.id())->at(0).orderId == 21);

    const auto pendingOverflow = std::array{order(u"B", source, 1, flags::TX_PENDING)};
    checkRefusal(model.processEvents(u"B", source.id(), pendingOverflow), api::IndexedEventModelError::RESOURCE_LIMIT);
    const auto replacement = std::array{order(u"A", source, 1, 0, 16)};
    const auto changes = model.processEvents(u"A", source.id(), replacement);
    REQUIRE(changes);
    REQUIRE(changes->size() == 1);
    const auto &events = changes->front().events;
    REQUIRE(events.size() == 2);
    CHECK(events[0].orderId == 12);
    CHECK(events[1].orderId == 16);

    const auto extraPartition = std::array{order(u"C", source, 1, flags::TX_PENDING)};
    checkRefusal(model.processEvents(u"C", source.id(), extraPartition), api::IndexedEventModelError::RESOURCE_LIMIT);
    checkRefusal(model.processEvents(u"AB", source.id(), {}), api::IndexedEventModelError::RESOURCE_LIMIT);
    const auto tooMany = std::array{original[0], original[0], original[0], original[0]};
    checkRefusal(model.processEvents(u"A", source.id(), tooMany), api::IndexedEventModelError::RESOURCE_LIMIT);
    const auto remove = std::array{order(u"B", source, 1, flags::REMOVE_EVENT)};
    REQUIRE(model.processEvents(u"B", source.id(), remove));
    REQUIRE(model.processEvents(u"C", source.id(), extraPartition));
    model.clear();
    CHECK(model.processEvents(u"C", source.id(), std::array{order(u"C", source, 2)})->at(0).events.size() == 1);
}

TEST_CASE("built-in Candle and wrapped Order preserve their exact public value and flag ownership") {
    api::IndexedEventModel<events::Candle> candles;
    events::Candle candle;
    const auto noSymbol = std::array{candle};
    checkRefusal(candles.processEvents(u"", 0, noSymbol), api::IndexedEventModelError::WRONG_KEY);
    candle.eventSymbol = events::CandleSymbol::valueOf(u"IBM{=m}");
    candle.index = std::numeric_limits<std::int64_t>::min();
    candle.eventFlags = flags::SNAPSHOT_BEGIN | flags::SNAPSHOT_END;
    const auto key = candle.eventSymbol->getSymbol();
    REQUIRE(key);
    const auto input = std::array{candle};
    const auto result = candles.processEvents(*key, candle.getSource().id(), input);
    REQUIRE(result);
    REQUIRE(result->at(0).events.size() == 1);
    CHECK(result->at(0).events[0].index == candle.index);
    CHECK(result->at(0).events[0].eventFlags == 0);
    CHECK(input[0].eventFlags == (flags::SNAPSHOT_BEGIN | flags::SNAPSHOT_END));

    api::IndexedEventModel<events::OtcMarketsOrder> orders;
    events::OtcMarketsOrder wrapped;
    wrapped.order = order(u"A", events::OrderSource::DEFAULT, 1, flags::SNAPSHOT_BEGIN | flags::SNAPSHOT_END, 4);
    const auto accepted = orders.processEvents(u"A", wrapped.getSourceId(), std::array{wrapped});
    REQUIRE(accepted);
    CHECK(accepted->at(0).events.at(0).order.eventFlags == 0);
    CHECK(orders.getSnapshot(u"A", wrapped.getSourceId())->at(0).order.orderId == 4);
}

} // namespace
