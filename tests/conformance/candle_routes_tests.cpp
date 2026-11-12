// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "evidence_cells.hpp"
#include "qd/events/candle_routes.hpp"

#include <algorithm>
#include <array>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace c = q::core;
namespace e = q::events;
using Type = c::SerialFieldType;
constexpr c::SchemaLimits SCHEMA{16, 256, 256, 128, 8192};
constexpr e::MappingLimits MAPPING{32, 32, 16, 16, 128, 128, 2048, 100000};
constexpr e::CandleRouteLimits LIMITS{16, 128, 128, MAPPING};

struct OracleCatalog {
    std::string name;
    std::vector<c::LocalRecord> records;
    std::vector<std::vector<std::string>> routes;
};

/** @param text Captured ASCII QD text.
 * @return Independent UTF-16 units, without locale-dependent conversions.
 */
std::u16string units(std::string_view text) {
    return {text.begin(), text.end()};
}

/** @param catalogs Existing captured catalogs.
 * @param name Captured catalog identifier.
 * @return Mutable catalog or a new empty catalog for the first record.
 */
OracleCatalog &catalog(std::vector<OracleCatalog> &catalogs, std::string_view name) {
    for (auto &item : catalogs)
        if (item.name == name)
            return item;

    catalogs.push_back({std::string(name), {}, {}});

    return catalogs.back();
}

/** @param path Captured Java delegate-set oracle file.
 * @param rejections Receives the exact production refusal rows.
 * @return Ordered, independently owned schema catalogs and routing observations.
 */
std::vector<OracleCatalog> readOracle(const std::filesystem::path &path,
                                      std::vector<std::vector<std::string>> &rejections) {
    std::ifstream input(path);
    REQUIRE(input);
    std::vector<OracleCatalog> catalogs;
    std::string row;

    while (std::getline(input, row)) {
        const auto cells = q::test_support::split(row);
        REQUIRE_FALSE(cells.empty());

        if (cells[0] == "record") {
            REQUIRE(cells.size() == 5);
            auto &target = catalog(catalogs, cells[1]);
            CHECK(std::stoul(cells[2]) == target.records.size());
            CHECK(cells[4] == "true");
            target.records.push_back({static_cast<std::int32_t>(target.records.size()),
                                      units(cells[3]),
                                      true,
                                      {},
                                      {},
                                      c::IndexedTimePairInt32{0, 1}});
        } else if (cells[0] == "field") {
            REQUIRE(cells.size() == 6);
            auto &record = catalog(catalogs, cells[1]).records.at(std::stoul(cells[2]));
            const auto fieldId = record.fields.size();
            record.fields.push_back({std::stoi(cells[3])});
            record.lookups.push_back({units(cells[4]), fieldId});

            if (cells[5] != cells[4])
                record.lookups.push_back({units(cells[5]), fieldId});
        } else if (cells[0] == "route") {
            REQUIRE(cells.size() == 9);
            catalog(catalogs, cells[1]).routes.push_back(cells);
        } else if (cells[0] == "reject") {
            REQUIRE(cells.size() == 4);
            rejections.push_back(cells);
        } else {
            FAIL("Unexpected Candle route oracle row");
        }
    }

    REQUIRE(catalogs.size() == 5);
    REQUIRE(rejections.size() == 8);

    return catalogs;
}

/** @return Minimal real TradeHistory field names and indexed Time/Sequence words. */
c::LocalRecord tradeHistory() {
    c::LocalRecord record{0, u"TradeHistory", true, {}, {}, c::IndexedTimePairInt32{0, 1}};
    constexpr std::array names{u"Time", u"Sequence", u"Price", u"Size"};
    constexpr std::array types{Type::TIME_SECONDS, Type::SEQUENCE, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL};

    for (std::size_t index = 0; index < names.size(); ++index) {
        record.lookups.push_back({names[index], index});
        record.fields.push_back({std::to_underlying(types[index])});
    }

    return record;
}

/** @param names Exact candidate record names in selection order.
 * @return Independently validated schema with standard Candle or TradeHistory field shapes.
 */
c::LocalSchema makeSchema(std::span<const std::u16string_view> names) {
    const auto standard = e::defaultCandleSchema(SCHEMA);
    REQUIRE(standard);
    std::vector<c::LocalRecord> records;

    for (const auto name : names) {
        auto record = name == u"TradeHistory" ? tradeHistory() : standard->records()[0];
        record.id = static_cast<std::int32_t>(records.size());
        record.name = name;
        records.push_back(std::move(record));
    }

    auto schema = c::LocalSchema::create(records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);

    return std::move(*schema);
}

/** @param text Java-style Candle symbol.
 * @return Independent immutable Candle symbol.
 */
e::CandleSymbol candle(std::u16string_view text) {
    return e::CandleSymbol::valueOf(text);
}

/** Compare one selected record and independent physical symbol.
 * @param routes Prepared complete candidate catalog.
 * @param source Logical symbol supplied by an application.
 * @param id Expected physical schema ID.
 * @param text Expected complete physical QD symbol.
 */
void checkRoute(const e::PreparedCandleRoutes &routes, std::u16string_view source, std::int32_t id,
                std::u16string_view text) {
    const auto symbol = candle(source);
    const auto route = routes.resolve(symbol);
    REQUIRE(route);
    CHECK(route->recordId == id);
    CHECK(route->symbol == text);
    CHECK(route->context.valid());
}
} // namespace

TEST_CASE("Candle routes and reverse symbols match both pinned Java delegate contracts") {
    std::vector<std::vector<std::string>> rejections;
    const auto catalogs = readOracle(ORG_TTLDTOR_QDCXX_CANDLE_ROUTES_FIXTURE, rejections);
    std::size_t checked = 0;

    for (const auto &item : catalogs) {
        CAPTURE(item.name);
        const auto schema = c::LocalSchema::create(item.records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);
        std::vector<std::int32_t> ids;

        for (const auto &record : item.records)
            ids.push_back(record.id);

        const auto resolver = e::PreparedCandleRoutes::prepare(*schema, ids, LIMITS);
        REQUIRE(resolver);
        std::size_t ticker = 0;
        std::size_t history = 0;

        for (const auto &row : item.routes) {
            INFO("contract=", row[2], " logicalSymbol=", row[3]);
            REQUIRE((row[2] == "Ticker" || row[2] == "History"));
            ticker += row[2] == "Ticker" ? 1 : 0;
            history += row[2] == "History" ? 1 : 0;
            const auto logicalText = units(row[3]);
            const auto logical = candle(logicalText);
            const auto result = resolver->resolve(logical);
            const auto count = std::stoi(row[4]);
            REQUIRE((count == 0 || count == 1));

            if (count == 0) {
                REQUIRE_FALSE(result);
                CHECK(result.error().error == e::CandleRouteError::MISSING_ROUTE);
                CHECK(row[5] == "-");
                CHECK(row[6] == "-");
                CHECK(row[7] == "-");
                CHECK(row[8] == "-");
                ++checked;
                continue;
            }

            REQUIRE(result);
            const auto expectedName = units(row[5]);
            const auto found = std::ranges::find_if(item.records, [&](const c::LocalRecord &record) {
                return record.name == expectedName;
            });
            REQUIRE(found != item.records.end());
            CHECK(result->recordId == found->id);
            CHECK(result->context.matches(schema->recordContext(found->id)));
            CHECK(result->symbol == units(row[6]));
            CHECK(result->kind ==
                  (row[7] == "TRADE_HISTORY" ? e::CandleRecordKind::TRADE_HISTORY : e::CandleRecordKind::CANDLE));
            REQUIRE((row[7] == "TRADE_HISTORY" || row[7] == "CANDLE"));
            const auto restored = resolver->restore(result->recordId, result->symbol);
            REQUIRE(restored);
            REQUIRE(restored->getSymbol());
            CHECK(*restored->getSymbol() == units(row[8]));
            ++checked;
        }

        CHECK(ticker == 10);
        CHECK(history == 10);
    }

    CHECK(checked == 100);
}

TEST_CASE("Candle catalog failures match pinned Java delegate-set outcomes") {
    std::vector<std::vector<std::string>> rejections;
    static_cast<void>(readOracle(ORG_TTLDTOR_QDCXX_CANDLE_ROUTES_FIXTURE, rejections));

    for (const auto &row : rejections) {
        INFO("descriptor=", row[1], " contract=", row[2]);
        REQUIRE((row[2] == "Ticker" || row[2] == "History"));
        std::vector<std::u16string_view> names;
        e::CandleRouteError expected;

        if (row[1] == "duplicate-period") {
            names = {u"Trade.1min", u"Candle{=m}"};
            expected = e::CandleRouteError::DUPLICATE_DESCRIPTOR;
            CHECK(row[3] == "IllegalArgumentException");
        } else if (row[1] == "invalid-price-period") {
            names = {u"Candle{=2m,price=bid}"};
            expected = e::CandleRouteError::INVALID_INPUT;
            CHECK(row[3] == "IllegalArgumentException");
        } else if (row[1] == "invalid-price") {
            names = {u"Candle{=m,price=wrong}"};
            expected = e::CandleRouteError::INVALID_INPUT;
            CHECK(row[3] == "NO_DELEGATE");
        } else {
            REQUIRE(row[1] == "unsupported-attributes");
            names = {u"Candle{=m,tho=true}"};
            expected = e::CandleRouteError::INVALID_INPUT;
            CHECK(row[3] == "NO_DELEGATE");
        }

        const auto schema = makeSchema(names);
        std::vector<std::int32_t> ids;

        for (const auto &record : schema.records())
            ids.push_back(record.id);

        const auto result = e::PreparedCandleRoutes::prepare(schema, ids, LIMITS);
        REQUIRE_FALSE(result);
        CHECK(result.error().error == expected);
    }
}

TEST_CASE("Candle descriptor precedence and physical symbol projection") {
    constexpr std::array<std::u16string_view, 6> names{
        u"Candle", u"TradeHistory", u"Trade.1min", u"Candle{=m,price=bid}", u"Trade.5min", u"Candle{=m,price=ask}"};
    const auto schema = makeSchema(names);
    constexpr std::array<std::int32_t, 6> ids{0, 1, 2, 3, 4, 5};
    const auto routes = e::PreparedCandleRoutes::prepare(schema, ids, LIMITS);
    REQUIRE(routes);

    checkRoute(*routes, u"IBM", 1, u"IBM");
    checkRoute(*routes, u"IBM{price=bid}", 1, u"IBM{price=bid}");
    checkRoute(*routes, u"IBM&Q{=1m}", 2, u"IBM&Q");
    checkRoute(*routes, u"IBM&Q{=1m,price=bid}", 3, u"IBM&Q");
    checkRoute(*routes, u"IBM{=1m,price=ask}", 5, u"IBM");
    checkRoute(*routes, u"IBM{=1m,price=mark}", 2, u"IBM{price=mark}");
    checkRoute(*routes, u"IBM{=5m,price=bid}", 4, u"IBM{price=bid}");
    checkRoute(*routes, u"IBM{=7m}", 0, u"IBM{=7m}");

    const auto selected = routes->resolve(candle(u"IBM&Q{=1m,price=bid}"));
    REQUIRE(selected);
    CHECK(selected->context.matches(schema.recordContext(3)));
    CHECK(selected->kind == e::CandleRecordKind::CANDLE);
    const auto tick = routes->resolve(candle(u"IBM"));
    REQUIRE(tick);
    CHECK(tick->kind == e::CandleRecordKind::TRADE_HISTORY);
}

TEST_CASE("Candle generic fallback and missing descriptor remain distinct") {
    constexpr std::array<std::u16string_view, 2> names{u"Candle", u"TradeHistory"};
    const auto schema = makeSchema(names);
    constexpr std::array<std::int32_t, 2> ids{0, 1};
    const auto routes = e::PreparedCandleRoutes::prepare(schema, ids, LIMITS);
    REQUIRE(routes);
    checkRoute(*routes, u"IBM{=1m,price=ask}", 0, u"IBM{=m,price=ask}");
    checkRoute(*routes, u"IBM{=9m}", 0, u"IBM{=9m}");

    const std::array<std::int32_t, 1> tickOnly{1};
    const auto restricted = e::PreparedCandleRoutes::prepare(schema, tickOnly, LIMITS);
    REQUIRE(restricted);
    const auto missing = restricted->resolve(candle(u"IBM{=1m}"));
    REQUIRE_FALSE(missing);
    CHECK(missing.error().error == e::CandleRouteError::MISSING_ROUTE);
}

TEST_CASE("Candle rejects duplicate descriptors and invalid candidate records") {
    constexpr std::array<std::u16string_view, 3> names{u"Trade.1min", u"Candle{=m}", u"Candle{=2m,price=bid}"};
    const auto schema = makeSchema(names);
    constexpr std::array<std::int32_t, 2> duplicates{0, 1};
    const auto ambiguous = e::PreparedCandleRoutes::prepare(schema, duplicates, LIMITS);
    REQUIRE_FALSE(ambiguous);
    CHECK(ambiguous.error().error == e::CandleRouteError::DUPLICATE_DESCRIPTOR);
    CHECK(ambiguous.error().recordId == 1);

    constexpr std::array<std::int32_t, 1> invalidPrice{2};
    const auto invalid = e::PreparedCandleRoutes::prepare(schema, invalidPrice, LIMITS);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().error == e::CandleRouteError::INVALID_INPUT);

    constexpr std::array<std::int32_t, 2> repeated{0, 0};
    const auto duplicateId = e::PreparedCandleRoutes::prepare(schema, repeated, LIMITS);
    REQUIRE_FALSE(duplicateId);
    CHECK(duplicateId.error().error == e::CandleRouteError::INVALID_INPUT);
}

TEST_CASE("Candle routing enforces bounds and refuses invalid selector or missing mapping") {
    constexpr std::array<std::u16string_view, 1> names{u"Candle"};
    const auto schema = makeSchema(names);
    constexpr std::array<std::int32_t, 1> ids{0};
    auto tiny = LIMITS;
    tiny.records = 0;
    const auto noSlots = e::PreparedCandleRoutes::prepare(schema, ids, tiny);
    REQUIRE_FALSE(noSlots);
    CHECK(noSlots.error().error == e::CandleRouteError::RESOURCE_LIMIT);

    tiny = LIMITS;
    tiny.recordNameUnits = 5;
    const auto noName = e::PreparedCandleRoutes::prepare(schema, ids, tiny);
    REQUIRE_FALSE(noName);
    CHECK(noName.error().error == e::CandleRouteError::RESOURCE_LIMIT);

    tiny = LIMITS;
    tiny.symbolUnits = 2;
    const auto routes = e::PreparedCandleRoutes::prepare(schema, ids, tiny);
    REQUIRE(routes);
    const auto longSymbol = routes->resolve(candle(u"IBM"));
    REQUIRE_FALSE(longSymbol);
    CHECK(longSymbol.error().error == e::CandleRouteError::RESOURCE_LIMIT);
    const auto wildcard = routes->resolve(candle(u"*"));
    REQUIRE_FALSE(wildcard);
    CHECK(wildcard.error().error == e::CandleRouteError::INVALID_INPUT);
    const auto unknownRecord = routes->restore(9, u"AB");
    REQUIRE_FALSE(unknownRecord);
    CHECK(unknownRecord.error().error == e::CandleRouteError::INVALID_INPUT);
    const auto emptyPhysical = routes->restore(0, u"");
    REQUIRE_FALSE(emptyPhysical);
    CHECK(emptyPhysical.error().error == e::CandleRouteError::INVALID_INPUT);

    auto broken = schema.records()[0];
    broken.fields.pop_back();
    broken.lookups.pop_back();
    const auto incomplete = c::LocalSchema::create(std::array{broken}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(incomplete);
    auto wrongLayout = incomplete->records()[0];
    wrongLayout.indexedTimeLayout = c::IndexedTimePairInt32{1, 0};
    const auto inconsistent =
        c::LocalSchema::create(std::array{wrongLayout}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(inconsistent);
    const auto refused = e::PreparedCandleRoutes::prepare(*inconsistent, ids, LIMITS);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().error == e::CandleRouteError::INVALID_MAPPING);

    auto ordinary = schema.records()[0];
    ordinary.hasTime = false;
    ordinary.indexedTimeLayout.reset();
    const auto ticker = c::LocalSchema::create(std::array{ordinary}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(ticker);
    CHECK(e::PreparedCandleRoutes::prepare(*ticker, ids, LIMITS));
}

TEST_CASE("Candle reverse projection rejects an expanded symbol beyond its bound") {
    constexpr std::array<std::u16string_view, 1> names{u"Candle{=m,price=bid}"};
    const auto schema = makeSchema(names);
    constexpr std::array<std::int32_t, 1> ids{0};
    auto limits = LIMITS;
    limits.symbolUnits = 3;
    const auto routes = e::PreparedCandleRoutes::prepare(schema, ids, limits);
    REQUIRE(routes);
    const auto tooLong = routes->restore(0, u"IBM");
    REQUIRE_FALSE(tooLong);
    CHECK(tooLong.error().error == e::CandleRouteError::RESOURCE_LIMIT);
}
