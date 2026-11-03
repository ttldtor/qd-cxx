// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/order_source.hpp"

#include <doctest/doctest.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

using namespace org::ttldtor::qdcxx::events;

namespace {
/** @param text Oracle UTF-16 hexadecimal units or NULL.
 * @return Owned exact decoded name, including malformed Unicode.
 */
std::optional<std::u16string> decode(std::string_view text) {
    if (text == "NULL") {
        return std::nullopt;
    }

    std::u16string result;
    REQUIRE(text.size() % 4 == 0);

    for (std::size_t offset = 0; offset < text.size(); offset += 4) {
        result.push_back(static_cast<char16_t>(std::stoul(std::string(text.substr(offset, 4)), nullptr, 16)));
    }

    return result;
}

/** @param source Actual identity.
 * @param cells Oracle operation, input, ID, name and hash columns.
 */
void check(const IndexedEventSource &source, const std::vector<std::string> &cells) {
    REQUIRE(cells.size() == 5);
    CHECK(source.id() == std::stol(cells[2]));
    const auto expected = decode(cells[3]);
    CHECK(source.name().has_value() == expected.has_value());

    if (expected) {
        REQUIRE(source.name());
        CHECK(*source.name() == *expected);
    }

    CHECK(source.hashCode() == std::stol(cells[4]));
}
} // namespace

TEST_CASE("pinned production source observations") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_SOURCE_FIXTURE);
    REQUIRE(input.is_open());
    std::string line;
    std::size_t rows = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream stream(line);
        std::vector<std::string> cells;
        std::string cell;

        while (std::getline(stream, cell, '\t')) {
            cells.push_back(cell);
        }

        REQUIRE(cells.size() >= 2);
        INFO(line);
        ++rows;

        if (cells[0] == "BASE") {
            check(IndexedEventSource(static_cast<std::int32_t>(std::stol(cells[2])), decode(cells[1])), cells);
        } else if (cells[0] == "NAME") {
            const auto name = decode(cells[1]);
            const auto view = name ? std::optional<std::u16string_view>(*name) : std::nullopt;

            if (cells.size() == 3) {
                CHECK((cells[2] == "IllegalArgumentException" || cells[2] == "NullPointerException"));
                CHECK_THROWS_AS(static_cast<void>(OrderSource::valueOf(view)), std::invalid_argument);
            } else {
                const auto source = OrderSource::valueOf(view);
                check(source.indexedSource(), cells);
                CHECK(OrderSource::valueOf(source.id()) == source);
            }
        } else if (cells[0] == "ID" || cells[0] == "BUILTIN") {
            const auto id = static_cast<std::int32_t>(std::stol(cells[cells[0] == "ID" ? 1 : 2]));

            if (cells.size() == 3) {
                CHECK(cells[2] == "IllegalArgumentException");
                CHECK_THROWS_AS(static_cast<void>(OrderSource::valueOf(id)), std::invalid_argument);
            } else {
                const auto source = OrderSource::valueOf(id);
                check(source.indexedSource(), cells);
                CHECK(OrderSource::valueOf(source.name()) == source);
            }
        } else if (cells[0] == "EQUAL") {
            REQUIRE(cells.size() == 5);
            const auto id = static_cast<std::int32_t>(std::stol(cells[1]));
            const IndexedEventSource left(id, std::nullopt);
            const IndexedEventSource right(id, u"different");
            CHECK((left == right) == (cells[2] == "true"));
            CHECK((right == left) == (cells[3] == "true"));
            CHECK((left.hashCode() == right.hashCode()) == (cells[4] == "true"));
        } else if (cells[0] == "CROSS") {
            const IndexedEventSource base(OrderSource::NTV.id(), std::nullopt);
            CHECK((base == OrderSource::NTV) == (cells[1] == "true"));
            CHECK((OrderSource::NTV == base) == (cells[2] == "true"));
            CHECK((base.hashCode() == OrderSource::NTV.hashCode()) == (cells[3] == "true"));
        } else if (cells[0] == "UNEQUAL") {
            CHECK((IndexedEventSource(-1, u"same") == IndexedEventSource(1, u"same")) == (cells[1] == "true"));
        } else {
            FAIL("Unknown oracle operation");
        }
    }

    CHECK(input.eof());
    CHECK(rows > 200);
}

TEST_CASE("owned values and ordinary public spelling") {
    auto ntv = OrderSource::valueOf(u"NTV");
    CHECK(ntv == OrderSource::NTV);
    CHECK_THROWS_AS(static_cast<void>(OrderSource::valueOf(std::nullopt)), std::invalid_argument);
    CHECK(OrderSource::valueOf(0) == OrderSource::DEFAULT);
    CHECK_THROWS_AS(static_cast<void>(OrderSource::valueOf(-1)), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(OrderSource::valueOf(10)), std::invalid_argument);
    CHECK(OrderSource::NTV_LOWER.name() == u"ntv");
    CHECK(OrderSource::NTV_LOWER != ntv);
    auto copy = ntv;
    ntv = OrderSource::DEFAULT;
    CHECK(copy.name() == u"NTV");
    CHECK(ntv.name() == u"DEFAULT");
    std::u16string mutableName = u"owned";
    IndexedEventSource source(-1, mutableName);
    mutableName[0] = u'X';
    CHECK(source.name() == u"owned");
    CHECK(source.hashCode() == -1);
    CHECK(std::hash<IndexedEventSource>{}(source) == static_cast<std::size_t>(-1));
    CHECK(std::hash<OrderSource>{}(copy) == std::hash<IndexedEventSource>{}(copy.indexedSource()));
    std::unordered_set<IndexedEventSource> sources;
    sources.emplace(4, u"one");
    sources.emplace(4, u"two");
    CHECK(sources.size() == 1);
}
