// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/market_event_symbols.hpp"
#include <array>
#include <charconv>
#include <doctest/doctest.h>
#include <fstream>
#include <sstream>
#include <stdexcept>

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

TEST_CASE("Attribute operations match production Java exact UTF16 and malformed syntax") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_MARKET_ATTRIBUTE_FIXTURE);
    REQUIRE(input.good());
    std::string line;
    std::size_t rows = 0;
    std::size_t nullKeyErrors = 0;
    std::size_t substringErrors = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::array<std::string, 5> columns;
        std::istringstream parser(line);

        for (auto &column : columns) {
            std::getline(parser, column, '\t');
        }

        CAPTURE(rows);
        CAPTURE(columns[0]);
        CAPTURE(columns[1]);
        CAPTURE(columns[2]);
        CAPTURE(columns[3]);
        const auto symbol = decode(columns[1]);
        const auto key = decode(columns[2]);
        const auto value = decode(columns[3]);
        const auto invoke = [&]() -> std::optional<std::u16string> {
            if (columns[0] == "get") {
                return getAttributeStringByKey(view(symbol), view(key));
            }

            if (columns[0] == "remove") {
                return removeAttributeStringByKey(view(symbol), view(key));
            }

            REQUIRE(columns[0] == "change");

            return changeAttributeStringByKey(view(symbol), view(key), view(value));
        };

        if (columns[0] == "has") {
            CHECK(hasAttributes(view(symbol)) == (columns[4] == "true"));
        } else if (columns[4] == "THROW:NullPointerException") {
            CHECK_THROWS_AS(invoke(), std::invalid_argument);
            ++nullKeyErrors;
        } else if (columns[4] == "THROW:StringIndexOutOfBoundsException") {
            CHECK_THROWS_AS(invoke(), std::out_of_range);
            ++substringErrors;
        } else {
            REQUIRE(columns[4].find("THROW:") != 0);
            CHECK(invoke() == decode(columns[4]));
        }

        ++rows;
    }

    CHECK(rows == 276958);
    CHECK(nullKeyErrors > 0);
    // The count is recorded even if this bounded corpus has no invalid substring ranges.
    CAPTURE(substringErrors);
}

TEST_CASE("Attribute results own their storage and null keys precede symbol inspection") {
    std::optional<std::u16string> changed;
    std::optional<std::u16string> value;

    {
        const std::u16string symbol = u"SPY&Q{a=1,a=2,z=3}";
        const std::u16string key = u"a";
        const std::u16string replacement = u"bid";
        changed = changeAttributeStringByKey(symbol, key, replacement);
        value = getAttributeStringByKey(symbol, key);
        CHECK(symbol == u"SPY&Q{a=1,a=2,z=3}");
        CHECK(key == u"a");
        CHECK(replacement == u"bid");
    }

    CHECK(changed == std::optional<std::u16string>(u"SPY&Q{a=bid,z=3}"));
    CHECK(value == std::optional<std::u16string>(u"1"));
    CHECK_THROWS_AS((void)getAttributeStringByKey(std::nullopt, std::nullopt), std::invalid_argument);
    CHECK_THROWS_AS((void)changeAttributeStringByKey(std::nullopt, std::nullopt, std::nullopt), std::invalid_argument);
    CHECK_THROWS_AS((void)removeAttributeStringByKey(std::nullopt, std::nullopt), std::invalid_argument);
    static_assert(noexcept(hasAttributes(std::nullopt)));
}
