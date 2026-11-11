// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/event_flags.hpp"
#include "qd/events/theo_price.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <doctest/doctest.h>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace c = q::core;
namespace e = q::events;
using Type = c::SerialFieldType;

constexpr c::SchemaLimits SCHEMA{1, 32, 32, 64, 512};
constexpr e::MappingLimits MAPPING{32, 32, 16, 32, 128, 64, 1024, 10000};
constexpr std::array<std::u16string_view, 5> REQUIRED{u"Theo.Time", u"Theo.Price", u"Theo.UnderlyingPrice",
                                                      u"Theo.Delta", u"Theo.Gamma"};

std::vector<c::FieldValue> zeroValues(const c::LocalSchema &schema) {
    std::vector<c::FieldValue> result;

    for (const auto &field : schema.records()[0].fields) {
        result.push_back({static_cast<Type>(field.serialTypeId), 0});
    }

    return result;
}
} // namespace

TEST_CASE("TheoPrice mapping owns values and preserves exact time words and decimal snapshots") {
    const auto schema = e::defaultTheoPriceSchema(SCHEMA);
    REQUIRE(schema);
    const auto mapper = e::PreparedTheoPrice::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = zeroValues(*schema);
    fields[0].raw = -1;
    fields[1].raw = 0x00400005;
    fields[2].raw = 0x180;
    fields[3].raw = 0x280;
    std::u16string symbol = u"IBM";
    const auto result = mapper->map(symbol, {schema->recordContext(0), fields}, symbol.size(), 1234, 5);
    REQUIRE(result);
    CHECK(result->symbol == u"IBM");
    symbol[0] = u'X';
    CHECK(result->symbol == u"IBM");
    CHECK(result->eventTime == 1234);
    CHECK(result->eventFlags == 5);
    CHECK(result->getSource().id() == 0);
    CHECK(result->getTime() == -999);
    CHECK(result->getSequence() == 5);
    CHECK(result->getPrice() == 1);
    CHECK(result->getUnderlyingPrice() == 2);
    CHECK(std::isnan(result->getDelta()));
    CHECK(mapper->map(symbol, {schema->recordContext(0), fields}, 2) ==
          std::unexpected(e::MappingError::RESOURCE_LIMIT));
    const auto other = e::defaultTheoPriceSchema(SCHEMA);
    REQUIRE(other);
    CHECK(mapper->map(symbol, {other->recordContext(0), fields}, 3) == std::unexpected(e::MappingError::WRONG_CONTEXT));
    fields[0].raw = std::numeric_limits<std::int64_t>::max();
    CHECK(mapper->map(symbol, {schema->recordContext(0), fields}, 3) ==
          std::unexpected(e::MappingError::INVALID_VALUES));
}

TEST_CASE("TheoPrice missing sequence and optional values default while required fields refuse") {
    c::LocalRecord record{0, u"TheoPrice", false, {}, {}};

    for (const auto name : REQUIRED) {
        record.lookups.push_back({std::u16string(name), record.fields.size()});
        record.fields.push_back({std::to_underlying(name == u"Theo.Time" ? Type::TIME_SECONDS : Type::WIDE_DECIMAL)});
    }

    const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapper = e::PreparedTheoPrice::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = zeroValues(*schema);
    fields[0].raw = -1;
    const auto event = mapper->map(u"IBM", {schema->recordContext(0), fields}, 3);
    REQUIRE(event);
    CHECK(event->getIndex() == -4294967296LL);
    CHECK(event->getTime() == -1000);
    CHECK(event->getSequence() == 0);
    CHECK(std::isnan(event->getDividend()));
    CHECK(std::isnan(event->getInterest()));

    for (const auto omitted : REQUIRED) {
        c::LocalRecord partial{0, u"TheoPrice", false, {}, {}};

        for (const auto name : REQUIRED) {
            if (name != omitted) {
                partial.lookups.push_back({std::u16string(name), partial.fields.size()});
                partial.fields.push_back(
                    {std::to_underlying(name == u"Theo.Time" ? Type::TIME_SECONDS : Type::WIDE_DECIMAL)});
            }
        }

        const auto missing = c::LocalSchema::create(std::array{partial}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(missing);
        CHECK(e::PreparedTheoPrice::prepare(*missing, 0, MAPPING) ==
              std::unexpected(e::MappingError::MISSING_REQUIRED));
    }
}

TEST_CASE("TheoPrice removal validates raw indexed coordinate and retains complete signed words") {
    const auto standard = e::defaultTheoPriceSchema(SCHEMA);
    REQUIRE(standard);
    auto record = standard->records()[0];
    record.fields[0].serialTypeId = std::to_underlying(Type::UTF_CHAR);
    record.fields[1].serialTypeId = std::to_underlying(Type::UTF_CHAR);
    const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapper = e::PreparedTheoPrice::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = zeroValues(*schema);
    fields[0].raw = -1;
    fields[1].raw = -1;
    const e::RecordValueView values{schema->recordContext(0), fields};
    CHECK(mapper->map(u"IBM", values, 3) == std::unexpected(e::MappingError::INVALID_VALUES));
    CHECK(mapper->map(u"IBM", values, 3, 0, 0, -1) == std::unexpected(e::MappingError::INVALID_INPUT));
    CHECK(mapper->map(u"IBM", values, 3, 0, e::indexed_event_flags::REMOVE_EVENT, 0) ==
          std::unexpected(e::MappingError::INVALID_VALUES));
    const auto event = mapper->map(u"IBM", values, 3, 0, e::indexed_event_flags::REMOVE_EVENT, -1);
    REQUIRE(event);
    CHECK(event->getIndex() == -1);
    CHECK(event->getTime() == 23);
    CHECK(event->getSequence() == 0x3fffff);
}

TEST_CASE("TheoPrice rejects conflicting aliases and retains direct LONG business conversion") {
    const auto standard = e::defaultTheoPriceSchema(SCHEMA);
    REQUIRE(standard);
    auto record = standard->records()[0];
    record.fields[2].serialTypeId = std::to_underlying(Type::LONG);
    const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapper = e::PreparedTheoPrice::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = zeroValues(*schema);
    fields[2].raw = 9007199254740993LL;
    std::vector<c::FieldValueView> views;

    for (const auto &field : fields) {
        views.push_back({field.type, field.raw, {}, {}});
    }

    const auto event = mapper->mapView(u"IBM", {schema->recordContext(0), views}, 3);
    REQUIRE(event);
    CHECK(event->getPrice() == static_cast<double>(fields[2].raw));
    views[2].raw = 1;
    CHECK(event->getPrice() == static_cast<double>(9007199254740993LL));

    record.fields.push_back({std::to_underlying(Type::LONG)});
    record.lookups[5].fieldId = record.fields.size() - 1;
    const auto ambiguous = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(ambiguous);
    CHECK(e::PreparedTheoPrice::prepare(*ambiguous, 0, MAPPING) == std::unexpected(e::MappingError::AMBIGUOUS_FIELD));
}
