// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/candle_period.hpp"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <charconv>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace org::ttldtor::qdcxx::events;

namespace {
/** @param text Hexadecimal unsigned integer from a reviewed fixture.
 * @return Parsed complete unsigned value.
 * @throws std::runtime_error The fixture syntax is invalid.
 */
std::uint64_t hexInteger(std::string_view text) {
    std::uint64_t value = 0;
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);

    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        throw std::runtime_error("Invalid fixture hexadecimal integer");
    }

    return value;
}

/** @param text Four hexadecimal digits per raw UTF-16 code unit.
 * @return Owned exact UTF-16 text, including unpaired surrogates.
 * @throws std::runtime_error Fixture syntax is invalid.
 */
std::u16string utf16(std::string_view text) {
    if (text.size() % 4 != 0) {
        throw std::runtime_error("Invalid fixture UTF-16 length");
    }

    std::u16string result;

    for (std::size_t i = 0; i < text.size(); i += 4) {
        result.push_back(static_cast<char16_t>(hexInteger(text.substr(i, 4))));
    }

    return result;
}

/** @param line One borrowed TSV row.
 * @return Eight borrowed columns, valid while line remains unchanged.
 * @throws std::runtime_error The row has the wrong number of columns.
 */
std::array<std::string_view, 8> columns(std::string_view line) {
    std::array<std::string_view, 8> result{};

    for (std::size_t i = 0; i < result.size(); ++i) {
        auto tab = line.find('\t');

        if ((i + 1 == result.size()) != (tab == std::string_view::npos)) {
            throw std::runtime_error("Invalid fixture column count");
        }

        result[i] = line.substr(0, tab);

        if (tab != std::string_view::npos) {
            line.remove_prefix(tab + 1);
        }
    }

    return result;
}
} // namespace

TEST_CASE("Candle period matches production Java value and type observations") {
    std::ifstream input(QDCXX_CANDLE_PERIOD_FIXTURE, std::ios::binary);
    REQUIRE(input.is_open());
    std::string line;
    std::size_t row = 0;

    while (std::getline(input, line)) {
        ++row;
        CAPTURE(row);
        auto data = columns(line);
        auto text = utf16(data[1]);
        bool rejected = false;

        try {
            if (data[0] == "type") {
                auto type = parseCandleType(text);
                CHECK(data[4] != "ERROR");
                CHECK(candleTypeShortName(type) == utf16(data[4]));
                CHECK(static_cast<int>(type) == std::stoi(std::string(data[5])));
                CHECK(candleTypeIntervalMillis(type) == std::stoll(std::string(data[7])));
            } else {
                auto period = data[0] == "parse"
                                  ? CandlePeriod::parse(text)
                                  : CandlePeriod(std::bit_cast<double>(hexInteger(data[3])),
                                                 static_cast<CandleType>(std::stoi(std::string(data[2]))));
                CHECK(data[4] != "ERROR");
                CHECK(period.toString() == utf16(data[4]));
                CHECK(static_cast<int>(period.getType()) == std::stoi(std::string(data[5])));
                CHECK(std::bit_cast<std::uint64_t>(period.getValue()) == hexInteger(data[6]));
                CHECK(period.getPeriodIntervalMillis() == std::stoll(std::string(data[7])));
            }
        } catch (const std::invalid_argument &) {
            rejected = true;
        }

        CHECK(rejected == (data[4] == "ERROR"));
    }

    CHECK(input.eof());
    CHECK(row == 73546);
}

TEST_CASE("Candle units retain exact ambiguous prefixes and minute month distinction") {
    CHECK(candleTypeShortName(CandleType::MINUTE) == u"m");
    CHECK(candleTypeShortName(CandleType::MONTH) == u"mo");
    CHECK(parseCandleType(u"M") == CandleType::MINUTE);
    CHECK(parseCandleType(u"Mo") == CandleType::MONTH);
    CHECK(parseCandleType(u"p") == CandleType::PRICE);
    CHECK(parseCandleType(u"pm") == CandleType::PRICE_MOMENTUM);
    CHECK(parseCandleType(u"pr") == CandleType::PRICE_RENKO);
    CHECK(parseCandleType(u"Minutes") == CandleType::MINUTE);
    CHECK_THROWS_AS((void)parseCandleType(u"MINUTES"), std::invalid_argument);
    CHECK_THROWS_AS((void)parseCandleType(u"PM"), std::invalid_argument);
    CHECK_THROWS_AS((void)parseCandleType(u" minute"), std::invalid_argument);
    CHECK_THROWS_AS(CandlePeriod(1, static_cast<CandleType>(-1)), std::invalid_argument);
    CHECK_THROWS_AS((void)candleTypeShortName(static_cast<CandleType>(13)), std::invalid_argument);
    CHECK_THROWS_AS((void)candleTypeIntervalMillis(static_cast<CandleType>(99)), std::invalid_argument);
}

TEST_CASE("Candle periods keep reference interval saturation spelling and equality") {
    CHECK(CandlePeriod::DEFAULT == CandlePeriod::TICK);
    CHECK(CandlePeriod::DAY.toString() == u"d");
    CHECK(CandlePeriod::parse(u"2.5Minutes").toString() == u"2.5m");
    CHECK(CandlePeriod(1, CandleType::MONTH).getPeriodIntervalMillis() == 2592000000LL);
    CHECK(CandlePeriod(1, CandleType::OPTEXP).getPeriodIntervalMillis() == 2592000000LL);
    CHECK(CandlePeriod(1, CandleType::YEAR).getPeriodIntervalMillis() == 31536000000LL);
    CHECK(CandlePeriod(-0.0019, CandleType::SECOND).getPeriodIntervalMillis() == -1);
    CHECK(CandlePeriod(std::numeric_limits<double>::infinity(), CandleType::DAY).getPeriodIntervalMillis() ==
          std::numeric_limits<std::int64_t>::max());
    CHECK(CandlePeriod(-std::numeric_limits<double>::infinity(), CandleType::DAY).getPeriodIntervalMillis() ==
          std::numeric_limits<std::int64_t>::min());
    CHECK(CandlePeriod(std::numeric_limits<double>::infinity(), CandleType::TICK).getPeriodIntervalMillis() == 0);
    CHECK(CandlePeriod(0x1p63).toString() == u"9223372036854775807t");
    CHECK(CandlePeriod(-0x1p63).toString() == u"-9223372036854775808t");
    CHECK(CandlePeriod(-0.0).toString() == u"0t");
    CHECK_FALSE(CandlePeriod(0.0) == CandlePeriod(-0.0));
    CHECK(CandlePeriod(std::numeric_limits<double>::quiet_NaN()) ==
          CandlePeriod(std::bit_cast<double>(std::uint64_t{0xfff8000000000001})));
    CHECK_FALSE(CandlePeriod(1, CandleType::MINUTE) == CandlePeriod(1, CandleType::MONTH));
    CHECK(CandlePeriod::parse(u"1e309d").toString() == u"Infinityd");
    CHECK_THROWS_AS((void)CandlePeriod::parse(u"Infinityd"), std::invalid_argument);
    CHECK_THROWS_AS((void)CandlePeriod::parse(u"NaNd"), std::invalid_argument);
    CHECK_THROWS_AS((void)CandlePeriod::parse(u"0x1p2d"), std::invalid_argument);
}
