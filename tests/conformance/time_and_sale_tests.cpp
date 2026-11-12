// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/event_flags.hpp"
#include "qd/events/nuam_time_and_sale.hpp"
#include "qd/events/time_and_sale.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <doctest/doctest.h>
#include <limits>
#include <string>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace c = q::core;
namespace e = q::events;
using Type = c::SerialFieldType;
constexpr c::SchemaLimits SCHEMA{1, 32, 64, 64, 1024};
constexpr e::MappingLimits MAPPING{32, 64, 16, 40, 160, 64, 2048, 20000};
constexpr e::EventValueLimits VALUE_LIMITS{64, 64, 256};

std::vector<c::FieldValue> emptyValues(const c::LocalSchema &schema) {
    std::vector<c::FieldValue> result;

    for (const auto &field : schema.records()[0].fields)
        result.push_back({static_cast<Type>(field.serialTypeId), 0});

    return result;
}
} // namespace

TEST_CASE("TimeAndSale maps standard Java HISTORY values and separates event from trade flags") {
    const auto schema = e::defaultTimeAndSaleSchema(SCHEMA);
    REQUIRE(schema);
    const auto mapper = e::PreparedTimeAndSale::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = emptyValues(*schema);
    // Values come from low-partial record 1 in tests/fixtures/history-receiver/oracle.tsv.
    fields[0].raw = 3;
    fields[4].raw = 774530;
    fields[5].raw = 2572930;
    fields[8].raw = 0x4142;
    fields[9].raw = (u'Q' << 8) | (2 << 5) | (1 << 4) | (1 << 3) | (1 << 2) | 1;
    std::u16string symbol = u"IBM";
    const auto event = mapper->map(symbol, {schema->recordContext(0), fields}, VALUE_LIMITS, 7123,
                                   e::indexed_event_flags::SNAPSHOT_BEGIN);
    REQUIRE(event);
    CHECK(event->symbol == u"IBM");
    symbol[0] = u'X';
    CHECK(event->symbol == u"IBM");
    CHECK(event->eventTime == 7123);
    CHECK(event->eventFlags == e::indexed_event_flags::SNAPSHOT_BEGIN);
    CHECK(event->getIndex() == 12884901888LL);
    CHECK(event->getTime() == 3000);
    CHECK(event->getSequence() == 0);
    CHECK(event->getPrice() == 30.25);
    CHECK(event->getSizeAsDouble() == 100.5);
    CHECK(event->getSize() == 100);
    CHECK(std::isnan(event->getBidPrice()));
    CHECK(event->exchangeSaleConditions == std::optional<std::u16string>(u"AB"));
    CHECK(event->getTradeThroughExempt() == u'Q');
    CHECK(event->getAggressorSide() == e::Side::SELL);
    CHECK(event->isSpreadLeg());
    CHECK(event->isExtendedTradingHours());
    CHECK(event->isValidTick());
    CHECK(event->isCorrection());
    CHECK_FALSE(event->isNew());
    CHECK_FALSE(event->isCancel());
    CHECK(event->getSource().id() == 0);
    auto reserved = *event;
    reserved.flags = 3 | (3 << 5);
    CHECK(reserved.getType() == e::TimeAndSaleType::NEW);
    CHECK(reserved.getAggressorSide() == e::Side::UNDEFINED);
    CHECK(reserved.flags == (3 | (3 << 5)));
}

TEST_CASE("TimeAndSale preserves regional symbols, optional text, and finite text bounds") {
    const auto standard = e::defaultTimeAndSaleSchema(SCHEMA);
    REQUIRE(standard);
    auto record = standard->records()[0];
    record.name = u"TimeAndSale&Q";
    record.lookups.push_back({u"TimeNanoPart", record.fields.size()});
    record.fields.push_back({std::to_underlying(Type::COMPACT_INT)});
    record.lookups.push_back({u"Buyer", record.fields.size()});
    record.fields.push_back({std::to_underlying(Type::UTF_CHAR_ARRAY)});
    record.lookups.push_back({u"Seller", record.fields.size()});
    record.fields.push_back({std::to_underlying(Type::STRING)});
    const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapper = e::PreparedTimeAndSale::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = emptyValues(*schema);
    fields[0].raw = -1;
    fields[1].raw = 0x00400005;
    fields[2].raw = u'X';
    fields[10].raw = 765432;
    fields[11].units = u"buyer";
    fields[12].units = u"";
    const auto event = mapper->map(u"IBM", {schema->recordContext(0), fields}, VALUE_LIMITS);
    REQUIRE(event);
    CHECK(event->symbol == u"IBM&Q");
    CHECK(event->exchangeCode == u'X');
    CHECK(event->getTime() == -999);
    CHECK(event->getTimeNanos() == -998234568);
    CHECK(event->getSequence() == 5);
    CHECK(event->buyer == std::optional<std::u16string>(u"buyer"));
    CHECK(event->seller == std::optional<std::u16string>(u""));
    fields[11].units = u"changed";
    CHECK(event->buyer == std::optional<std::u16string>(u"buyer"));
    CHECK(mapper->map(u"IBM", {schema->recordContext(0), fields}, {4, 64, 256}) ==
          std::unexpected(e::MappingError::RESOURCE_LIMIT));
    CHECK(mapper->map(u"IBM", {schema->recordContext(0), fields}, {64, 6, 256}) ==
          std::unexpected(e::MappingError::RESOURCE_LIMIT));
    CHECK(mapper->map(u"IBM", {schema->recordContext(0), fields}, {64, 64, 11}) ==
          std::unexpected(e::MappingError::RESOURCE_LIMIT));
}

TEST_CASE("TimeAndSale rejects missing required fields and conflicting aliases") {
    const auto standard = e::defaultTimeAndSaleSchema(SCHEMA);
    REQUIRE(standard);
    auto missing = standard->records()[0];

    for (auto &lookup : missing.lookups)
        if (lookup.name == u"Time")
            lookup.name = u"UnmappedTime";

    const auto partial = c::LocalSchema::create(std::array{missing}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(partial);
    CHECK(e::PreparedTimeAndSale::prepare(*partial, 0, MAPPING) == std::unexpected(e::MappingError::MISSING_REQUIRED));
    auto conflict = standard->records()[0];
    conflict.fields.push_back({std::to_underlying(Type::WIDE_DECIMAL)});

    for (auto &lookup : conflict.lookups)
        if (lookup.name == u"BidPrice")
            lookup.fieldId = conflict.fields.size() - 1;

    const auto ambiguous = c::LocalSchema::create(std::array{conflict}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(ambiguous);
    CHECK(e::PreparedTimeAndSale::prepare(*ambiguous, 0, MAPPING) == std::unexpected(e::MappingError::AMBIGUOUS_FIELD));
}

TEST_CASE("TimeAndSale validates exact indexed removals and borrowed field lifetimes") {
    const auto standard = e::defaultTimeAndSaleSchema(SCHEMA);
    REQUIRE(standard);
    auto record = standard->records()[0];
    record.fields[0].serialTypeId = std::to_underlying(Type::UTF_CHAR);
    record.fields[1].serialTypeId = std::to_underlying(Type::UTF_CHAR);
    const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapper = e::PreparedTimeAndSale::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = emptyValues(*schema);
    fields[0].raw = -1;
    fields[1].raw = -1;
    std::vector<c::FieldValueView> borrowed;

    for (const auto &field : fields)
        borrowed.push_back({field.type, field.raw, {}, {}});

    const e::BorrowedRecordValueView input{schema->recordContext(0), borrowed};
    CHECK(mapper->mapView(u"IBM", input, VALUE_LIMITS) == std::unexpected(e::MappingError::INVALID_VALUES));
    CHECK(mapper->mapView(u"IBM", input, VALUE_LIMITS, 0, e::indexed_event_flags::REMOVE_EVENT, 0) ==
          std::unexpected(e::MappingError::INVALID_VALUES));
    const auto event = mapper->mapView(u"IBM", input, VALUE_LIMITS, 0,
                                       e::indexed_event_flags::REMOVE_EVENT | e::indexed_event_flags::TX_PENDING, -1);
    REQUIRE(event);
    CHECK(event->index == -1);
    CHECK(event->eventFlags == (e::indexed_event_flags::REMOVE_EVENT | e::indexed_event_flags::TX_PENDING));
    CHECK(event->getTime() == 23);
    CHECK(event->getSequence() == 0x3fffff);
}

TEST_CASE("Nuam sale requires matching identifiers and preserves indexed correction") {
    const auto schema = e::defaultNuamTimeAndSaleSchema(SCHEMA);
    REQUIRE(schema);
    const auto mapper = e::PreparedNuamTimeAndSale::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = emptyValues(*schema);
    fields[0].raw = -1;
    fields[1].raw = 0x00400005;
    fields[3].raw = 9007199254740993LL;
    fields[9].raw = 1;
    fields[10].raw = std::numeric_limits<std::int64_t>::min();
    const auto result = mapper->map(u"IBM", {schema->recordContext(0), fields}, VALUE_LIMITS, 7123,
                                    e::indexed_event_flags::SNAPSHOT_BEGIN);
    REQUIRE(result);
    CHECK(result->getMatchId() == std::numeric_limits<std::int64_t>::min());
    CHECK(result->getTradeId() == 9007199254740993LL);
    CHECK(result->getTime() == -999);
    CHECK(result->getSequence() == 5);
    CHECK(result->getEventFlags() == e::indexed_event_flags::SNAPSHOT_BEGIN);
    CHECK(result->getSource().id() == 0);
    CHECK(result->isCorrection());

    for (const auto required : {u"MatchId", u"TradeId"}) {
        auto missing = schema->records()[0];

        for (auto &lookup : missing.lookups) {
            if (lookup.name == required)
                lookup.name = u"UnrelatedId";
        }

        const auto missingSchema =
            c::LocalSchema::create(std::array{missing}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(missingSchema);
        const auto absent = e::PreparedNuamTimeAndSale::prepare(*missingSchema, 0, MAPPING);
        REQUIRE_FALSE(absent);
        CHECK(absent.error() == e::MappingError::MISSING_REQUIRED);
    }
}
