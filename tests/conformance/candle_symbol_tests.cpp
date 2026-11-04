// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/candle_symbol.hpp"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace org::ttldtor::qdcxx::events;

namespace {
// Exercise factory use from a different translation unit before main begins.
const auto EARLY_SYMBOL = CandleSymbol::valueOf(u"CFG", CandleDataType::SOURCE, CandlePeriod::DAY);

/** @param text Complete unsigned hexadecimal fixture integer.
 * @return Parsed value.
 * @throws std::runtime_error Invalid fixture syntax.
 */
std::uint64_t integer(std::string_view text) {
    std::uint64_t result = 0;
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), result, 16);

    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        throw std::runtime_error("Invalid integer");
    }

    return result;
}

/** @param text Exact UTF-16 hex or NULL.
 * @return Independently owned text.
 * @throws std::runtime_error Invalid fixture syntax.
 */
std::optional<std::u16string> utf16(std::string_view text) {
    if (text == "NULL") {
        return std::nullopt;
    }

    if (text.size() % 4 != 0) {
        throw std::runtime_error("Invalid UTF16");
    }

    std::u16string result;

    for (std::size_t i = 0; i < text.size(); i += 4) {
        result.push_back(static_cast<char16_t>(integer(text.substr(i, 4))));
    }

    return result;
}

/** @param text Owned optional fixture input.
 * @return Borrowed optional view valid until input mutation.
 */
std::optional<std::u16string_view> view(const std::optional<std::u16string> &text) {
    return text ? std::optional<std::u16string_view>(*text) : std::nullopt;
}

/** @param line Complete borrowed fixture row.
 * @return Sixteen borrowed columns, valid while line remains unchanged.
 * @throws std::runtime_error Invalid column count.
 */
std::array<std::string_view, 16> columns(std::string_view line) {
    std::array<std::string_view, 16> result{};

    for (std::size_t i = 0; i < result.size(); ++i) {
        auto tab = line.find('\t');

        if ((i + 1 == result.size()) != (tab == std::string_view::npos)) {
            throw std::runtime_error("Invalid columns");
        }

        result[i] = line.substr(0, tab);

        if (tab != std::string_view::npos) {
            line.remove_prefix(tab + 1);
        }
    }

    return result;
}

/** @param text One trusted controlled attribute descriptor.
 * @return Typed independent attribute value.
 * @throws std::runtime_error Unknown descriptor.
 */
CandleAttribute attribute(std::string_view text) {
    auto value = text.substr(1);
    auto decimal = [&] {
        return std::stoi(std::string(value));
    };

    switch (text.front()) {
    case 'e':
        return CandleExchange(static_cast<char16_t>(integer(value)));
    case 'p':
        return static_cast<CandlePrice>(decimal());
    case 's':
        return static_cast<CandleSession>(decimal());
    case 'a':
        return static_cast<CandleAlignment>(decimal());
    case 'l':
        return CandlePriceLevel(std::bit_cast<double>(integer(value)));
    case 'd': {
        auto owned = utf16(value);

        return CandleDataType(view(owned));
    }
    case 't': {
        auto slash = value.find('/');

        return CandlePeriod(std::bit_cast<double>(integer(value.substr(0, slash))),
                            static_cast<CandleType>(std::stoi(std::string(value.substr(slash + 1)))));
    }
    default:
        throw std::runtime_error("Unknown attribute descriptor");
    }
}

/** @param input Borrowed nullable original symbol.
 * @param descriptors Controlled semicolon-separated fixture attributes.
 * @return Public factory result; list lengths are bounded by this fixture's contract.
 */
CandleSymbol make(std::optional<std::u16string_view> input, std::string_view descriptors) {
    std::vector<CandleAttribute> attributes;

    while (!descriptors.empty()) {
        auto end = descriptors.find(';');
        attributes.push_back(attribute(descriptors.substr(0, end)));

        if (end == std::string_view::npos) {
            break;
        }

        descriptors.remove_prefix(end + 1);
    }

    // Exercise the actual typed public overloads without introducing a test-only construction path.
    switch (attributes.size()) {
    case 0:
        return CandleSymbol::valueOf(input);
    case 1:
        return CandleSymbol::valueOf(input, {attributes[0]});
    case 2:
        return CandleSymbol::valueOf(input, {attributes[0], attributes[1]});
    case 7:
        return CandleSymbol::valueOf(input, {attributes[0], attributes[1], attributes[2], attributes[3], attributes[4],
                                             attributes[5], attributes[6]});
    default:
        throw std::runtime_error("Unexpected controlled attribute count");
    }
}

/** @param value Native binary64 value.
 * @return Exact stored binary64 bits, including a period NaN payload.
 */
std::uint64_t rawBits(double value) {
    return std::bit_cast<std::uint64_t>(value);
}
} // namespace

TEST_CASE("CandleSymbol matches production factories getters canonical identity and Java equality") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_CANDLE_SYMBOL_FIXTURE, std::ios::binary);
    REQUIRE(input.is_open());
    std::string line;
    std::size_t rows = 0;

    while (std::getline(input, line)) {
        ++rows;
        CAPTURE(rows);
        auto data = columns(line);
        auto original = utf16(data[1]);
        bool rejected = false;
        try {
            auto symbol = make(view(original), data[0] == "create" ? data[2] : "");

            if (data[0] == "equals") {
                auto right = utf16(data[2]);
                auto other = CandleSymbol::valueOf(view(right));
                CHECK(data[3] == "OK");
                bool equalError = false;
                try {
                    auto equal = symbol.equals(other);
                    CHECK(data[4] != "ERROR");
                    CHECK(equal == (data[4] == "true"));
                } catch (const std::invalid_argument &) {
                    equalError = true;
                }
                CHECK(equalError == (data[4] == "ERROR"));
                continue;
            }

            CHECK(data[3] == "OK");

            if (data[3] != "OK") {
                continue;
            }

            CHECK(symbol.toString() == utf16(data[4]));
            auto expectedBase = utf16(data[5]);
            CHECK(symbol.getBaseSymbol() == view(expectedBase));
            CHECK(symbol.getExchange().getExchangeCode() == integer(data[6]));
            CHECK(static_cast<int>(symbol.getPrice()) == std::stoi(std::string(data[7])));
            CHECK(static_cast<int>(symbol.getSession()) == std::stoi(std::string(data[8])));
            CHECK(rawBits(symbol.getPeriod().getValue()) == integer(data[9]));
            CHECK(static_cast<int>(symbol.getPeriod().getType()) == std::stoi(std::string(data[10])));
            CHECK(static_cast<int>(symbol.getAlignment()) == std::stoi(std::string(data[11])));
            CHECK(rawBits(symbol.getPriceLevel().getValue()) == integer(data[12]));
            CHECK(symbol.getDataType().getValue() == *utf16(data[13]));

            if (data[14] == "ERROR") {
                CHECK_THROWS_AS((void)symbol.hashCode(), std::invalid_argument);
            } else {
                CHECK(symbol.hashCode() == std::stol(std::string(data[14])));
            }
        } catch (const std::invalid_argument &) {
            rejected = true;
        } catch (const std::out_of_range &) {
            rejected = true;
        }
        CHECK(rejected == (data[3] == "ERROR"));
    }

    CHECK(rows == 9822);
}

TEST_CASE("CandleSymbol owns inputs preserves explicit getters and has a total C++ null equality") {
    CHECK(EARLY_SYMBOL.toString() == u"CFG{=d,dt=source}");
    CHECK(EARLY_SYMBOL.getDataType() == CandleDataType::SOURCE);
    auto input = std::u16string(u"AAPL&Q{=15Minutes,price=BID}");
    auto symbol = CandleSymbol::valueOf(input);
    input.assign(u"changed");
    CHECK(symbol.toString() == u"AAPL&Q{=15m,price=bid}");
    auto copy = symbol;
    auto moved = std::move(copy);
    CHECK(copy == moved);
    CHECK(symbol == moved);
    auto rendered = symbol.toString();
    rendered->assign(u"changed");
    CHECK(symbol.toString() == u"AAPL&Q{=15m,price=bid}");
    auto negativeZero = CandleSymbol::valueOf(u"A", CandlePeriod(-0.0));
    auto positiveZero = CandleSymbol::valueOf(u"A{=0t}");
    CHECK(negativeZero == positiveZero);
    CHECK(std::signbit(negativeZero.getPeriod().getValue()));
    CHECK_FALSE(std::signbit(positiveZero.getPeriod().getValue()));
    auto infinity = CandleSymbol::valueOf(u"A", CandlePeriod(std::numeric_limits<double>::infinity(), CandleType::DAY));
    CHECK(infinity.toString() == u"A{=Infinityd}");
    CHECK_THROWS_AS((void)CandleSymbol::valueOf(*infinity.getSymbol()), std::invalid_argument);
    CHECK_THROWS_AS((void)CandleSymbol::valueOf(u"A", {CandlePrice::BID, CandlePrice::BID}), std::invalid_argument);
    auto absent = CandleSymbol::valueOf(std::nullopt);
    auto otherAbsent = CandleSymbol::valueOf(std::nullopt);
    CHECK(absent == otherAbsent);
    CHECK(absent.equals(absent));
    CHECK_THROWS_AS((void)absent.equals(otherAbsent), std::invalid_argument);
    CHECK_FALSE(symbol.equals(absent));
    CHECK_THROWS_AS((void)absent.hashCode(), std::invalid_argument);
    CHECK(!absent.getBaseSymbol());
    CHECK(absent.getDataType() == CandleDataType::DEFAULT);
    CHECK(CandleSymbol::valueOf(u"A{z=1,a=m}") != CandleSymbol::valueOf(u"A{a=m,z=1}"));
}
