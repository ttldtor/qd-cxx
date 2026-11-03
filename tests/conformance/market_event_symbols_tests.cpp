// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/market_event_symbols.hpp"
#include <array>
#include <charconv>
#include <doctest/doctest.h>
#include <fstream>
#include <sstream>

using namespace org::ttldtor::qdcxx::events::market_symbols;

namespace {
std::optional<std::u16string> decode(std::string_view text) {
    if (text == "NULL") {
        return std::nullopt;
    }

    REQUIRE(text.size() % 4 == 0);
    std::u16string result;

    for (std::size_t position = 0; position < text.size(); position += 4) {
        unsigned unit = 0;
        const auto parsed = std::from_chars(text.data() + position, text.data() + position + 4, unit, 16);
        REQUIRE(parsed.ec == std::errc{});
        REQUIRE(parsed.ptr == text.data() + position + 4);
        result.push_back(static_cast<char16_t>(unit));
    }

    return result;
}

std::optional<std::u16string_view> view(const std::optional<std::u16string> &text) {
    return text ? std::optional<std::u16string_view>(*text) : std::nullopt;
}
} // namespace

TEST_CASE("Regional/base operations match pinned Java including every UTF16 code unit") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_MARKET_SYMBOL_FIXTURE);
    REQUIRE(input.good());
    std::string line;
    std::size_t rows = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::array<std::string, 4> columns;
        std::istringstream parser(line);

        for (auto &column : columns) {
            std::getline(parser, column, '\t');
        }

        CAPTURE(rows);
        CAPTURE(columns[0]);
        CAPTURE(columns[1]);
        CAPTURE(columns[2]);
        const auto symbol = decode(columns[1]);
        const auto argument = decode(columns[2]);
        const auto symbolView = view(symbol);
        const auto &operation = columns[0];

        if (operation == "has") {
            CHECK(hasExchangeCode(symbolView) == (columns[3] == "true"));
        } else if (operation == "code") {
            const auto expected = decode(columns[3]);
            REQUIRE(expected);
            REQUIRE(expected->size() == 1);
            CHECK(getExchangeCode(symbolView) == expected->front());
        } else if (operation == "base" || operation == "unit") {
            CHECK(getBaseSymbol(symbolView) == decode(columns[3]));

            if (operation == "unit") {
                REQUIRE(symbol);
                CHECK(hasExchangeCode(symbolView));
                CHECK(getExchangeCode(symbolView) == symbol->back());
                CHECK(changeExchangeCode(u"A", symbol->back()) ==
                      (symbol->back() == 0 ? std::optional<std::u16string>(u"A") : symbol));
            }
        } else if (operation == "exchange") {
            REQUIRE(argument);
            REQUIRE(argument->size() == 1);
            CHECK(changeExchangeCode(symbolView, argument->front()) == decode(columns[3]));
        } else if (operation == "replace") {
            CHECK(changeBaseSymbol(symbolView, view(argument)) == decode(columns[3]));
        } else if (operation == "supported") {
            CHECK(SUPPORTED_EXCHANGES == *decode(columns[3]));
        } else if (operation == "default") {
            CHECK(DEFAULT_EXCHANGES == *decode(columns[3]));
        } else {
            FAIL("Unknown oracle operation");
        }

        ++rows;
    }

    CHECK(rows == 66378);
}

TEST_CASE("Returned strings outlive caller storage and inputs remain unchanged") {
    std::optional<std::u16string> changed;

    {
        const std::u16string input = u"SPY&Q{z=2,a=1}";
        const std::u16string replacement = u"IBM";
        changed = changeBaseSymbol(input, replacement);
        CHECK(input == u"SPY&Q{z=2,a=1}");
        CHECK(replacement == u"IBM");
    }

    REQUIRE(changed);
    CHECK(*changed == u"IBM&Q{z=2,a=1}");
}
