// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "evidence_cells.hpp"
#include "qd/events/candle.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace c = q::core;
namespace e = q::events;
using Type = c::SerialFieldType;
constexpr c::SchemaLimits SCHEMA{1, 32, 32, 64, 1024};
constexpr e::MappingLimits MAPPING{32, 32, 16, 16, 128, 64, 1024, 10000};
constexpr std::size_t ORACLE_RECORDS = 8;
constexpr std::size_t ORACLE_EVENT_CELLS = 25;
constexpr std::size_t SYMBOL_UNITS = 32;

struct OracleSample {
    std::vector<std::int64_t> values;
    std::vector<std::string> event;
};

struct OracleScenario {
    c::LocalRecord record{0, u"Candle", true, {}, {}, c::IndexedTimePairInt32{0, 1}};
    std::array<OracleSample, ORACLE_RECORDS> samples;
};

/** @param schema Immutable complete test schema.
 * @return Complete canonical values matching the exact field shape.
 */
std::vector<c::FieldValue> defaults(const c::LocalSchema &schema) {
    std::vector<c::FieldValue> values;

    for (const auto &field : schema.records()[0].fields)
        values.push_back({static_cast<Type>(field.serialTypeId), 0});

    return values;
}

/** @param record Complete local record.
 * @param name Exact case-sensitive local lookup key.
 * @return Selected physical local field ID.
 */
std::size_t find(const c::LocalRecord &record, std::u16string_view name) {
    for (const auto &lookup : record.lookups)
        if (lookup.name == name)
            return lookup.fieldId;

    throw std::runtime_error("missing test field");
}

/** @param field Raw Decimal significand, before its rank byte.
 * @param exponent Decimal exponent in -127..127.
 * @return Exact wide decimal representation.
 */
std::int64_t wide(std::int64_t field, int exponent) {
    return std::bit_cast<std::int64_t>((static_cast<std::uint64_t>(field) << 8) |
                                       static_cast<std::uint64_t>(128 - exponent));
}

/** Read the captured production schema, complete raw fields and delegate observations.
 * @param path Captured profile's oracle.tsv.
 * @return Independently owned schema and eight complete event rows.
 */
OracleScenario readOracle(const std::filesystem::path &path) {
    std::ifstream input(path);
    REQUIRE(input);
    OracleScenario result;
    std::vector<std::pair<std::string, std::int32_t>> physical;
    std::size_t recordCount = 0;
    std::string row;

    while (std::getline(input, row)) {
        const auto cells = q::test_support::split(row);
        REQUIRE_FALSE(cells.empty());

        if (cells[0] == "record") {
            REQUIRE(cells.size() == 4);
            CHECK(cells[3] == "true");
            result.record.name.assign(cells[2].begin(), cells[2].end());
            ++recordCount;
        } else if (cells[0] == "physical") {
            REQUIRE(cells.size() == 4);
            CHECK(std::stoul(cells[1]) == physical.size());
            physical.emplace_back(cells[3], std::stoi(cells[2]));
        } else if (cells[0] == "field") {
            REQUIRE(cells.size() == 4);
            const auto id = result.record.fields.size();
            result.record.fields.push_back({std::stoi(cells[1])});
            result.record.lookups.push_back({std::u16string(cells[2].begin(), cells[2].end()), id});

            if (cells[3] != cells[2])
                result.record.lookups.push_back({std::u16string(cells[3].begin(), cells[3].end()), id});
        } else if (cells[0] == "value") {
            REQUIRE(cells.size() == 3);
            result.samples.at(std::stoul(cells[1])).values.push_back(std::stoll(cells[2]));
        } else if (cells[0] == "event") {
            REQUIRE(cells.size() == ORACLE_EVENT_CELLS);
            auto &event = result.samples.at(std::stoul(cells[1])).event;
            REQUIRE(event.empty());
            event = cells;
        } else {
            FAIL("Unexpected Candle oracle row");
        }
    }

    REQUIRE(recordCount == 1);
    REQUIRE(physical.size() >= 2);
    CHECK(physical[0].first == "Time");
    CHECK(physical[1].first == "Sequence");
    std::size_t logical = 0;

    for (const auto &[name, type] : physical) {
        if (type == 0)
            continue;

        REQUIRE(logical < result.record.fields.size());
        CHECK(result.record.fields[logical].serialTypeId == type);
        CHECK(find(result.record, std::u16string(name.begin(), name.end())) == logical);
        ++logical;
    }

    CHECK(logical == result.record.fields.size());

    for (const auto &sample : result.samples) {
        REQUIRE(sample.event.size() == ORACLE_EVENT_CELLS);
        REQUIRE(sample.values.size() == result.record.fields.size());
    }

    return result;
}

/** @param value Business double from native mapping.
 * @return Java Double.doubleToLongBits representation.
 */
std::int64_t doubleBits(double value) {
    return std::isnan(value) ? 0x7ff8000000000000LL : std::bit_cast<std::int64_t>(value);
}

/** Compare one complete owning value with the production Java delegate observation.
 * @param actual Native projected event.
 * @param expected Complete captured event row.
 */
void verifyOracleEvent(const e::Candle &actual, const std::vector<std::string> &expected) {
    REQUIRE(expected.size() == ORACLE_EVENT_CELLS);
    REQUIRE(actual.eventSymbol);
    REQUIRE(actual.eventSymbol->getSymbol());
    CHECK(*actual.eventSymbol->getSymbol() == std::u16string(expected[2].begin(), expected[2].end()));
    CHECK(actual.eventTime == std::stoll(expected[3]));
    CHECK(actual.eventFlags == std::stoi(expected[4]));
    CHECK(actual.getSource().id() == std::stoi(expected[5]));
    CHECK(actual.getIndex() == std::stoll(expected[6]));
    CHECK(actual.getTime() == std::stoll(expected[7]));
    CHECK(actual.getSequence() == std::stoi(expected[8]));
    CHECK(actual.getCount() == std::stoll(expected[9]));
    const std::array values{actual.getOpen(),
                            actual.getHigh(),
                            actual.getLow(),
                            actual.getClose(),
                            actual.getVolumeAsDouble(),
                            actual.getVWAP(),
                            actual.getBidVolumeAsDouble(),
                            actual.getAskVolumeAsDouble(),
                            actual.getImpVolatility(),
                            actual.getOpenInterestAsDouble()};

    for (std::size_t index = 0; index < values.size(); ++index)
        CHECK(doubleBits(values[index]) == std::stoll(expected[10 + index]));

    CHECK(actual.getVolume() == std::stoll(expected[20]));
    CHECK(actual.getBidVolume() == std::stoll(expected[21]));
    CHECK(actual.getAskVolume() == std::stoll(expected[22]));
    CHECK(actual.getOpenInterest() == std::stoll(expected[23]));
    CHECK(actual.getIndex() == std::stoll(expected[24]));
}
} // namespace

TEST_CASE("Candle owning and borrowed projections match the pinned Java production delegates") {
    const auto root = std::filesystem::path(ORG_TTLDTOR_QDCXX_CANDLE_FIXTURE);

    for (const std::string_view profile : {"standard", "tiny", "optional", "long", "numeric-long", "trade", "period"}) {
        CAPTURE(profile);
        const auto fixture = readOracle(root / profile / "oracle.tsv");
        const auto schema =
            c::LocalSchema::create(std::array{fixture.record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);
        const auto kind = profile == "trade" ? e::CandleRecordKind::TRADE_HISTORY : e::CandleRecordKind::CANDLE;
        const auto mapper = e::PreparedCandle::prepare(*schema, 0, kind, MAPPING);
        REQUIRE(mapper);

        if (profile == "standard") {
            const auto standard = e::defaultCandleSchema(SCHEMA);
            REQUIRE(standard);
            REQUIRE(standard->records()[0].fields.size() == fixture.record.fields.size());

            for (std::size_t index = 0; index < fixture.record.fields.size(); ++index)
                CHECK(standard->records()[0].fields[index].serialTypeId == fixture.record.fields[index].serialTypeId);
        }

        for (const auto &sample : fixture.samples) {
            CAPTURE(sample.event[1]);
            auto values = defaults(*schema);

            for (std::size_t index = 0; index < values.size(); ++index)
                values[index].raw = sample.values[index];

            const std::u16string symbolText(sample.event[2].begin(), sample.event[2].end());
            const auto symbol = e::CandleSymbol::valueOf(std::u16string_view(symbolText));
            const auto time = std::stoll(sample.event[3]);
            const auto flags = std::stoi(sample.event[4]);
            const auto mapped = mapper->map(symbol, {schema->recordContext(0), values}, SYMBOL_UNITS, time, flags);
            REQUIRE(mapped);
            verifyOracleEvent(*mapped, sample.event);
            std::vector<c::FieldValueView> views;
            views.reserve(values.size());

            for (const auto &value : values)
                views.push_back({value.type, value.raw, {}, {}});

            const auto borrowed = mapper->mapView(symbol, {schema->recordContext(0), views}, SYMBOL_UNITS, time, flags);
            REQUIRE(borrowed);
            verifyOracleEvent(*borrowed, sample.event);

            for (auto &value : values)
                value.raw = 0;

            for (auto &view : views)
                view.raw = 0;

            verifyOracleEvent(*mapped, sample.event);
            verifyOracleEvent(*borrowed, sample.event);
        }
    }
}

TEST_CASE("Candle required-field refusals match the pinned Java delegates") {
    const auto root = std::filesystem::path(ORG_TTLDTOR_QDCXX_CANDLE_FIXTURE);

    for (const std::string_view profile : {"optional", "trade"}) {
        CAPTURE(profile);
        const auto fixture = readOracle(root / profile / "oracle.tsv");
        const auto kind = profile == "trade" ? e::CandleRecordKind::TRADE_HISTORY : e::CandleRecordKind::CANDLE;
        std::ifstream input(root / profile / "required.tsv");
        REQUIRE(input);
        std::vector<std::u16string> required;
        std::string row;

        while (std::getline(input, row)) {
            const auto cells = q::test_support::split(row);
            REQUIRE(cells.size() == 3);
            CHECK(cells[0] == "reject");
            CHECK(cells[2] == "IllegalArgumentException");
            required.emplace_back(cells[1].begin(), cells[1].end());
        }

        REQUIRE(required.size() == (profile == "trade" ? 4 : 6));

        for (const auto &omitted : required) {
            c::LocalRecord record{0, fixture.record.name, false, {}, {}};

            for (const auto &name : required) {
                if (name == omitted)
                    continue;

                record.lookups.push_back({name, record.fields.size()});
                record.fields.push_back(fixture.record.fields[find(fixture.record, name)]);
            }

            const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
            REQUIRE(schema);
            CHECK(e::PreparedCandle::prepare(*schema, 0, kind, MAPPING) ==
                  std::unexpected(e::MappingError::MISSING_REQUIRED));
        }
    }
}

TEST_CASE("standard Candle maps owning metadata and absent values") {
    auto schema = e::defaultCandleSchema(SCHEMA);
    REQUIRE(schema);
    auto mapping = e::PreparedCandle::prepare(*schema, 0, e::CandleRecordKind::CANDLE, MAPPING);
    REQUIRE(mapping);
    auto values = defaults(*schema);
    values[0].raw = -1;
    values[1].raw = 0x00400001;
    const auto symbol = e::CandleSymbol::valueOf(std::u16string_view(u"IBM{=1m}"));
    auto candle = mapping->map(symbol, {schema->recordContext(0), values}, 32, 7311, 5);
    REQUIRE(candle);
    REQUIRE(candle->eventSymbol);
    CHECK(candle->eventSymbol->getSymbol() == symbol.getSymbol());
    CHECK(candle->eventTime == 7311);
    CHECK(candle->eventFlags == 5);
    CHECK(candle->getSource().id() == 0);
    CHECK(candle->getTime() == -999);
    CHECK(candle->getSequence() == 1);
    CHECK(candle->getCount() == 0);
    CHECK(std::isnan(candle->getOpen()));
    CHECK(std::isnan(candle->getVolumeAsDouble()));
    CHECK(candle->getVolume() == 0);
    CHECK_FALSE(mapping->map(symbol, {schema->recordContext(0), values}, 1));
    values.pop_back();
    CHECK_FALSE(mapping->map(symbol, {schema->recordContext(0), values}, 32));
}

TEST_CASE("Candle Count preserves decimal rounding and full integer precision") {
    auto original = e::defaultCandleSchema(SCHEMA);
    REQUIRE(original);
    auto record = original->records()[0];
    const auto count = find(record, u"Count");
    record.fields[count].serialTypeId = std::to_underlying(Type::WIDE_DECIMAL);
    const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapping = e::PreparedCandle::prepare(*schema, 0, e::CandleRecordKind::CANDLE, MAPPING);
    REQUIRE(mapping);
    auto values = defaults(*schema);
    const auto symbol = e::CandleSymbol::valueOf(std::u16string_view(u"IBM"));

    for (const auto &[raw, expected] :
         std::array{std::pair{wide(15, -1), std::int64_t{2}}, std::pair{wide(-15, -1), std::int64_t{-1}},
                    std::pair{wide(9007199254740993LL, 0), std::int64_t{9007199254740993LL}},
                    std::pair{wide(1, 19), std::numeric_limits<std::int64_t>::max()},
                    std::pair{wide(-1, 19), std::numeric_limits<std::int64_t>::min()},
                    std::pair{std::int64_t{0}, std::int64_t{0}}}) {
        values[count].raw = raw;
        const auto result = mapping->map(symbol, {schema->recordContext(0), values}, 32);
        REQUIRE(result);
        CHECK(result->getCount() == expected);
    }
}

TEST_CASE("Candle maps a standard schema without optional fields and refuses missing prices") {
    auto full = e::defaultCandleSchema(SCHEMA);
    REQUIRE(full);
    c::LocalRecord record{0, u"Candle", true, {}, {}, c::IndexedTimePairInt32{0, 1}};
    constexpr std::array required{u"Time", u"Sequence", u"Open", u"High", u"Low", u"Close"};

    for (const auto name : required) {
        const auto index = find(full->records()[0], name);
        record.lookups.push_back({name, record.fields.size()});
        record.fields.push_back(full->records()[0].fields[index]);
    }

    const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapping = e::PreparedCandle::prepare(*schema, 0, e::CandleRecordKind::CANDLE, MAPPING);
    REQUIRE(mapping);
    auto values = defaults(*schema);
    const auto symbol = e::CandleSymbol::valueOf(std::u16string_view(u"IBM"));
    const auto candle = mapping->map(symbol, {schema->recordContext(0), values}, 32);
    REQUIRE(candle);
    CHECK(candle->getCount() == 0);
    CHECK(std::isnan(candle->getVWAP()));
    CHECK(std::isnan(candle->getBidVolumeAsDouble()));
    CHECK(std::isnan(candle->getAskVolumeAsDouble()));
    CHECK(std::isnan(candle->getImpVolatility()));
    CHECK(std::isnan(candle->getOpenInterestAsDouble()));

    record.lookups.pop_back();
    record.fields.pop_back();
    const auto incomplete = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(incomplete);
    const auto refused = e::PreparedCandle::prepare(*incomplete, 0, e::CandleRecordKind::CANDLE, MAPPING);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == e::MappingError::MISSING_REQUIRED);
}

TEST_CASE("TradeHistory uses one price for OHLC and exposes missing optional values") {
    c::LocalRecord record{0, u"TradeHistory", true, {}, {}, c::IndexedTimePairInt32{0, 1}};
    constexpr std::array names{u"Time", u"Sequence", u"Price", u"Size"};
    constexpr std::array types{Type::TIME_SECONDS, Type::SEQUENCE, Type::LONG, Type::COMPACT_INT};

    for (std::size_t index = 0; index < names.size(); ++index) {
        record.lookups.push_back({names[index], index});
        record.fields.push_back({std::to_underlying(types[index])});
    }

    auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapping = e::PreparedCandle::prepare(*schema, 0, e::CandleRecordKind::TRADE_HISTORY, MAPPING);
    REQUIRE(mapping);
    auto values = defaults(*schema);
    values[2].raw = 9007199254740993LL;
    values[3].raw = 37;
    const auto symbol = e::CandleSymbol::valueOf(std::u16string_view(u"IBM{=t}"));
    const auto candle = mapping->map(symbol, {schema->recordContext(0), values}, 32, 0, 2);
    REQUIRE(candle);
    CHECK(candle->getCount() == 1);
    CHECK(candle->getOpen() == 9007199254740992.0);
    CHECK(candle->getHigh() == candle->getClose());
    CHECK(candle->getLow() == candle->getClose());
    CHECK(candle->getVolume() == 37);
    CHECK(std::isnan(candle->getVWAP()));
    CHECK(std::isnan(candle->getBidVolumeAsDouble()));
    CHECK(candle->eventFlags == 2);
    record.lookups.pop_back();
    record.fields.pop_back();
    auto missing = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(missing);
    const auto refused = e::PreparedCandle::prepare(*missing, 0, e::CandleRecordKind::TRADE_HISTORY, MAPPING);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == e::MappingError::MISSING_REQUIRED);
}
