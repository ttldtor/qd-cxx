// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/event_text.hpp"
#include "qd/events/indexed_event_source.hpp"
#include "qd/events/order_source.hpp"
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/events/summary.hpp"
#include "qd/events/time_helpers.hpp"
#include "qd/events/trade.hpp"
#include "qd/events/trade_eth.hpp"
#include "qd/time/time_zone_data.hpp"
#include <array>
#include <bit>
#include <doctest/doctest.h>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace {
namespace e = org::ttldtor::qdcxx::events;
constexpr e::EventTextLimits LIMITS{65536, 65536, 262144, 1000000}; ///< Finite per-call test allowances.

/** @param text ASCII fixture cell.
 * @return Exact owned UTF-16 ASCII copy.
 */
std::u16string ascii(std::string_view text) {
    return {text.begin(), text.end()};
}

/** @param text Comma-separated four-digit UTF-16 units; empty means empty text.
 * @return Owned exact UTF-16, including NUL and individual surrogates.
 */
std::u16string units(std::string_view text) {
    std::u16string result;

    for (std::size_t i = 0; i < text.size(); i += 5) {
        result.push_back(static_cast<char16_t>(std::stoul(std::string(text.substr(i, 4)), nullptr, 16)));
    }

    return result;
}

#ifdef ORG_TTLDTOR_QDCXX_EVENT_TEXT_ZONE_ROOT
/** @return Owned test contexts; the original data handle and ID strings are destroyed before any formatting. */
const std::map<std::string, e::EventTextContext> &namedContexts() {
    static const auto contexts = [] {
        auto data = org::ttldtor::qdcxx::time::TimeZoneData::initialize(ORG_TTLDTOR_QDCXX_EVENT_TEXT_ZONE_ROOT);
        REQUIRE(data);
        std::map<std::string, e::EventTextContext> result;

        for (const auto *name : {"America/New_York", "Europe/Berlin", "Asia/Kathmandu", "Australia/Lord_Howe",
                                 "Europe/Dublin", "Pacific/Apia", "SystemV/EST5EDT", "Asia/Tokyo"}) {
            auto id = ascii(name);
            auto ctx = e::EventTextContext::timeZone(&*data, id, 0);
            REQUIRE(ctx);
            result.emplace(name, std::move(*ctx));
        }

        return result;
    }();

    return contexts;
}

constexpr std::size_t NAMED_PROFILES = 8;
#else
constexpr std::size_t NAMED_PROFILES = 0;
#endif

/** @param name Fixture profile.
 * @return Whether this build can exercise the profile; named coverage is mandatory in the backend build.
 */
bool available(std::string_view name) {
    return name == "UTC" || name.starts_with("fixed:") || NAMED_PROFILES != 0;
}

/** @param name Exact UTC or fixed signed32 offset profile.
 * @return Independent explicit context.
 */
e::EventTextContext context(std::string_view name) {
    if (name == "UTC") {
        return e::EventTextContext::utc();
    }

#ifdef ORG_TTLDTOR_QDCXX_EVENT_TEXT_ZONE_ROOT

    if (!name.starts_with("fixed:")) {
        return namedContexts().at(std::string(name));
    }

#endif
    REQUIRE(name.starts_with("fixed:"));

    return e::EventTextContext::fixedOffset(static_cast<std::int32_t>(std::stoll(std::string(name.substr(6)))));
}

/** @param filled Select the exact filled production Java input.
 * @return Owned Quote; empty symbol matches Java empty string, not Java null.
 */
e::Quote quote(bool filled) {
    e::Quote v{};

    if (filled) {
        v.symbol = {u'A', 0, 0xd800, u'\'', u'{', u'}'};
        v.eventTime = -1;
        v.bidTimeMs = 1234567890123;
        v.askTimeMs = 1234567890999;
        v.timeMs = 1234567890999;
        v.sequence = 4194303;
        v.timeNanoPart = -17;
        v.askExchangeCode = 0xdfff;
        v.businessNumericSnapshot.emplace();
        auto &n = *v.businessNumericSnapshot;
        n[0].directValue = -0.0;
        n[1].directValue = 0.001;
        n[2].directValue = std::numeric_limits<double>::infinity();
    }

    return v;
}

/** @param filled Select the filled production Java Profile input.
 * @return Complete owned event with direct business binary64 values.
 */
e::Profile profile(bool filled) {
    e::Profile v{};

    if (filled) {
        v.symbol = {u'P', u'\'', 0xdfff};
        v.eventTime = 1;
        v.description = std::u16string(u"quotes'\n\0", 9);
        v.haltStartTimeMs = -1;
        v.haltEndTimeMs = 1234567890123;
        v.exDividendDayId = -1;
        v.businessNumericSnapshot.emplace();
        auto &n = *v.businessNumericSnapshot;
        n[0].directValue = -0.0;
        n[1].directValue = -std::numeric_limits<double>::infinity();
        n[2].directValue = 1e7;
        n[3].directValue = std::numeric_limits<double>::denorm_min();
        n[6].directValue = 1234567890123456.5;
    }

    return v;
}

/** @param filled Select the exact production Java Summary public-setter input.
 * @return Complete owned Summary, retaining full signed64 OpenInterest separately from business doubles.
 */
e::Summary summary(bool filled) {
    e::Summary value{};

    if (filled) {
        value.symbol = {u'S', 0, 0xd800, u'\'', u'{', u'}'};
        value.eventTime = -1;
        value.dayId = std::numeric_limits<std::int32_t>::min();
        value.prevDayId = std::numeric_limits<std::int32_t>::max();
        value.openInterest = 3000000000LL;
        value.flags = 14;
        value.businessNumericSnapshot.emplace();
        auto &numbers = *value.businessNumericSnapshot;
        numbers[0].directValue = -0.0;
        numbers[1].directValue = std::numeric_limits<double>::infinity();
        numbers[2].directValue = -std::numeric_limits<double>::infinity();
        numbers[3].directValue = std::numeric_limits<double>::denorm_min();
        numbers[4].directValue = 1e7;
        numbers[5].directValue = 1234567890123456.5;
    }

    return value;
}

/** @tparam Event Trade or TradeETH.
 * @param filled Select the shared TradeBase setter input.
 * @return Complete owned event preserving direct business values.
 */
template <typename Event> Event trade(bool filled) {
    Event v{};

    if (filled) {
        v.symbol = {u'T', 0, 0xd800};
        v.eventTime = -1;
        v.timeSequence = *e::withTimeSequenceSequence(e::withTimeSequenceTime(0, -1), 4194303);
        v.timeNanoPart = -17;
        v.exchangeCode = 0xdfff;
        v.tradeId = std::numeric_limits<std::int64_t>::min();
        v.dayId = std::numeric_limits<std::int32_t>::min();
        v.flags = 9;
        v.businessNumericSnapshot.emplace();
        auto &n = *v.businessNumericSnapshot;
        n[0].directValue = -0.0;
        n[1].directValue = std::numeric_limits<double>::infinity();
        n[2].directValue = std::numeric_limits<double>::denorm_min();
        n[3].directValue = 1234567890123456.5;
    }

    return v;
}

/** @param type Exact supported Java enum name.
 * @param code Known production ordinal.
 * @return Public enum text; unknown type is a malformed fixture.
 */
std::expected<std::u16string, e::EventTextError> enumText(std::string_view type, std::int32_t code) {
    if (type == "Direction") {
        return e::toString(static_cast<e::Direction>(code), LIMITS);
    }

    if (type == "TradingStatus") {
        return e::toString(static_cast<e::TradingStatus>(code), LIMITS);
    }

    if (type == "ShortSaleRestriction") {
        return e::toString(static_cast<e::ShortSaleRestriction>(code), LIMITS);
    }

    if (type == "Side") {
        return e::toString(static_cast<e::Side>(code), LIMITS);
    }

    if (type == "Scope") {
        return e::toString(static_cast<e::Scope>(code), LIMITS);
    }

    if (type == "OrderAction") {
        return e::toString(static_cast<e::OrderAction>(code), LIMITS);
    }

    if (type == "IcebergType") {
        return e::toString(static_cast<e::IcebergType>(code), LIMITS);
    }

    if (type == "PriceType") {
        return e::toString(static_cast<e::PriceType>(code), LIMITS);
    }

    if (type == "OtcMarketsPriceType") {
        return e::toString(static_cast<e::OtcMarketsPriceType>(code), LIMITS);
    }

    if (type == "TimeAndSaleType") {
        return e::toString(static_cast<e::TimeAndSaleType>(code), LIMITS);
    }

    if (type == "AuctionType") {
        return e::toString(static_cast<e::AuctionType>(code), LIMITS);
    }

    REQUIRE(type == "ImbalanceSide");

    return e::toString(static_cast<e::ImbalanceSide>(code), LIMITS);
}
} // namespace

TEST_CASE("actual Java25 value and complete event text under explicit contexts") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_EVENT_TEXT_FIXTURE);
    REQUIRE(input.good());
    std::ifstream proof(ORG_TTLDTOR_QDCXX_EVENT_TEXT_CACHE_PROOF);
    REQUIRE(proof.good());
    std::map<std::string, std::pair<std::string, std::string>> cacheProof;
    std::string line;

    while (std::getline(proof, line)) {
        std::array<std::string, 5> cells;
        std::istringstream fields(line);

        for (auto &cell : cells) {
            std::getline(fields, cell, '\t');
        }

        REQUIRE(cells[4] == "TimeFormat-UTC-second-cache-with-subsecond-offset");
        cacheProof.emplace(cells[0] + ":" + cells[1], std::pair(cells[2], cells[3]));
    }

    REQUIRE(cacheProof.size() == 3);
    std::size_t usedCacheProof = 0;
    std::size_t rows = 0, doubles = 0, chars = 0, events = 0;

    while (std::getline(input, line)) {
        std::array<std::string, 4> cells;
        std::istringstream stream(line);

        for (auto &cell : cells) {
            std::getline(stream, cell, '\t');
        }

        if (!available(cells[0])) {
            continue;
        }

        ++rows;
        const auto &kind = cells[1];
        const auto &arg = cells[2];
        const auto ctx = context(cells[0]);
        std::expected<std::u16string, e::EventTextError> actual;
        bool hex = false;

        if (kind == "double") {
            ++doubles;
            actual = e::formatEventDouble(std::bit_cast<double>(std::stoull(arg, nullptr, 16)), LIMITS);
        } else if (kind == "wide") {
            auto v = profile(false);
            v.businessNumericSnapshot.emplace();
            (*v.businessNumericSnapshot)[6].directValue = std::bit_cast<double>(std::stoull(arg, nullptr, 16));
            const auto complete = e::toString(v, ctx, LIMITS);
            REQUIRE(complete);
            const auto start = complete->find(u", shares=") + std::u16string_view(u", shares=").size();
            const auto end = complete->find(u", freeFloat=", start);
            actual = complete->substr(start, end - start);
        } else if (kind == "char") {
            ++chars;
            actual = e::encodeEventChar(static_cast<char16_t>(std::stoul(arg)), LIMITS);
        } else if (kind == "flags") {
            actual = e::formatEventFlags(static_cast<std::int32_t>(std::stoll(arg)), LIMITS);
        } else if (kind == "enum") {
            const auto split = arg.find(':');
            actual = enumText(arg.substr(0, split), std::stoi(arg.substr(split + 1)));
        } else if (kind == "orderSource") {
            hex = true;
            const auto name = ascii(arg);
            actual = e::toString(e::OrderSource::valueOf(std::u16string_view(name)), LIMITS);
        } else if (kind == "source") {
            hex = true;

            if (arg == "null") {
                const auto result = e::toString(e::IndexedEventSource(-1, std::nullopt), LIMITS);
                REQUIRE(result);
                CHECK_FALSE(result->has_value());
                CHECK(units(cells[3]) == u"null");
                continue;
            }

            const auto name = arg == "empty"     ? std::u16string{}
                              : arg == "literal" ? std::u16string(u"null")
                                                 : std::u16string{u'A', 0, 0xd800, u'\'', u'{', u'}'};
            const auto result = e::toString(e::IndexedEventSource(7, name), LIMITS);
            REQUIRE(result);
            REQUIRE(result->has_value());
            actual = **result;
        } else if (kind == "millis" || kind == "seconds" || kind == "secondsFresh") {
            actual = e::formatEventTime(std::stoll(arg), kind == "millis", ctx, LIMITS);
        } else {
            hex = true;
            ++events;

            if (kind == "summary") {
                actual = e::toString(summary(arg == "filled"), ctx, LIMITS);
            } else if (kind == "summaryInterest") {
                auto value = summary(false);
                value.symbol = u"interest";
                value.openInterest = std::stoll(arg);
                actual = e::toString(value, ctx, LIMITS);
            } else if (kind == "summaryFlags") {
                auto value = summary(false);
                value.symbol = u"flags";
                value.flags = static_cast<std::int32_t>(std::stoll(arg));
                actual = e::toString(value, ctx, LIMITS);
            } else if (kind == "profileFlags") {
                auto value = profile(false);
                value.symbol = u"flags";
                value.flags = static_cast<std::int32_t>(std::stoll(arg));
                actual = e::toString(value, ctx, LIMITS);
            } else if (kind == "tradeFlags") {
                auto value = trade<e::Trade>(false);
                value.symbol = u"flags";
                value.flags = static_cast<std::int32_t>(std::stoll(arg));
                actual = e::toString(value, ctx, LIMITS);
            } else if (kind == "tradeETHFlags") {
                auto value = trade<e::TradeETH>(false);
                value.symbol = u"flags";
                value.flags = static_cast<std::int32_t>(std::stoll(arg));
                actual = e::toString(value, ctx, LIMITS);
            } else if (kind == "quote") {
                actual = e::toString(quote(arg == "filled"), ctx, LIMITS);
            } else if (kind == "profile") {
                actual = e::toString(profile(arg == "filled"), ctx, LIMITS);
            } else if (kind == "trade") {
                actual = e::toString(trade<e::Trade>(arg == "filled"), ctx, LIMITS);
            } else {
                REQUIRE(kind == "tradeETH");
                actual = e::toString(trade<e::TradeETH>(arg == "filled"), ctx, LIMITS);
            }
        }

        INFO("row=" << rows << " profile=" << cells[0] << " kind=" << kind << " input=" << arg);
        REQUIRE(actual);
        const auto deviation = kind == "seconds" ? cacheProof.find(cells[0] + ":" + arg) : cacheProof.end();

        if (deviation != cacheProof.end()) {
            ++usedCacheProof;
            CHECK(cells[3] == deviation->second.first);
            CHECK(*actual == ascii(deviation->second.second));
        } else {
            CHECK(*actual == (hex ? units(cells[3]) : ascii(cells[3])));
        }
    }

    CHECK(input.eof());
    CHECK(usedCacheProof == cacheProof.size());
    CHECK(doubles >= 50000);
    CHECK(chars == 65536);
    CHECK(events == 576 + NAMED_PROFILES * 96);
}

TEST_CASE("finite input output and work allowances reject atomically") {
    const auto ctx = e::EventTextContext::utc();
    const auto v = quote(true);
    const auto expected = e::toString(v, ctx, LIMITS);
    REQUIRE(expected);
    auto limits = LIMITS;
    limits.inputUnits = v.symbol.size();
    limits.outputUnits = expected->size();
    CHECK(e::toString(v, ctx, limits) == expected);
    --limits.outputUnits;
    CHECK(e::toString(v, ctx, limits).error() == e::EventTextError::RESOURCE_LIMIT);
    limits = LIMITS;
    limits.inputUnits = v.symbol.size() - 1;
    CHECK(e::toString(v, ctx, limits).error() == e::EventTextError::RESOURCE_LIMIT);
    limits = LIMITS;
    limits.work = 130;
    CHECK(e::encodeEventChar(0, limits) == std::u16string(u"\\0"));
    limits.work = 129;
    CHECK(e::encodeEventChar(0, limits).error() == e::EventTextError::RESOURCE_LIMIT);
    CHECK(e::toString(static_cast<e::Direction>(7), LIMITS).error() == e::EventTextError::INVALID_ENUM_CODE);
    CHECK(e::toString(static_cast<e::OrderAction>(-1), LIMITS).error() == e::EventTextError::INVALID_ENUM_CODE);
}

TEST_CASE("UTF8 conversion preserves valid pairs and has explicit malformed-unit policy") {
    CHECK(e::toUtf8(u"valid", LIMITS, static_cast<e::MalformedUtf16>(-1)).error() ==
          e::EventTextError::INVALID_CONTEXT);
    const std::u16string paired{u'A', 0, 0xd83d, 0xde00};
    const std::string expected("A\0\xf0\x9f\x98\x80", 6);
    CHECK(e::toUtf8(paired, LIMITS, e::MalformedUtf16::REJECT) == expected);
    const std::u16string broken{0xd800, u'A', 0xdc00};
    CHECK(e::toUtf8(broken, LIMITS, e::MalformedUtf16::REJECT).error() == e::EventTextError::INVALID_UTF16);
    CHECK(e::toUtf8(broken, LIMITS, e::MalformedUtf16::REPLACE) == std::string("\xef\xbf\xbd"
                                                                               "A"
                                                                               "\xef\xbf\xbd"));
    auto limits = LIMITS;
    limits.utf8Bytes = expected.size();
    CHECK(e::toUtf8(paired, limits, e::MalformedUtf16::REJECT) == expected);
    --limits.utf8Bytes;
    CHECK(e::toUtf8(paired, limits, e::MalformedUtf16::REJECT).error() == e::EventTextError::RESOURCE_LIMIT);
    const auto ctx = e::EventTextContext::utc();
    CHECK(e::toStringUtf8(quote(true), ctx, LIMITS, e::MalformedUtf16::REJECT).error() ==
          e::EventTextError::INVALID_UTF16);
    CHECK(e::toStringUtf8(quote(true), ctx, LIMITS, e::MalformedUtf16::REPLACE).has_value());
}

TEST_CASE("copied immutable contexts support independent concurrent const calls") {
    const auto v = profile(true);
    const auto first = e::EventTextContext::fixedOffset(19800000);
    const auto second = first;
    auto movedSource = first;
    const auto moved = std::move(movedSource);
    CHECK(e::toString(v, moved, LIMITS) == e::toString(v, movedSource, LIMITS));
    const auto expected = e::toString(v, first, LIMITS);
    std::array<std::expected<std::u16string, e::EventTextError>, 2> results;
    std::thread left([&] {
        results[0] = e::toString(v, first, LIMITS);
    });
    std::thread right([&] {
        results[1] = e::toString(v, second, LIMITS);
    });
    left.join();
    right.join();
    CHECK(results[0] == expected);
    CHECK(results[1] == expected);
}

TEST_CASE("Summary UTF8 and finite limits retain the full long OpenInterest value") {
    auto value = summary(false);
    value.symbol = u"OI";
    value.openInterest = std::numeric_limits<std::int64_t>::min();
    const auto ctx = e::EventTextContext::utc();
    const auto text = e::toStringUtf8(value, ctx, LIMITS, e::MalformedUtf16::REJECT);
    REQUIRE(text);
    CHECK(text->find("openInterest=-9223372036854775808") != std::string::npos);
    const auto wide = e::toString(value, ctx, LIMITS);
    REQUIRE(wide);
    auto exact = LIMITS;
    exact.inputUnits = value.symbol.size();
    exact.outputUnits = wide->size();
    exact.utf8Bytes = text->size();
    CHECK(e::toStringUtf8(value, ctx, exact, e::MalformedUtf16::REJECT) == text);
    --exact.utf8Bytes;
    CHECK(e::toStringUtf8(value, ctx, exact, e::MalformedUtf16::REJECT).error() == e::EventTextError::RESOURCE_LIMIT);
    value.symbol.push_back(0xd800);
    CHECK(e::toString(value, ctx, LIMITS).has_value());
    CHECK(e::toStringUtf8(value, ctx, LIMITS, e::MalformedUtf16::REJECT).error() == e::EventTextError::INVALID_UTF16);
}

TEST_CASE("Summary text matches actual public Java setters under every explicit fixed context") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_EVENT_TEXT_FIXTURE);
    REQUIRE(input.good());
    std::string line;
    std::size_t checked = 0;

    while (std::getline(input, line)) {
        std::array<std::string, 4> cells;
        std::istringstream fields(line);

        for (auto &cell : cells) {
            std::getline(fields, cell, '\t');
        }

        if (!cells[1].starts_with("summary") || !available(cells[0])) {
            continue;
        }

        auto value = summary(cells[1] == "summary" && cells[2] == "filled");

        if (cells[1] == "summaryInterest") {
            value.symbol = u"interest";
            value.openInterest = std::stoll(cells[2]);
        } else if (cells[1] == "summaryFlags") {
            value.symbol = u"flags";
            value.flags = static_cast<std::int32_t>(std::stoll(cells[2]));
        } else {
            REQUIRE(cells[1] == "summary");
        }

        const auto ctx = context(cells[0]);
        const auto actual = e::toString(value, ctx, LIMITS);
        INFO("profile=" << cells[0] << " kind=" << cells[1] << " input=" << cells[2]);
        REQUIRE(actual);
        CHECK(*actual == units(cells[3]));
        ++checked;
    }

    CHECK(input.eof());
    CHECK(checked == 168 + NAMED_PROFILES * 28);
}

TEST_CASE("timezone factory is explicit and fixed bindings need no initialized backend") {
    CHECK(e::EventTextContext::timeZone(nullptr, u"America/New_York", 0).error() ==
          e::EventTextError::TIME_ZONE_NOT_READY);
    CHECK(e::EventTextContext::timeZone(nullptr, u"unknown/invalid", 0).error() ==
          e::EventTextError::UNKNOWN_TIME_ZONE);
    CHECK(e::EventTextContext::timeZone(nullptr, u"UTC", 0, 2).error() == e::EventTextError::RESOURCE_LIMIT);
    const auto fixed = e::EventTextContext::timeZone(nullptr, u"GMT+05:30", 0);
    REQUIRE(fixed);
    CHECK(e::formatEventTime(-1, true, *fixed, LIMITS) ==
          e::formatEventTime(-1, true, e::EventTextContext::fixedOffset(19800000), LIMITS));
    const auto utc = e::EventTextContext::timeZone(nullptr, u"UTC", 0);
    REQUIRE(utc);
    CHECK(e::formatEventTime(std::numeric_limits<std::int64_t>::min(), true, *utc, LIMITS) ==
          std::u16string(u"-9223372036854775808"));
}

#ifdef ORG_TTLDTOR_QDCXX_EVENT_TEXT_ZONE_ROOT
TEST_CASE("named owning contexts survive input destruction and support concurrent const formatting") {
    auto source = namedContexts().at("Europe/Berlin");
    const auto moved = std::move(source);
    const auto expected = e::toString(quote(true), moved, LIMITS);
    REQUIRE(expected);
    CHECK(e::toString(quote(true), source, LIMITS) == expected);
    std::array<std::expected<std::u16string, e::EventTextError>, 2> results;
    std::thread left([&] {
        results[0] = e::toString(quote(true), moved, LIMITS);
    });
    std::thread right([&] {
        results[1] = e::toString(quote(true), source, LIMITS);
    });
    left.join();
    right.join();
    CHECK(results[0] == expected);
    CHECK(results[1] == expected);
}
#endif
