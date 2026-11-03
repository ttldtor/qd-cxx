// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/ipf/instrument_profile.hpp"
#include <bit>
#include <charconv>
#include <cmath>
#include <doctest/doctest.h>
#include <fstream>
#include <limits>
#include <sstream>
using namespace org::ttldtor::qdcxx::ipf;

namespace {
std::u16string decode(std::string_view text) {
    REQUIRE(text.size() % 4 == 0);
    std::u16string result;

    for (std::size_t i = 0; i < text.size(); i += 4) {
        unsigned unit = 0;
        auto parsed = std::from_chars(text.data() + i, text.data() + i + 4, unit, 16);
        REQUIRE(parsed.ec == std::errc{});
        result.push_back(static_cast<char16_t>(unit));
    }

    return result;
}

std::uint64_t bits(std::string_view text) {
    std::uint64_t result = 0;
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), result, 16);
    REQUIRE(parsed.ec == std::errc{});

    return result;
}
} // namespace

TEST_CASE("Pinned Java IPF numeric and date oracle") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_IPF_FIXTURE);
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
        CAPTURE(columns[3]);
        auto name = decode(columns[1]);
        InstrumentProfile profile;
        std::expected<void, ProfileError> edit;

        if (columns[0] == "format") {
            edit = profile.setNumericField(name, std::bit_cast<double>(bits(columns[2])));
        } else if (columns[0] == "date-format") {
            edit = profile.setDateField(name, static_cast<std::int32_t>(std::stoll(columns[2])));
        } else {
            edit = profile.setField(name, decode(columns[2]));
        }

        if (columns[0] == "number") {
            auto result = edit ? profile.getNumericField(name)
                               : std::expected<double, ProfileError>{std::unexpected(edit.error())};

            if (columns[3] == "ERROR") {
                CHECK_FALSE(result);
            } else {
                REQUIRE(result);
                auto actual =
                    std::isnan(*result) ? std::uint64_t{0x7ff8000000000000} : std::bit_cast<std::uint64_t>(*result);
                CHECK(actual == bits(columns[3]));
            }
        } else if (columns[0] == "date") {
            auto result = edit ? profile.getDateField(name)
                               : std::expected<std::int32_t, ProfileError>{std::unexpected(edit.error())};

            if (columns[3] == "ERROR") {
                CHECK_FALSE(result);
            } else {
                REQUIRE(result);
                CHECK(*result == std::stoll(columns[3]));
            }
        } else {
            if (columns[3] == "ERROR") {
                CHECK_FALSE(edit);
            } else {
                REQUIRE(edit);
                auto actualText = profile.getField(name);
                INFO(std::string(actualText.begin(), actualText.end()));
                CHECK(actualText == decode(columns[3]));
            }
        }

        ++rows;
    }

    CHECK(rows > 5000);
}

TEST_CASE("Standard typed accessors and exact custom ownership") {
    InstrumentProfile profile;
    CHECK(standardFields().size() == 31);
    CHECK(standardTypes().size() == 17);

    for (auto field : standardFields()) {
        CHECK(profile.getField(field.name).empty());
        CHECK(findField(field.name) == &standardFields()[static_cast<std::size_t>(field.field)]);
    }

    CHECK(findField(u"symbol") == nullptr);
    REQUIRE(profile.setType(u"CUSTOM_TYPE"));
    REQUIRE(profile.setSymbol(u"IBM"));
    REQUIRE(profile.setIcb(123));
    REQUIRE(profile.setSic(456));
    REQUIRE(profile.setMultiplier(2.5));
    REQUIRE(profile.setSpc(100));
    REQUIRE(profile.setStrike(-0.0));
    REQUIRE(profile.setExpiration(1));
    REQUIRE(profile.setLastTrade(-1));
    CHECK(profile.getType() == u"CUSTOM_TYPE");
    CHECK(profile.getSymbol() == u"IBM");
    CHECK(profile.getIcb() == 123);
    CHECK(profile.getSic() == 456);
    CHECK(profile.getMultiplier() == 2.5);
    CHECK(profile.getSpc() == 100);
    CHECK(std::signbit(profile.getStrike()));
    CHECK(profile.getExpiration() == 1);
    CHECK(profile.getLastTrade() == -1);
    std::u16string name{u'X', 0, 0xd800}, value{u'v', 0, 0xdc00};
    REQUIRE(profile.setField(name, value));
    auto copy = profile;
    name[0] = u'Y';
    value[0] = u'w';
    CHECK(profile.getField(std::u16string{u'X', 0, 0xd800}) == std::u16string{u'v', 0, 0xdc00});
    REQUIRE(profile.setField(u"other", u"1"));
    CHECK(copy.getField(u"other").empty());
    REQUIRE(profile.setField(u"", u"empty-name"));
    REQUIRE(profile.setField(u"symbol", u"case-sensitive"));
    CHECK(profile.getSymbol() == u"IBM");
    REQUIRE(profile.setField(u"symbol", nullptr));
    CHECK(profile.getField(u"symbol").empty());
    REQUIRE(profile.setSymbol(nullptr));
    CHECK(profile.getSymbol().empty());
    CHECK_FALSE(profile.sameSymbol(copy));
    auto moved = std::move(copy);
    CHECK(moved.getSymbol() == u"IBM");
    CHECK(copy.getSymbol().empty());
    CHECK(copy.customFieldNames().empty());
    copy = moved;
    CHECK(copy == moved);
    REQUIRE(copy.setDescription(u"independent"));
    CHECK_FALSE(copy == moved);
}

TEST_CASE("Finite per-instance limits and atomic conversion failures") {
    InstrumentProfile profile({1, 32, 8, 12, 16});
    REQUIRE(profile.setField(u"X", u"12345678"));
    auto before = profile;
    CHECK(profile.setField(u"Y", u"1").error() == ProfileError::LIMIT_EXCEEDED);
    CHECK(profile == before);
    CHECK(profile.setField(u"X", u"123456789").error() == ProfileError::LIMIT_EXCEEDED);
    CHECK(profile == before);
    CHECK(profile.setField(u"STRIKE", u"invalid").error() == ProfileError::INVALID_NUMBER);
    CHECK(profile == before);
    CHECK(profile.setField(u"EXPIRATION", u"invalid").error() == ProfileError::INVALID_DATE);
    CHECK(profile == before);
    CHECK(profile.setNumericField(u"SYMBOL", 1).error() == ProfileError::TEXTUAL_FIELD);
    CHECK(profile == before);
    CHECK(profile.setNumericField(u"STRIKE", std::numeric_limits<double>::max()).error() ==
          ProfileError::LIMIT_EXCEEDED);
    CHECK(profile == before);
    REQUIRE(profile.setField(u"X", nullptr));
    CHECK(profile.customFieldNames().empty());
    REQUIRE(profile.setField(u"Y", u"1"));
    InstrumentProfile other({0, 32, 1, 1, 1});
    REQUIRE(other.setSymbol(u"A"));
    CHECK(other.getSymbol() == u"A");
    CHECK(other.setField(u"X", u"1").error() == ProfileError::LIMIT_EXCEEDED);
    CHECK(profile.setField(u"SYMBOL", std::u16string(17, u'x')).error() == ProfileError::LIMIT_EXCEEDED);
    auto maximum = std::numeric_limits<std::size_t>::max();
    InstrumentProfile hugeLimits({maximum, maximum, maximum, maximum, maximum});
    REQUIRE(hugeLimits.setField(u"X", u"1"));
    InstrumentProfile empty({0, 0, 0, 0, 0});
    CHECK(empty.getField(u"missing").empty());
    CHECK(empty.setSymbol(u"A").error() == ProfileError::LIMIT_EXCEEDED);
}

TEST_CASE("Profile value equality and presentation order differ from exact symbol identity") {
    InstrumentProfile first, second;
    CHECK(first.sameSymbol(second));
    REQUIRE(first.setType(u"STOCK"));
    REQUIRE(second.setType(u"OPTION"));
    CHECK(first.sameSymbol(second));
    CHECK(first.compareTo(second) < 0);
    REQUIRE(second.setType(u"STOCK"));
    REQUIRE(first.setSymbol(u"A"));
    REQUIRE(second.setSymbol(u"a"));
    CHECK_FALSE(first.sameSymbol(second));
    CHECK(first.compareTo(second) < 0);
    second = first;
    REQUIRE(first.setStrike(-0.0));
    CHECK_FALSE(first == second);
    CHECK(first.compareTo(second) == 0);
    REQUIRE(first.setStrike(std::numeric_limits<double>::quiet_NaN()));
    REQUIRE(second.setStrike(std::nan("7")));
    CHECK(first == second);
    CHECK(compareTypes(u"STOCK", u"unknown") < 0);
    CHECK(compareTypes(u"a", u"b") < 0);
}

TEST_CASE("Aggregate retained UTF16 bound refuses an otherwise admissible edit atomically") {
    InstrumentProfile profile({2, 32, 8, 12, 16});
    REQUIRE(profile.setField(u"X", u"12345678"));
    auto before = profile;
    auto namesBefore = profile.customFieldNames();

    // X retains nine units; four SYMBOL units would exceed the aggregate twelve-unit bound.
    CHECK(profile.setSymbol(u"ABCD").error() == ProfileError::LIMIT_EXCEEDED);
    CHECK(profile == before);
    CHECK(profile.customFieldNames() == namesBefore);

    // Three units exactly fill the remaining capacity, proving the refused edit charged nothing.
    REQUIRE(profile.setSymbol(u"ABC"));
    CHECK(profile.getSymbol() == u"ABC");
    CHECK(profile.getField(u"X") == u"12345678");
}

TEST_CASE("Overlong custom name refuses a small value without charging its resource ledger") {
    InstrumentProfile profile({2, 4, 8, 12, 16});
    REQUIRE(profile.setField(u"KEY", u"v"));
    auto before = profile;
    auto namesBefore = profile.customFieldNames();

    // Five name units exceed only the name bound; count, value and aggregate would otherwise fit.
    CHECK(profile.setField(u"LONG5", u"x").error() == ProfileError::LIMIT_EXCEEDED);
    CHECK(profile == before);
    CHECK(profile.customFieldNames() == namesBefore);

    // Four name plus four value units exactly use the eight uncharged remaining units.
    REQUIRE(profile.setField(u"ABCD", u"1234"));
    CHECK(profile.customFieldNames().size() == 2);
    CHECK(profile.getField(u"KEY") == u"v");
    CHECK(profile.getField(u"ABCD") == u"1234");
}

TEST_CASE("Precutover input year retains Julian arithmetic after day overflow") {
    const std::array<std::pair<std::u16string_view, std::int32_t>, 4> cases{
        {{u"1583-01--100", -141440}, {u"1582-09-40", -141422}, {u"1582-01-365", -141350}, {u"1581-12-400", -141336}}};

    for (const auto &[text, expected] : cases) {
        InstrumentProfile profile;
        REQUIRE(profile.setField(u"EXPIRATION", text));
        CHECK(profile.getExpiration() == expected);
    }
}

TEST_CASE("Twenty fractional digits round the binary value rather than its rounded shortest text") {
    InstrumentProfile profile;
    REQUIRE(profile.setStrike(std::bit_cast<double>(std::uint64_t{0x3ee9e4093026776e})));
    CHECK(profile.getField(u"STRIKE") == u"0.00001234567890123401");
}
