// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/order_source.hpp"
#include "qd/events/source_selection.hpp"

#include <doctest/doctest.h>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace org::ttldtor::qdcxx::events;

TEST_CASE("production capability matrix and specific-source symbol observations") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_CAPABILITY_FIXTURE);
    REQUIRE(input.is_open());
    std::string line;
    std::size_t rows = 0;
    std::size_t capabilityRows = 0;
    std::size_t lists = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        INFO(line);
        std::istringstream stream(line);
        std::vector<std::string> cells;
        std::string cell;

        while (std::getline(stream, cell, '\t')) {
            cells.push_back(cell);
        }

        REQUIRE(cells.size() >= 2);
        ++rows;

        if (cells[0] == "CAP") {
            REQUIRE(cells.size() == 4);
            const auto source = OrderSource::valueOf(static_cast<std::int32_t>(std::stol(cells[1])));
            const auto type = static_cast<OrderEventType>(std::stoi(cells[2]));
            CHECK(source.isPublishable(type) == (cells[3] == "true"));
            ++capabilityRows;
        } else if (cells[0] == "FULL") {
            REQUIRE(cells.size() == 3);
            CHECK(OrderSource::valueOf(static_cast<std::int32_t>(std::stol(cells[1]))).isFullOrderBook() ==
                  (cells[2] == "true"));
        } else if (cells[0] == "LIST") {
            const auto actual = cells[1] == "FULL"
                                    ? OrderSource::fullOrderBook()
                                    : OrderSource::publishable(static_cast<OrderEventType>(std::stoi(cells[1])));
            REQUIRE(actual.size() == cells.size() - 2);

            for (std::size_t index = 0; index < actual.size(); ++index) {
                CHECK(actual[index].id() == std::stol(cells[index + 2]));
            }

            ++lists;
        } else if (cells[0] == "INVALID") {
            REQUIRE(cells.size() == 4);
            CHECK(cells[3] == "IllegalArgumentException");
            const auto type = static_cast<OrderEventType>(cells[2] == "null" ? -1 : 6);

            if (cells[1] == "instance") {
                CHECK_THROWS_AS(static_cast<void>(OrderSource::DEFAULT.isPublishable(type)), std::invalid_argument);
                CHECK_THROWS_AS(static_cast<void>(OrderSource::valueOf(u"TEST").isPublishable(type)),
                                std::invalid_argument);
            } else {
                CHECK_THROWS_AS(static_cast<void>(OrderSource::publishable(type)), std::invalid_argument);
            }
        } else if (cells[0] == "SPECIFIC") {
            REQUIRE(cells.size() == 5);
            const auto id = static_cast<std::int32_t>(std::stol(cells[1]));
            const auto selected = SourceSelection::specific(IndexedEventSource(id, std::nullopt));
            const auto same = SourceSelection::specific(IndexedEventSource(id, u"other"));
            REQUIRE(selected.source());
            CHECK(selected.source()->id() == std::stol(cells[2]));
            CHECK((selected == same) == (cells[3] == "true"));
            CHECK((selected == SourceSelection::all()) == (cells[4] == "true"));
            CHECK(selected.matches(IndexedEventSource(id, u"arbitrary")));
            CHECK_FALSE(selected.matches(IndexedEventSource(id == 0 ? 1 : 0, std::nullopt)));
        } else if (cells[0] == "NULL_SOURCE") {
            // A specific selector requires a value; null source is unrepresentable in this API.
            CHECK(cells[1] == "NullPointerException");
        } else {
            FAIL("Unknown oracle operation");
        }
    }

    CHECK(input.eof());
    CHECK(rows == 503);
    CHECK(capabilityRows == 414);
    CHECK(lists == 7);
}

TEST_CASE("all selection is open-ended and distinct from explicit default") {
    const auto all = SourceSelection::all();
    const auto defaultOnly = SourceSelection::specific(IndexedEventSource::DEFAULT);
    CHECK(all.isAll());
    CHECK(all.source() == nullptr);
    CHECK(all == SourceSelection::all());
    CHECK(all != defaultOnly);
    CHECK_FALSE(defaultOnly.isAll());
    CHECK(defaultOnly.matches(IndexedEventSource(0, std::nullopt)));
    CHECK_FALSE(defaultOnly.matches(OrderSource::NTV.indexedSource()));

    for (auto id : {std::numeric_limits<std::int32_t>::min(), -1, 0, 1, 10, std::numeric_limits<std::int32_t>::max()}) {
        CHECK(all.matches(IndexedEventSource(id, std::nullopt)));
    }
}

TEST_CASE("selectors and catalog results own their values") {
    auto original = IndexedEventSource(-1, u"owned");
    const auto selection = SourceSelection::specific(original);
    auto copy = selection;
    original = IndexedEventSource(0, std::nullopt);
    REQUIRE(selection.source());
    CHECK(selection.source()->name() == u"owned");
    copy = SourceSelection::all();
    CHECK(selection.source()->id() == -1);
    auto moved = SourceSelection::specific(IndexedEventSource(10, u"special"));
    const auto target = std::move(moved);
    REQUIRE(target.source());
    CHECK(target.source()->name() == u"special");
    // Moved-from values remain valid; no retained pointer is dereferenced after invalidation.
    moved = SourceSelection::all();
    CHECK(moved.isAll());
    auto sources = OrderSource::fullOrderBook();
    REQUIRE(sources.size() == 3);
    sources[0] = OrderSource::valueOf(u"TEST");
    CHECK(OrderSource::fullOrderBook()[0] == OrderSource::DEFAULT);
    CHECK_FALSE(sources[0].isFullOrderBook());
}
