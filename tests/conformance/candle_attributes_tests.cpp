// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/candle_attributes.hpp"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>

using namespace org::ttldtor::qdcxx::events;

namespace {
// These owning snapshots initialize in the test TU before main; the constants and
// methods are defined in the separately linked library TU.
const std::array<std::u16string, 7> EARLY_DATA_TYPES{
    CandleDataType::SOURCE.toString(), CandleDataType::THO.toString(), CandleDataType::ALL.toString(),
    CandleDataType::RTH.toString(),    CandleDataType::ETH.toString(), CandleDataType::UNDEFINED.toString(),
    CandleDataType::DEFAULT.toString()};
const bool EARLY_LEVEL_NAN = std::isnan(CandlePriceLevel::DEFAULT.getValue());

/** @param text Complete hexadecimal unsigned value.
 * @return Parsed integer.
 * @throws std::runtime_error Invalid fixture syntax.
 */
std::uint64_t integer(std::string_view text) {
    std::uint64_t value = 0;
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);

    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        throw std::runtime_error("Invalid fixture integer");
    }

    return value;
}

/** @param text Four hex digits per UTF-16 unit, or NULL.
 * @return Owned exact text, or nullopt.
 * @throws std::runtime_error Invalid fixture syntax.
 */
std::optional<std::u16string> utf16(std::string_view text) {
    if (text == "NULL") {
        return std::nullopt;
    }

    if (text.size() % 4 != 0) {
        throw std::runtime_error("Invalid fixture UTF-16");
    }

    std::u16string result;

    for (std::size_t i = 0; i < text.size(); i += 4) {
        result.push_back(static_cast<char16_t>(integer(text.substr(i, 4))));
    }

    return result;
}

/** @param text Owned text for this observation.
 * @return Borrowed optional view valid during the observation.
 */
std::optional<std::u16string_view> view(const std::optional<std::u16string> &text) {
    return text ? std::optional<std::u16string_view>(*text) : std::nullopt;
}

/** @param line One borrowed fixture row.
 * @return Five borrowed columns, valid until line mutation.
 * @throws std::runtime_error Invalid column count.
 */
std::array<std::string_view, 5> columns(std::string_view line) {
    std::array<std::string_view, 5> result{};

    for (std::size_t i = 0; i < result.size(); ++i) {
        auto tab = line.find('\t');

        if ((i + 1 == result.size()) != (tab == std::string_view::npos)) {
            throw std::runtime_error("Invalid fixture columns");
        }

        result[i] = line.substr(0, tab);

        if (tab != std::string_view::npos) {
            line.remove_prefix(tab + 1);
        }
    }

    return result;
}

/** @param text Borrowed ASCII hexadecimal text encoded in the fixture.
 * @return Exact ASCII copy.
 */
std::string ascii(std::u16string_view text) {
    std::string result;

    for (auto unit : text) {
        result.push_back(static_cast<char>(unit));
    }

    return result;
}
} // namespace

TEST_CASE("Candle attributes match unmodified production Java observations") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_CANDLE_ATTRIBUTES_FIXTURE, std::ios::binary);
    REQUIRE(input.is_open());
    std::string line;
    std::size_t rows = 0;

    while (std::getline(input, line)) {
        ++rows;
        CAPTURE(rows);
        auto data = columns(line);
        auto original = utf16(data[2]);
        auto operand = utf16(data[3]);
        auto text = view(original);
        auto value = view(operand);
        std::optional<std::u16string> actual;
        bool error = false;
        try {
            if (data[0] == "price") {
                if (data[1] == "parse")
                    actual = candlePriceText(parseCandlePrice(text));
                else if (data[1] == "get")
                    actual = candlePriceText(getCandlePrice(text));
                else if (data[1] == "normalize")
                    actual = normalizeCandlePrice(text);
                else
                    actual = changeCandlePrice(text, parseCandlePrice(value));
            } else if (data[0] == "session") {
                if (data[1] == "parse")
                    actual = candleSessionText(parseCandleSession(text));
                else if (data[1] == "get")
                    actual = candleSessionText(getCandleSession(text));
                else if (data[1] == "normalize")
                    actual = normalizeCandleSession(text);
                else
                    actual = changeCandleSession(text, parseCandleSession(value));
            } else if (data[0] == "alignment") {
                if (data[1] == "parse")
                    actual = candleAlignmentText(parseCandleAlignment(text));
                else if (data[1] == "get")
                    actual = candleAlignmentText(getCandleAlignment(text));
                else if (data[1] == "normalize")
                    actual = normalizeCandleAlignment(text);
                else
                    actual = changeCandleAlignment(text, parseCandleAlignment(value));
            } else if (data[0] == "level") {
                if (data[1] == "normalize")
                    actual = CandlePriceLevel::normalizeAttributeForSymbol(text);
                else if (data[1] == "change")
                    actual = CandlePriceLevel::parse(value).changeAttributeForSymbol(text);
                else {
                    auto level = data[1] == "parse" ? CandlePriceLevel::parse(text)
                                 : data[1] == "get" ? CandlePriceLevel::getAttributeForSymbol(text)
                                                    : CandlePriceLevel(std::bit_cast<double>(integer(ascii(*text))));
                    REQUIRE(data[4] != "ERROR");
                    auto colon = data[4].find(':');
                    auto bits = std::bit_cast<std::uint64_t>(level.getValue());

                    if (std::isnan(level.getValue()))
                        bits = 0x7ff8000000000000ULL;

                    CHECK(bits == integer(data[4].substr(0, colon)));
                    actual = level.toString();
                    CHECK(actual == utf16(data[4].substr(colon + 1)));
                    continue;
                }
            } else if (data[0] == "data") {
                if (data[1] == "parse")
                    actual = CandleDataType::parse(text).toString();
                else if (data[1] == "get")
                    actual = CandleDataType::getAttributeForSymbol(text).toString();
                else if (data[1] == "normalize")
                    actual = CandleDataType::normalizeAttributeForSymbol(text);
                else
                    actual = CandleDataType::parse(value).changeAttributeForSymbol(text);
            } else if (data[0] == "exchange") {
                auto exchange = data[1] == "get" ? CandleExchange::getAttributeForSymbol(text)
                                                 : CandleExchange(static_cast<char16_t>(integer(ascii(*value))));

                if (data[1] == "change")
                    actual = exchange.changeAttributeForSymbol(text);
                else {
                    REQUIRE(data[4] != "ERROR");
                    auto colon = data[4].find(':');
                    CHECK(exchange.getExchangeCode() == integer(data[4].substr(0, colon)));
                    CHECK(exchange.toString() == *utf16(data[4].substr(colon + 1)));
                    continue;
                }
            } else {
                FAIL("Unknown fixture operation");
            }
        } catch (const std::invalid_argument &) {
            error = true;
        } catch (const std::out_of_range &) {
            error = true;
        }
        CHECK(error == (data[4] == "ERROR"));

        if (!error && data[4] != "ERROR") {
            CHECK(actual == utf16(data[4]));
        }
    }

    CHECK(rows > 264000);
}

TEST_CASE("Candle attribute API owns data and distinguishes parsing from symbol recognition") {
    CHECK(parseCandlePrice(u"b") == CandlePrice::BID);
    CHECK(parseCandlePrice(u"\u017f") == CandlePrice::SETTLEMENT);
    CHECK(parseCandleSession(u"t") == CandleSession::REGULAR);
    CHECK(getCandleSession(u"A{tho=t}") == CandleSession::ANY);
    CHECK(getCandleSession(u"A{tho=TRUE}") == CandleSession::REGULAR);
    CHECK_THROWS_AS((void)parseCandleAlignment(u"session"), std::invalid_argument);
    CHECK_THROWS_AS((void)candlePriceText(static_cast<CandlePrice>(-1)), std::invalid_argument);
    CHECK_THROWS_AS((void)candleSessionText(static_cast<CandleSession>(2)), std::invalid_argument);
    CHECK_THROWS_AS((void)candleAlignmentText(static_cast<CandleAlignment>(2)), std::invalid_argument);
    CHECK_THROWS_AS((void)CandlePriceLevel(-0.0), std::invalid_argument);
    CHECK_THROWS_AS((void)CandlePriceLevel(-1), std::invalid_argument);
    CHECK_THROWS_AS((void)CandlePriceLevel(std::numeric_limits<double>::infinity()), std::invalid_argument);
    CHECK(CandlePriceLevel(0).toString() == u"0");
    CHECK(CandlePriceLevel::parse(u"0x1.8p1").toString() == u"3");
    CHECK(CandlePriceLevel::parse(u"NaN") == CandlePriceLevel{});
    CHECK(CandlePriceLevel(0x1p63).toString() == u"9223372036854775807");
    CHECK(CandlePriceLevel{}.changeAttributeForSymbol(u"A{pl=1}") == u"A");
    CHECK(normalizeCandlePrice(u"A{price=LAST}") == u"A{price=last}");
    CHECK(normalizeCandleSession(u"A{tho=f}") == u"A{tho=f}");
    CHECK(CandlePriceLevel::normalizeAttributeForSymbol(u"A{pl=+NaN}") == u"A{pl=NaN}");
    CHECK(CandleDataType::normalizeAttributeForSymbol(u"A{dt=}") == u"A");
    CHECK(CandleDataType::parse(std::nullopt) == CandleDataType::UNDEFINED);
    CHECK(CandleDataType(u"SOURCE") != CandleDataType::SOURCE);
    auto input = std::u16string(u"custom");
    auto owned = CandleDataType(input);
    input.assign(u"mutated");
    CHECK(owned.getValue() == u"custom");
    auto rendered = owned.toString();
    rendered.assign(u"changed");
    CHECK(owned.getValue() == u"custom");
    CHECK(CandleExchange{}.toString() == u"COMPOSITE");
    CHECK(CandleExchange(u'&').changeAttributeForSymbol(u"A{a=s}") == u"A&&{a=s}");
    CHECK(CandleExchange::getAttributeForSymbol(std::u16string{u'A', u'&', static_cast<char16_t>(0xd800)})
              .getExchangeCode() == static_cast<char16_t>(0xd800));
    CHECK(!CandleExchange{}.changeAttributeForSymbol(std::nullopt));
}

TEST_CASE("Candle canonical constants are ready during another translation unit initialization") {
    INFO("SOURCE address: " << static_cast<const void *>(&CandleDataType::SOURCE));
    INFO("DEFAULT level address: " << static_cast<const void *>(&CandlePriceLevel::DEFAULT));
    INFO("early SOURCE size: " << EARLY_DATA_TYPES[0].size()
                               << " current SOURCE size: " << CandleDataType::SOURCE.getValue().size());
    constexpr std::array<std::u16string_view, 7> EXPECTED{u"source", u"tho", u"all", u"rth", u"eth", u"", u""};

    for (std::size_t i = 0; i < EXPECTED.size(); ++i) {
        CAPTURE(i);
        CHECK(EARLY_DATA_TYPES[i] == EXPECTED[i]);
    }

    CHECK(EARLY_LEVEL_NAN);
    CHECK(CandleDataType(u"source") == CandleDataType::SOURCE);
    auto copy = CandleDataType::SOURCE.toString();
    copy.assign(u"modified");
    CHECK(CandleDataType::SOURCE.getValue() == u"source");
}
