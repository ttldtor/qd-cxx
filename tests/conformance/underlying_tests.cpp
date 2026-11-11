// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/event_flags.hpp"
#include "qd/events/underlying.hpp"

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

std::vector<c::FieldValue> zeroValues(const c::LocalSchema &schema) {
    std::vector<c::FieldValue> result;

    for (const auto &field : schema.records()[0].fields) {
        result.push_back({static_cast<Type>(field.serialTypeId), 0});
    }

    return result;
}
} // namespace

TEST_CASE("Underlying mapping owns values and preserves exact time words and decimal snapshots") {
    const auto schema = e::defaultUnderlyingSchema(SCHEMA);
    REQUIRE(schema);
    const auto mapper = e::PreparedUnderlying::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = zeroValues(*schema);
    fields[0].raw = -1;
    fields[1].raw = 0x00400005;
    fields[2].raw = 0x180;
    fields[5].raw = 0x180;
    fields[6].raw = 0x280;
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
    CHECK(result->getVolatility() == 1);
    CHECK(std::isnan(result->getFrontVolatility()));
    CHECK(result->getCallVolume() == 1);
    CHECK(result->getPutVolume() == 2);
    CHECK(result->getOptionVolume() == 3);
    CHECK(mapper->map(symbol, {schema->recordContext(0), fields}, 2) ==
          std::unexpected(e::MappingError::RESOURCE_LIMIT));
    const auto other = e::defaultUnderlyingSchema(SCHEMA);
    REQUIRE(other);
    CHECK(mapper->map(symbol, {other->recordContext(0), fields}, 3) == std::unexpected(e::MappingError::WRONG_CONTEXT));
    fields[0].raw = std::numeric_limits<std::int64_t>::max();
    CHECK(mapper->map(symbol, {schema->recordContext(0), fields}, 3) ==
          std::unexpected(e::MappingError::INVALID_VALUES));
}

TEST_CASE("Underlying optional fields default to zero time and NaN, and volume uses available side") {
    c::LocalRecord record{0, u"Underlying", false, {}, {}};
    record.fields.push_back({std::to_underlying(Type::WIDE_DECIMAL)});
    record.lookups.push_back({u"CallVolume", 0});
    const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapper = e::PreparedUnderlying::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = zeroValues(*schema);
    fields[0].raw = 0x180;
    const auto event = mapper->map(u"IBM", {schema->recordContext(0), fields}, 3);
    REQUIRE(event);
    CHECK(event->getIndex() == 0);
    CHECK(event->getTime() == 0);
    CHECK(event->getSequence() == 0);
    CHECK(std::isnan(event->getVolatility()));
    CHECK(std::isnan(event->getFrontVolatility()));
    CHECK(std::isnan(event->getBackVolatility()));
    CHECK(std::isnan(event->getPutVolume()));
    CHECK(std::isnan(event->getPutCallRatio()));
    CHECK(event->getOptionVolume() == 1);

    fields[0].raw = 0;
    const auto bothAbsent = mapper->map(u"IBM", {schema->recordContext(0), fields}, 3);
    REQUIRE(bothAbsent);
    CHECK(std::isnan(bothAbsent->getOptionVolume()));

    record.lookups[0].name = u"PutVolume";
    const auto putSchema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(putSchema);
    const auto putMapper = e::PreparedUnderlying::prepare(*putSchema, 0, MAPPING);
    REQUIRE(putMapper);
    fields[0].raw = 0x280;
    const auto putOnly = putMapper->map(u"IBM", {putSchema->recordContext(0), fields}, 3);
    REQUIRE(putOnly);
    CHECK(std::isnan(putOnly->getCallVolume()));
    CHECK(putOnly->getOptionVolume() == 2);
}

TEST_CASE("Underlying removal validates raw indexed coordinates and retains complete signed words") {
    const auto standard = e::defaultUnderlyingSchema(SCHEMA);
    REQUIRE(standard);
    auto record = standard->records()[0];
    record.fields[0].serialTypeId = std::to_underlying(Type::UTF_CHAR);
    record.fields[1].serialTypeId = std::to_underlying(Type::UTF_CHAR);
    const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapper = e::PreparedUnderlying::prepare(*schema, 0, MAPPING);
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

TEST_CASE("Underlying borrowed mapping retains the original numeric business conversion") {
    const auto standard = e::defaultUnderlyingSchema(SCHEMA);
    REQUIRE(standard);
    auto record = standard->records()[0];
    record.fields[2].serialTypeId = std::to_underlying(Type::LONG);
    const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapper = e::PreparedUnderlying::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = zeroValues(*schema);
    fields[2].raw = 9007199254740993LL;
    std::vector<c::FieldValueView> views;

    for (const auto &field : fields) {
        views.push_back({field.type, field.raw, {}, {}});
    }

    const auto event = mapper->mapView(u"IBM", {schema->recordContext(0), views}, 3);
    REQUIRE(event);
    CHECK(event->getVolatility() == static_cast<double>(fields[2].raw));
    views[2].raw = 1;
    CHECK(event->getVolatility() == static_cast<double>(9007199254740993LL));
}
