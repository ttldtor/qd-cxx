// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/nuam_trade.hpp"
#include "trade_fixture.hpp"

#include <array>
#include <cmath>
#include <doctest/doctest.h>
#include <limits>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace c = q::core;
namespace e = q::events;
using Type = c::SerialFieldType;
constexpr c::SchemaLimits SCHEMA{2, 64, 128, 128, 4096};
constexpr e::MappingLimits MAPPING{64, 128, 32, 64, 160, 128, 4096, 1000000};

/** @param schema Borrowed complete local schema.
 * @return Independent zero-initialized values with exact field types.
 */
std::vector<c::FieldValue> valuesFor(const c::LocalSchema &schema) {
    std::vector<c::FieldValue> values;

    for (const auto &field : schema.records().front().fields) {
        values.push_back({static_cast<Type>(field.serialTypeId), 0});
    }

    return values;
}
} // namespace

TEST_CASE("NuamTrade retains base Trade and independent statistics") {
    const auto schema = e::defaultNuamTradeSchema(SCHEMA);
    REQUIRE(schema);
    const auto mapper = e::PreparedNuamTrade::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto values = valuesFor(*schema);
    values[0].raw = -1;
    values[1].raw = 0x00400005;
    values[5].raw = 0x180;
    values[6].raw = 0x280;
    values[7].raw = 1;
    values[13].raw = std::numeric_limits<std::int64_t>::min();
    values[14].raw = 0x380;
    values[15].raw = 0x480;
    values[16].raw = std::numeric_limits<std::int32_t>::max();
    values[17].raw = 0x580;
    std::u16string symbol = u"IBM";
    auto result = mapper->map(symbol, {schema->recordContext(0), values}, symbol.size(), 1234);
    REQUIRE(result);
    symbol[0] = u'X';
    CHECK(result->symbol == u"IBM");
    CHECK(result->eventTime == 1234);
    CHECK(result->getTime() == -999);
    CHECK(result->getSequence() == 5);
    CHECK(result->getPrice() == 1);
    CHECK(result->getSizeAsDouble() == 2);
    CHECK(result->getTickDirection() == e::Direction::ZERO_UP);
    CHECK(result->getTradeStatTime() == std::numeric_limits<std::int64_t>::min());
    CHECK(result->getLastSignificantPrice() == 3);
    CHECK(result->getLastPriceForAll() == 4);
    CHECK(result->getNumberOfTrades() == std::numeric_limits<std::int32_t>::max());
    CHECK(result->getVWAP() == 5);
    result->lastPriceForAll.raw = 0x680;
    CHECK(result->getLastPriceForAll() == 6);
    CHECK(result->getLastSignificantPrice() == 3);
    CHECK(mapper->map(symbol, {schema->recordContext(0), values}, 2) ==
          std::unexpected(e::MappingError::RESOURCE_LIMIT));
    const auto other = e::defaultNuamTradeSchema(SCHEMA);
    REQUIRE(other);
    CHECK(mapper->map(symbol, {other->recordContext(0), values}, 3) == std::unexpected(e::MappingError::WRONG_CONTEXT));
}

TEST_CASE("Every NuamTrade business field is required independently") {
    constexpr std::array<std::u16string_view, 7> required{
        u"Last.Price",      u"Last.Size",      u"TradeStatTime", u"LastSignificantPrice",
        u"LastPriceForAll", u"NumberOfTrades", u"VWAP"};

    for (const auto omitted : required) {
        c::LocalRecord record{0, u"NuamTrade", false, {}, {}};

        for (const auto name : required) {
            if (name == omitted) {
                continue;
            }

            const auto type = name == u"TradeStatTime"    ? Type::TIME_MILLIS
                              : name == u"NumberOfTrades" ? Type::COMPACT_INT
                                                          : Type::WIDE_DECIMAL;
            record.lookups.push_back({std::u16string(name), record.fields.size()});
            record.fields.push_back({std::to_underlying(type)});
        }

        const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);
        CHECK(e::PreparedNuamTrade::prepare(*schema, 0, MAPPING) == std::unexpected(e::MappingError::MISSING_REQUIRED));
    }
}

TEST_CASE("NuamTrade owning and borrowed values match the pinned Java mapper and delegate") {
    const auto corpus = trade_fixture::load(ORG_TTLDTOR_QDCXX_NUAM_TRADE_FIXTURE);
    std::size_t compared = 0;
    std::size_t refused = 0;

    for (const auto &[name, scenario] : corpus) {
        CAPTURE(name);
        auto schema = c::LocalSchema::create(scenario.records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);

        for (std::size_t record = 0; record < scenario.records.size(); ++record) {
            const auto mapper = e::PreparedNuamTrade::prepare(*schema, static_cast<std::int32_t>(record), MAPPING);

            if (name.starts_with("required") || name.starts_with("missing-")) {
                CHECK_FALSE(mapper);
                ++refused;
                continue;
            }

            if (name.starts_with("id-one") || name.starts_with("bad") || name.starts_with("missing120")) {
                continue;
            }

            REQUIRE(mapper);

            for (const auto &[identity, sample] : scenario.samples) {
                if (identity.first != record) {
                    continue;
                }

                CAPTURE(identity.second);
                REQUIRE(sample.expected.size() == 33);
                const std::vector<std::string> tradeExpected(sample.expected.begin(), sample.expected.begin() + 25);
                std::vector<c::FieldValueView> borrowed;

                for (const auto &value : sample.values) {
                    borrowed.push_back({value.type, value.raw});
                }

                const auto owned = mapper->map(sample.symbol, {schema->recordContext(record), sample.values}, 128,
                                               std::stoll(sample.expected[1]));
                const auto view = mapper->mapView(sample.symbol, {schema->recordContext(record), borrowed}, 128,
                                                  std::stoll(sample.expected[1]));
                REQUIRE(owned);
                REQUIRE(view);

                for (const auto *actual : {&*owned, &*view}) {
                    CHECK(trade_fixture::matches(*actual, tradeExpected));
                    CHECK(actual->getTradeStatTime() == std::stoll(sample.expected[25]));
                    CHECK(actual->getNumberOfTrades() == std::stoi(sample.expected[26]));
                    const std::array raw{actual->lastSignificantPrice.raw, actual->lastPriceForAll.raw,
                                         actual->vwap.raw};
                    const std::array business{actual->getLastSignificantPrice(), actual->getLastPriceForAll(),
                                              actual->getVWAP()};

                    for (std::size_t i = 0; i < raw.size(); ++i) {
                        CHECK(raw[i] == std::stoll(sample.expected[27 + i]));
                        const auto bits = static_cast<std::uint64_t>(std::stoll(sample.expected[30 + i]));

                        if (std::isnan(std::bit_cast<double>(bits))) {
                            CHECK(std::isnan(business[i]));
                        } else {
                            CHECK(std::bit_cast<std::uint64_t>(business[i]) == bits);
                        }
                    }
                }

                ++compared;
            }
        }
    }

    CHECK(compared > 100);
    CHECK(refused >= 10);
}
