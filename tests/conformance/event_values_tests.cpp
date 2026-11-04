// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/event_flags.hpp"
#include "qd/events/market_enums.hpp"
#include "qd/events/profile.hpp"
#include "qd/events/short_string.hpp"
#include "qd/events/time_helpers.hpp"
#include "qd/events/trade.hpp"

#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace e = org::ttldtor::qdcxx::events;

/** @param text Fixture signed decimal cell.
 * @return Exact signed64 fixture value; malformed cells throw.
 */
std::int64_t number(const std::string &text) {
    std::size_t consumed = 0;
    const auto value = std::stoll(text, &consumed);

    REQUIRE(consumed == text.size());

    return value;
}

/** @param text Fixture nullable UTF-16 hex cell.
 * @return Owned nullable exact UTF-16 units; no Unicode normalization.
 */
std::optional<std::u16string> units(const std::string &text) {
    if (text == "null") {
        return std::nullopt;
    }

    if (text == "-") {
        return std::u16string{};
    }

    REQUIRE(text.size() % 4 == 0);
    std::u16string value;

    for (std::size_t index = 0; index < text.size(); index += 4) {
        value.push_back(static_cast<char16_t>(std::stoul(text.substr(index, 4), nullptr, 16)));
    }

    return value;
}

/** @tparam Enum Supported market enum.
 * @param raw Exact source code, including unknown values.
 * @param expected Actual Java valueOf result or error cell.
 */
template <e::MarketEnum Enum> void checkEnum(std::int32_t raw, const std::string &expected) {
    const auto retained = e::marketEnumFromCode<Enum>(raw);
    const auto value = e::marketEnumValue(retained);

    CHECK(e::marketEnumCode(retained) == raw);

    if (expected == "error") {
        REQUIRE_FALSE(value);
        CHECK(value.error() == e::EventValueError::INVALID_ENUM_CODE);
        CHECK_FALSE(e::isKnownMarketEnum(retained));
    } else {
        REQUIRE(value);
        CHECK(e::marketEnumCode(*value) == number(expected));
        CHECK(e::isKnownMarketEnum(retained) == (number(expected) == raw));
    }
}

/** @param type Actual production enum type name.
 * @param raw Input raw code.
 * @param expected Actual Java projection or error.
 */
void checkEnumType(const std::string &type, std::int32_t raw, const std::string &expected) {
    if (type == "Direction") {
        checkEnum<e::Direction>(raw, expected);
    } else if (type == "TradingStatus") {
        checkEnum<e::TradingStatus>(raw, expected);
    } else if (type == "ShortSaleRestriction") {
        checkEnum<e::ShortSaleRestriction>(raw, expected);
    } else if (type == "Side") {
        checkEnum<e::Side>(raw, expected);
    } else if (type == "Scope") {
        checkEnum<e::Scope>(raw, expected);
    } else if (type == "OrderAction") {
        checkEnum<e::OrderAction>(raw, expected);
    } else if (type == "IcebergType") {
        checkEnum<e::IcebergType>(raw, expected);
    } else if (type == "PriceType") {
        checkEnum<e::PriceType>(raw, expected);
    } else if (type == "OtcMarketsPriceType") {
        checkEnum<e::OtcMarketsPriceType>(raw, expected);
    } else if (type == "TimeAndSaleType") {
        checkEnum<e::TimeAndSaleType>(raw, expected);
    } else if (type == "AuctionType") {
        checkEnum<e::AuctionType>(raw, expected);
    } else if (type == "ImbalanceSide") {
        checkEnum<e::ImbalanceSide>(raw, expected);
    } else {
        FAIL("Unrecognized production enum type");
    }
}

/** @tparam Integer Signed32 or signed64 expected result.
 * @param result Checked native result.
 * @param expected Production result/error cell.
 * @param error Required native error for Java input rejection.
 */
template <typename Integer>
void checkedResult(const std::expected<Integer, e::EventValueError> &result, const std::string &expected,
                   e::EventValueError error) {
    if (expected == "error") {
        REQUIRE_FALSE(result);
        CHECK(result.error() == error);
    } else {
        REQUIRE(result);
        CHECK(*result == number(expected));
    }
}
} // namespace

TEST_CASE("public event values match actual pinned Java production operations") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_EVENT_VALUES_FIXTURE);
    REQUIRE(input.is_open());
    std::map<std::string, std::size_t> counts;
    std::string line;

    while (std::getline(input, line)) {
        std::vector<std::string> cells;
        std::istringstream stream(line);
        std::string cell;

        while (std::getline(stream, cell, '\t')) {
            cells.push_back(cell);
        }

        REQUIRE(cells.size() >= 3);
        const auto &kind = cells[0];
        CAPTURE(kind);
        CAPTURE(counts[kind]);
        ++counts[kind];

        if (kind == "enum") {
            REQUIRE(cells.size() == 4);
            checkEnumType(cells[1], static_cast<std::int32_t>(number(cells[2])), cells[3]);
        } else if (kind == "flags") {
            REQUIRE(cells.size() == 7);
            const std::array flags{e::indexed_event_flags::TX_PENDING,     e::indexed_event_flags::REMOVE_EVENT,
                                   e::indexed_event_flags::SNAPSHOT_BEGIN, e::indexed_event_flags::SNAPSHOT_END,
                                   e::indexed_event_flags::SNAPSHOT_SNIP,  e::indexed_event_flags::SNAPSHOT_MODE};

            for (std::size_t i = 0; i < flags.size(); ++i) {
                CHECK(flags[i] == number(cells[i + 1]));
            }
        } else if (kind == "bits32") {
            REQUIRE(cells.size() == 7);
            const auto flags = static_cast<std::int32_t>(number(cells[1]));
            const auto mask = static_cast<std::int32_t>(number(cells[2]));
            const auto shift = static_cast<std::int32_t>(number(cells[3]));
            const auto bits = static_cast<std::int32_t>(number(cells[4]));
            CHECK(e::getBits(flags, mask, shift) == number(cells[5]));
            CHECK(e::setBits(flags, mask, shift, bits) == number(cells[6]));
        } else if (kind == "bits64") {
            REQUIRE(cells.size() == 7);
            const auto shift = static_cast<std::int32_t>(number(cells[3]));
            CHECK(e::getBits(number(cells[1]), number(cells[2]), shift) == number(cells[5]));
            CHECK(e::setBits(number(cells[1]), number(cells[2]), shift, number(cells[4])) == number(cells[6]));
        } else if (kind == "nanos") {
            REQUIRE(cells.size() == 4);
            CHECK(e::getMillisFromNanos(number(cells[1])) == number(cells[2]));
            CHECK(e::getNanoPartFromNanos(number(cells[1])) == number(cells[3]));
        } else if (kind == "time") {
            REQUIRE(cells.size() == 4);
            CHECK(e::getSecondsFromTime(number(cells[1])) == number(cells[2]));
            CHECK(e::getMillisFromTime(number(cells[1])) == number(cells[3]));
        } else if (kind == "combine") {
            REQUIRE(cells.size() == 4);
            CHECK(e::getNanosFromMillisAndNanoPart(number(cells[1]), static_cast<std::int32_t>(number(cells[2]))) ==
                  number(cells[3]));
        } else if (kind == "packed") {
            REQUIRE(cells.size() == 4);
            CHECK(e::getTimeFromTimeSequence(number(cells[1])) == number(cells[2]));
            CHECK(e::getSequenceFromTimeSequence(number(cells[1])) == number(cells[3]));
            e::Trade trade;
            trade.timeSequence = number(cells[1]);
            CHECK(trade.getTime() == number(cells[2]));
            CHECK(trade.getSequence() == number(cells[3]));
        } else if (kind == "settime") {
            REQUIRE(cells.size() == 4);
            CHECK(e::withTimeSequenceTime(number(cells[1]), number(cells[2])) == number(cells[3]));
        } else if (kind == "setseq") {
            REQUIRE(cells.size() == 4);
            checkedResult(e::withTimeSequenceSequence(number(cells[1]), static_cast<std::int32_t>(number(cells[2]))),
                          cells[3], e::EventValueError::INVALID_SEQUENCE);
        } else if (kind == "day") {
            REQUIRE(cells.size() == 5);
            checkedResult(e::getDayIdByYearMonthDay(static_cast<std::int32_t>(number(cells[1])),
                                                    static_cast<std::int32_t>(number(cells[2])),
                                                    static_cast<std::int32_t>(number(cells[3]))),
                          cells[4], e::EventValueError::INVALID_MONTH);
        } else if (kind == "date") {
            REQUIRE(cells.size() == 3);
            checkedResult(e::getDayIdByYearMonthDay(static_cast<std::int32_t>(number(cells[1]))), cells[2],
                          e::EventValueError::INVALID_MONTH);
        } else if (kind == "calendar") {
            REQUIRE(cells.size() == 3);
            CHECK(e::getYearMonthDayByDayId(static_cast<std::int32_t>(number(cells[1]))) == number(cells[2]));
        } else if (kind == "encode") {
            REQUIRE(cells.size() == 4);
            const auto source = units(cells[1]);
            const auto result =
                e::encodeShortString(source ? std::optional<std::u16string_view>(*source) : std::nullopt);

            if (cells[2] == "error") {
                REQUIRE_FALSE(result);
                REQUIRE(source);
                CHECK(result.error() == (source->size() > 8 ? e::EventValueError::SHORT_STRING_TOO_LONG
                                                            : e::EventValueError::INVALID_SHORT_STRING_CHARACTER));
            } else {
                REQUIRE(result);
                CHECK(*result == number(cells[2]));
                CHECK(e::decodeShortString(*result) == units(cells[3]));
            }
        } else if (kind == "decode") {
            REQUIRE(cells.size() == 3);
            CHECK(e::decodeShortString(number(cells[1])) == units(cells[2]));
        } else if (kind == "field") {
            REQUIRE(cells.size() == 4);
            const auto source = units(cells[1]);
            const auto encoded =
                e::encodeShortString(source ? std::optional<std::u16string_view>(*source) : std::nullopt);
            REQUIRE(encoded);
            const auto raw = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(*encoded));
            CHECK(raw == number(cells[2]));
            CHECK(e::decodeShortString(raw) == units(cells[3]));
        } else if (kind == "schema") {
            REQUIRE(cells.size() == 4);
            CHECK(cells[1] == "TimeAndSale");
            CHECK(cells[2] == "TimeAndSale.ExchangeSaleConditions");
            CHECK(number(cells[3]) == 40);
        } else {
            FAIL("Unrecognized oracle row kind");
        }
    }

    CHECK(input.eof());
    CHECK(counts.size() == 17);
    CHECK(counts["enum"] == 12 * 19);
    CHECK(counts["bits32"] == 5760);
    CHECK(counts["bits64"] == 6720);
    CHECK(counts["day"] == 3664);
    CHECK(counts["calendar"] == 1049);
    CHECK(counts["decode"] == 2055);
    CHECK(counts["schema"] == 1);
}

TEST_CASE("raw enum and flags domains survive sentinel projections and unrelated edits") {
    static_assert(e::marketEnumCode(e::marketEnumFromCode<e::Direction>(-1)) == -1);
    static_assert(e::marketEnumValue(e::marketEnumFromCode<e::Direction>(7)) == e::Direction::UNDEFINED);
    static_assert(!e::MarketEnum<int>);
    const auto raw = e::marketEnumFromCode<e::OrderAction>(std::numeric_limits<std::int32_t>::min());
    CHECK(e::marketEnumCode(raw) == std::numeric_limits<std::int32_t>::min());
    CHECK_FALSE(e::marketEnumValue(raw));

    e::Trade trade;
    trade.flags = -1;
    CHECK(trade.getTickDirection() == e::Direction::UNDEFINED);
    CHECK(trade.flags == -1);
    e::Profile profile;
    profile.flags = -1;
    CHECK(profile.tradingStatus() == e::TradingStatus::UNDEFINED);
    CHECK(profile.shortSaleRestriction() == e::ShortSaleRestriction::UNDEFINED);
    CHECK(profile.flags == -1);

    const auto changed = e::setBits(std::int32_t{-1}, std::int32_t{7}, 1, std::int32_t{5});
    CHECK((static_cast<std::uint32_t>(changed) & ~std::uint32_t{14}) == (~std::uint32_t{0} & ~std::uint32_t{14}));
    CHECK(e::getBits(changed, std::int32_t{7}, 1) == 5);
    CHECK(e::getMillisFromNanos(-1) == -1);
    CHECK(e::getNanoPartFromNanos(-1) == 999999);
    CHECK(e::getDayIdByYearMonthDay(1970, 1, 1) == 0);
    CHECK(e::getDayIdByYearMonthDay(2000, 2, 30) == e::getDayIdByYearMonthDay(2000, 3, 1));
    CHECK(e::getYearMonthDayByDayId(-1) == 19691231);
    CHECK(e::encodeShortString(u"AB") == 0x4142);
    CHECK(e::decodeShortString(0x410042) == std::optional<std::u16string>(u"AB"));
}
