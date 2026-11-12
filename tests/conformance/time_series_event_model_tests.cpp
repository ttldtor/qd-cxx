// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/time_series_event_model.hpp"

#include "qd/events/candle.hpp"
#include "qd/events/option_sale.hpp"
#include "qd/events/order.hpp"
#include "qd/events/time_and_sale.hpp"
#include "qd/events/time_helpers.hpp"

#include <array>
#include <doctest/doctest.h>
#include <expected>
#include <limits>
#include <utility>

namespace {

namespace api = org::ttldtor::qdcxx::api;
namespace events = org::ttldtor::qdcxx::events;
namespace flags = events::indexed_event_flags;

template <class Event>
concept HasTimeSeriesModel = requires { typename api::TimeSeriesEventModel<Event>; };

static_assert(HasTimeSeriesModel<events::TimeAndSale>);
static_assert(HasTimeSeriesModel<events::Candle>);
static_assert(!HasTimeSeriesModel<events::Order>);
static_assert(!HasTimeSeriesModel<events::OptionSale>);

template <class T>
void checkRefusal(const std::expected<T, api::TimeSeriesEventModelError> &result,
                  api::TimeSeriesEventModelError reason) {
    REQUIRE_FALSE(result);
    CHECK(result.error() == reason);
}

events::TimeAndSale sale(std::u16string_view symbol, std::int64_t time, std::int32_t sequence,
                         std::int32_t eventFlags = 0, std::int64_t id = 0) {
    events::TimeAndSale result;
    result.symbol = symbol;
    result.index = events::withTimeSequenceTime(sequence, time);
    result.eventFlags = eventFlags;
    result.eventTime = 99'999;
    result.tradeId = id;

    return result;
}

TEST_CASE("time-series view uses inclusive business time and publishes only completed indexed cuts") {
    api::IndexedEventModelLimits limits;
    limits.retainedEvents = 4;
    api::TimeSeriesEventModel<events::TimeAndSale> model(limits);
    CHECK_FALSE(model.getSymbol());
    CHECK(model.getFromTime() == api::TIME_SERIES_FROM_TIME_DISABLED);
    REQUIRE(model.setSymbol(u"A&Q"));
    const auto first = std::array{sale(u"A&Q", 1000, 0, flags::SNAPSHOT_BEGIN, 10)};
    checkRefusal(model.processEvents(model.getRangeRevision(), first), api::TimeSeriesEventModelError::RANGE_INACTIVE);
    const auto active = model.setFromTime(-500);
    REQUIRE(active);
    REQUIRE(model.processEvents(*active, first));
    CHECK(model.getSnapshot()->events.empty());

    const auto middle = std::array{sale(u"A&Q", 0, 2, 0, 2), sale(u"A&Q", 0, 1, 0, 1), sale(u"A&Q", -500, 0, 0, 5),
                                   sale(u"A&Q", -501, 0, flags::SNAPSHOT_SNIP | flags::TX_PENDING, 9)};
    REQUIRE(model.processEvents(*active, middle));
    CHECK(model.getSnapshot()->events.empty());
    const auto tail = std::array{sale(u"A&Q", -900, 0, 0, 12)};
    REQUIRE(model.processEvents(*active, tail));
    const auto snapshot = model.getSnapshot();
    REQUIRE(snapshot);
    CHECK(snapshot->symbol == u"A&Q");
    CHECK(snapshot->fromTime == -500);
    CHECK(snapshot->rangeRevision == *active);
    REQUIRE(snapshot->events.size() == 4);
    CHECK(snapshot->events[0].getTime() == -500);
    CHECK(snapshot->events[0].tradeId == 5);
    CHECK(snapshot->events[1].tradeId == 1);
    CHECK(snapshot->events[2].tradeId == 2);
    CHECK(snapshot->events[3].tradeId == 10);

    for (const auto &event : snapshot->events) {
        CHECK(event.eventFlags == 0);
        CHECK(event.getSource().id() == events::IndexedEventSource::DEFAULT.id());
    }

    CHECK(middle.back().eventFlags == (flags::SNAPSHOT_SNIP | flags::TX_PENDING));
    const auto removal = std::array{sale(u"A&Q", -500, 0, flags::REMOVE_EVENT)};
    REQUIRE(model.processEvents(*active, removal));
    REQUIRE(model.getSnapshot()->events.size() == 3);
    CHECK(snapshot->events[0].getTime() == -500);
}

TEST_CASE("time-series revision changes preserve overlap and refuse an unfinished previous range") {
    api::TimeSeriesEventModel<events::TimeAndSale> model;
    REQUIRE(model.setSymbol(u"A"));
    const auto first = model.setFromTime(0);
    REQUIRE(first);
    const auto initial = std::array{sale(u"A", 100, 0, 0, 1), sale(u"A", 200, 0, 0, 2),
                                    sale(u"A", 300, 0, flags::SNAPSHOT_BEGIN | flags::TX_PENDING, 3)};
    REQUIRE(model.processEvents(*first, initial));
    REQUIRE(model.getSnapshot()->events.size() == 2);

    const auto narrowed = model.setFromTime(200);
    REQUIRE(narrowed);
    CHECK(*narrowed > *first);
    const auto overlap = model.getSnapshot();
    REQUIRE(overlap);
    REQUIRE(overlap->events.size() == 1);
    CHECK(overlap->events[0].tradeId == 2);
    const auto lateTail = std::array{sale(u"A", 300, 1, flags::SNAPSHOT_END, 4)};
    checkRefusal(model.processEvents(*first, lateTail), api::TimeSeriesEventModelError::RANGE_CHANGED);
    const auto afterRange = std::array{sale(u"A", 400, 0, 0, 5)};
    REQUIRE(model.processEvents(*narrowed, afterRange));
    const auto completed = model.getSnapshot();
    REQUIRE(completed);
    REQUIRE(completed->events.size() == 2);
    CHECK(completed->events[0].tradeId == 2);
    CHECK(completed->events[1].tradeId == 5);

    const auto widened = model.setFromTime(std::numeric_limits<std::int64_t>::min());
    REQUIRE(widened);
    CHECK(model.getSnapshot()->events.size() == 2);
    const auto pending = std::array{sale(u"A", 100, 0, flags::TX_PENDING, 6)};
    REQUIRE(model.processEvents(*widened, pending));
    CHECK(model.setFromTime(std::numeric_limits<std::int64_t>::min()) == widened);
    CHECK(model.setSymbol(u"A") == widened);
    const auto finish = std::array{sale(u"A", 100, 1, 0, 7)};
    REQUIRE(model.processEvents(*widened, finish));
    const auto latest = model.getSnapshot();
    REQUIRE(latest);
    REQUIRE(latest->events.size() == 4);
    CHECK(latest->events[0].tradeId == 6);
    CHECK(latest->events[1].tradeId == 7);
}

TEST_CASE("model disable, replacement, move and clear leave detached values independent") {
    api::TimeSeriesEventModel<events::TimeAndSale> model;
    REQUIRE(model.setSymbol(u"A"));
    const auto first = model.setFromTime(-1);
    REQUIRE(first);
    const auto one = std::array{sale(u"A", 0, 0, 0, 1)};
    REQUIRE(model.processEvents(*first, one));
    auto moved = std::move(model);
    CHECK_FALSE(model.getSymbol());
    CHECK(model.getFromTime() == api::TIME_SERIES_FROM_TIME_DISABLED);
    checkRefusal(model.processEvents(*first, one), api::TimeSeriesEventModelError::RANGE_INACTIVE);

    const auto copy = moved.getSnapshot();
    REQUIRE(copy);
    REQUIRE(copy->events.size() == 1);
    REQUIRE(moved.setFromTime(api::TIME_SERIES_FROM_TIME_DISABLED));
    CHECK(moved.getSymbol() == u"A");
    CHECK(moved.getSnapshot()->events.empty());
    checkRefusal(moved.processEvents(*first, one), api::TimeSeriesEventModelError::RANGE_INACTIVE);
    const auto enabled = moved.setFromTime(0);
    REQUIRE(enabled);
    checkRefusal(moved.processEvents(*first, one), api::TimeSeriesEventModelError::RANGE_CHANGED);
    REQUIRE(moved.processEvents(*enabled, one));
    const auto changed = moved.setSymbol(u"B");
    REQUIRE(changed);
    CHECK(moved.getSnapshot()->events.empty());
    checkRefusal(moved.processEvents(*changed, one), api::TimeSeriesEventModelError::WRONG_KEY);
    const auto cleared = moved.clear();
    REQUIRE(cleared);
    CHECK_FALSE(moved.getSymbol());
    CHECK(moved.getFromTime() == api::TIME_SERIES_FROM_TIME_DISABLED);
    CHECK(moved.clear() == cleared);
    CHECK(copy->symbol == u"A");
    CHECK(copy->events[0].tradeId == 1);
}

TEST_CASE("wrong symbol and late resource refusal preserve the whole time-series operation") {
    api::IndexedEventModelLimits limits;
    limits.symbolUnits = 1;
    limits.retainedEvents = 1;
    limits.inputEvents = 2;
    api::TimeSeriesEventModel<events::TimeAndSale> model(limits);
    REQUIRE(model.setSymbol(u"A"));
    const auto active = model.setFromTime(0);
    REQUIRE(active);
    const auto one = std::array{sale(u"A", 1, 0, 0, 1)};
    REQUIRE(model.processEvents(*active, one));
    const auto changedAndForeign = std::array{sale(u"A", 1, 0, 0, 2), sale(u"B", 2, 0, 0, 3)};
    checkRefusal(model.processEvents(*active, changedAndForeign), api::TimeSeriesEventModelError::WRONG_KEY);
    const auto overCapacity = std::array{sale(u"A", 1, 0, 0, 2), sale(u"A", 2, 0, 0, 3)};
    checkRefusal(model.processEvents(*active, overCapacity), api::TimeSeriesEventModelError::RESOURCE_LIMIT);
    checkRefusal(model.setSymbol(u"AB"), api::TimeSeriesEventModelError::RESOURCE_LIMIT);
    CHECK(model.getRangeRevision() == *active);
    REQUIRE(model.getSnapshot()->events.size() == 1);
    CHECK(model.getSnapshot()->events[0].tradeId == 1);
    const auto narrowed = model.setFromTime(2);
    REQUIRE(narrowed);
    REQUIRE(model.processEvents(*narrowed, overCapacity));
    REQUIRE(model.getSnapshot()->events.size() == 1);
    CHECK(model.getSnapshot()->events[0].tradeId == 3);
}

TEST_CASE("Candle range keeps its exact canonical symbol and sorts valid retained values by business time") {
    api::TimeSeriesEventModel<events::Candle> model;
    events::Candle first;
    first.eventSymbol = events::CandleSymbol::valueOf(u"IBM{=m}");
    // The C++ event preserves all ten millisecond bits, including raw 1023.
    first.index = std::int64_t{1023} << 22;
    auto second = first;
    second.index = std::int64_t{1} << 32;
    CHECK(first.index < second.index);
    CHECK(first.getTime() == 1023);
    CHECK(second.getTime() == 1000);

    const auto events = std::array{first, second};
    REQUIRE(model.setFromTime(1000));
    REQUIRE(model.setSymbol(u"IBM"));
    checkRefusal(model.processEvents(model.getRangeRevision(), events), api::TimeSeriesEventModelError::WRONG_KEY);
    const auto symbol = first.eventSymbol->getSymbol();
    REQUIRE(symbol);
    const auto configured = model.setSymbol(symbol);
    REQUIRE(configured);
    REQUIRE(model.processEvents(*configured, events));
    const auto snapshot = model.getSnapshot();
    REQUIRE(snapshot);
    REQUIRE(snapshot->events.size() == 2);
    CHECK(snapshot->events[0].getTime() == 1000);
    CHECK(snapshot->events[1].getTime() == 1023);
}

} // namespace

TEST_CASE("range recovery qualifies both selection revision and the originating source epoch") {
    api::TimeSeriesEventModel<events::TimeAndSale> model;
    REQUIRE(model.setSymbol(u"A"));
    const auto revision = model.setFromTime(0);
    REQUIRE(revision);
    const auto old = model.beginSourceEpoch(12);
    REQUIRE(old);
    const std::array initial{sale(u"A", 100, 0, flags::SNAPSHOT_BEGIN | flags::SNAPSHOT_END, 1)};
    REQUIRE(model.processSourceEvents(*old, *revision, initial));
    REQUIRE(model.confirmSourceEpoch(*old, *revision));
    const auto complete = model.getSnapshot();
    REQUIRE(complete);
    REQUIRE(complete->events.size() == 1);
    CHECK(complete->events.front().tradeId == 1);

    const std::array open{sale(u"A", 200, 0, flags::TX_PENDING, 2)};
    REQUIRE(model.processSourceEvents(*old, *revision, open));
    REQUIRE(model.markSourceGap(*old, api::ModelRetention::RETAIN_COMPLETED));
    checkRefusal(model.getSnapshot(), api::TimeSeriesEventModelError::SOURCE_INACTIVE);
    const auto gap = model.getSourceSnapshot();
    REQUIRE(gap);
    CHECK(gap->source.status == api::ModelSourceStatus::GAP);
    REQUIRE(gap->snapshot.events.size() == 1);
    CHECK(gap->snapshot.events.front().tradeId == 1);

    const auto current = model.beginSourceEpoch(13);
    REQUIRE(current);
    CHECK(model.getRangeRevision() == *revision);
    CHECK(model.getFromTime() == 0);
    CHECK(model.getSymbol() == u"A");
    checkRefusal(model.processEvents(*revision, initial), api::TimeSeriesEventModelError::SOURCE_EPOCH_REQUIRED);
    checkRefusal(model.processSourceEvents(*old, *revision, initial),
                 api::TimeSeriesEventModelError::SOURCE_EPOCH_MISMATCH);
    const auto empty = model.getSourceSnapshot();
    REQUIRE(empty);
    CHECK(empty->source.status == api::ModelSourceStatus::RECOVERING);
    CHECK(empty->snapshot.events.empty());

    const std::array start{sale(u"A", 300, 0, flags::SNAPSHOT_BEGIN, 3)};
    REQUIRE(model.processSourceEvents(*current, *revision, start));
    const auto unfinished = model.confirmSourceEpoch(*current, *revision);
    REQUIRE_FALSE(unfinished);
    CHECK(unfinished.error() == api::ModelSourceError::REPLAY_INCOMPLETE);

    const std::array end{sale(u"A", 350, 0, flags::SNAPSHOT_END, 4)};
    REQUIRE(model.processSourceEvents(*current, *revision, end));
    REQUIRE(model.confirmSourceEpoch(*current, *revision));

    const auto revised = model.setFromTime(350);
    REQUIRE(revised);
    CHECK(model.getSourceState().status == api::ModelSourceStatus::RECOVERING);
    checkRefusal(model.processSourceEvents(*current, *revision, end), api::TimeSeriesEventModelError::RANGE_CHANGED);
    const auto staleReplay = model.confirmSourceEpoch(*current, *revision);
    REQUIRE_FALSE(staleReplay);
    CHECK(staleReplay.error() == api::ModelSourceError::STALE_SELECTION);
    const auto overlap = model.getSourceSnapshot();
    REQUIRE(overlap);
    REQUIRE(overlap->snapshot.events.size() == 1);
    CHECK(overlap->snapshot.events.front().tradeId == 4);

    auto moved = std::move(model);
    checkRefusal(model.processSourceEvents(*current, *revised, end),
                 api::TimeSeriesEventModelError::SOURCE_EPOCH_MISMATCH);
    REQUIRE(moved.processSourceEvents(*current, *revised, end));
    REQUIRE(moved.confirmSourceEpoch(*current, *revised));
    REQUIRE(moved.closeSource(*current, api::ModelRetention::RETAIN_COMPLETED));
    checkRefusal(moved.setSymbol(u"B"), api::TimeSeriesEventModelError::SOURCE_INACTIVE);
    const auto closed = moved.getSourceSnapshot();
    REQUIRE(closed);
    CHECK(closed->source.status == api::ModelSourceStatus::CLOSED);
    REQUIRE(closed->snapshot.events.size() == 1);
    CHECK(closed->snapshot.events.front().tradeId == 4);
    CHECK(gap->snapshot.events.front().tradeId == 1);
}
