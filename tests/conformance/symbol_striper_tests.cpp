// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/core/symbol_striper.hpp"
#include "qd/encoding/penta_symbol.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace core = org::ttldtor::qdcxx::core;
namespace encoding = org::ttldtor::qdcxx::encoding;
using Error = core::SymbolStripeError;

/** Explicit generous grants for this fixed offline fixture, not production defaults.
 * @return Finite parser/query/backing grants.
 */
core::SymbolStriperLimits grants() {
    return {1024, 1024, 1024, 64 * 1024};
}

/** Preserve empty trailing TSV cells and exact fixture bytes.
 * @param line One CR-stripped fixture line.
 * @return Owning cells in original order.
 */
std::vector<std::string> split(std::string_view line) {
    std::vector<std::string> result;
    std::size_t begin = 0;

    for (;;) {
        const auto end = line.find('\t', begin);
        result.emplace_back(line.substr(begin, end == line.npos ? line.size() - begin : end - begin));

        if (end == line.npos) {
            return result;
        }

        begin = end + 1;
    }
}

/** Decode reviewed ASCII metadata without normalization.
 * @param text ASCII fixture name.
 * @return Exact owned UTF-16 name.
 */
std::u16string ascii(std::string_view text) {
    return {text.begin(), text.end()};
}

/** Decode comma-separated exact UTF-16 code units, including NUL/unpaired surrogates.
 * @param text Uppercase four-hex-digit units; empty is an empty non-null symbol.
 * @return Owned exact symbol.
 */
std::u16string units(std::string_view text) {
    std::u16string result;

    for (std::size_t index = 0; index < text.size(); index += 5) {
        REQUIRE(index + 4 <= text.size());
        const auto value = std::stoul(std::string(text.substr(index, 4)), nullptr, 16);
        result.push_back(static_cast<char16_t>(value));
    }

    return result;
}

/** Construct one fixture-selected production value.
 * @param spec ASCII canonical or accepted striper name.
 * @return Valid immutable owner.
 */
core::SymbolStriper striper(std::u16string_view spec) {
    auto result = core::SymbolStriper::parse(spec, grants());
    REQUIRE(result);

    return std::move(*result);
}

/** Render a proven interval in the Java BitSet display syntax for fixture comparison.
 * @param overlap Exact contiguous interval.
 * @return Java-compatible ordered stripe-set spelling.
 */
std::string bitSet(core::StripeIntersection overlap) {
    std::string result = "{";

    for (std::size_t index = 0; index < overlap.count; ++index) {
        if (index) {
            result += ", ";
        }

        result += std::to_string(overlap.first + index);
    }

    return result + "}";
}
} // namespace

TEST_CASE("pure symbol stripers match pinned Java production grammar selection filters and intersections") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_STRIPE_FIXTURE);
    REQUIRE(input.good());
    std::string line;
    std::size_t compared = 0;
    std::size_t unknown = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        INFO(line);
        const auto cells = split(line);
        REQUIRE(!cells.empty());

        if (cells[0] == "striper") {
            REQUIRE(cells.size() == 4);
            const auto value = striper(units(cells[1]));
            CHECK(value.name() == ascii(cells[2]));
            CHECK(value.stripeCount() == std::stoull(cells[3]));
        } else if (cells[0] == "stripe") {
            REQUIRE(cells.size() == 4);
            const auto value = striper(ascii(cells[1]));
            const auto filter = value.stripeFilter(std::stoull(cells[2]));
            REQUIRE(filter);
            CHECK(filter->name() == ascii(cells[3]));
        } else if (cells[0] == "invalid") {
            REQUIRE(cells.size() == 2);
            const auto result = core::SymbolStriper::parse(units(cells[1]), grants());
            REQUIRE_FALSE(result);
            CHECK((result.error() == Error::INVALID_SPECIFICATION || result.error() == Error::UNKNOWN_SPECIFICATION));
        } else if (cells[0] == "symbol") {
            REQUIRE(cells.size() == 8);
            const auto value = striper(ascii(cells[1]));
            const auto symbol = units(cells[2]);
            const auto selected = value.stripeIndex(symbol);
            REQUIRE(selected);
            CHECK(*selected == std::stoull(cells[4]));
            const auto encoded = encoding::encodePentaSymbol(symbol);
            CHECK((encoded ? encoded->cipher : 0) == std::stol(cells[3]));
            std::size_t accepted = 0;
            std::int64_t last = -1;
            const bool wildcard = symbol == u"*";

            for (std::size_t index = 0; index < value.stripeCount(); ++index) {
                const auto filter = value.stripeFilter(index);
                REQUIRE(filter);
                const auto matches = filter->accepts(symbol, wildcard);
                REQUIRE(matches);

                if (*matches) {
                    ++accepted;
                    last = static_cast<std::int64_t>(index);
                }
            }

            CHECK(accepted == std::stoull(cells[6]));
            CHECK(last == std::stoll(cells[7]));
        } else if (cells[0] == "intersection") {
            REQUIRE(cells.size() == 5);
            const auto value = striper(ascii(cells[1]));
            const auto filter = core::SymbolStripeFilter::parse(ascii(cells[2]), grants());

            if (!filter) {
                CHECK(filter.error() == Error::UNKNOWN_SPECIFICATION);
                CHECK(cells[4] == "unknown");
                ++unknown;
            } else {
                CHECK(filter->name() == ascii(cells[3]));
                const auto overlap = value.intersectingStripes(*filter);
                REQUIRE(overlap);
                CHECK((*overlap ? bitSet(**overlap) : "unknown") == cells[4]);
            }
        } else {
            REQUIRE(cells[0] == "factory");
            // Java-only overloaded constructor observations are separate from the native string API.
            CHECK(cells[2] == "by1");
        }

        ++compared;
    }

    CHECK(compared == 1263);
    CHECK(unknown == 10);
}

TEST_CASE("pure stripers retain owned state and enforce explicit independent grants") {
    std::u16string source = u"byrange-D-M-";
    auto original = striper(source);
    auto retained = original;
    auto leaf = original.stripeFilter(1);
    REQUIRE(leaf);
    source.assign(u"changed");
    original = {};
    CHECK(retained.name() == u"byrange-D-M-");
    CHECK(leaf->name() == u"range-D-M-");
    CHECK(*leaf->accepts(u"IBM"));
    CHECK_FALSE(*leaf->accepts(u"MSFT"));
    CHECK(*leaf->accepts(u"MSFT", true));
    auto moved = std::move(retained);
    CHECK_FALSE(retained.valid());
    CHECK(retained.stripeIndex(u"IBM").error() == Error::INVALID_STATE);
    CHECK(moved.stripeFilter(3).error() == Error::INVALID_INDEX);
    CHECK(moved.intersectingStripes({}).error() == Error::INVALID_STATE);

    auto limits = grants();
    limits.stripes = 1;
    CHECK(core::SymbolStriper::parse(u"byhash2", limits).error() == Error::RESOURCE_LIMIT);
    limits = grants();
    limits.specificationUnits = 2;
    CHECK(core::SymbolStriper::parse(u"by1", limits).error() == Error::RESOURCE_LIMIT);
    limits = grants();
    limits.storageBytes = 0;
    CHECK(core::SymbolStriper::parse(u"", limits).error() == Error::RESOURCE_LIMIT);
    limits = grants();
    limits.symbolUnits = 0;
    auto bounded = core::SymbolStriper::parse(u"byhash4", limits);
    REQUIRE(bounded);
    CHECK(*bounded->stripeIndex(u"") == 0);
    CHECK(bounded->stripeIndex(u"A").error() == Error::RESOURCE_LIMIT);
    limits = grants();
    limits.stripes = static_cast<std::size_t>(1) << 30;
    auto large = core::SymbolStriper::parse(u"byhash1073741824", limits);
    REQUIRE(large);
    CHECK(large->stripeCount() == limits.stripes);
    CHECK(*large->stripeIndex(u"AAPL") < large->stripeCount());
}
