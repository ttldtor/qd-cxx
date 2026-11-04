// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/text_events.hpp"
#include <array>
#include <doctest/doctest.h>

namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::events;
constexpr c::SchemaLimits SCHEMA{4, 32, 64, 128, 4096};
constexpr e::MappingLimits MAPPING{32, 64, 8, 16, 128, 128, 4096, 100000};

TEST_CASE("TextMessage owns nullable exact text and preserves packed time") {
    auto schema = e::defaultTextMessageSchema(SCHEMA);
    REQUIRE(schema);
    auto mapper = e::PreparedTextMessage::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    std::u16string payload{u'A', u'\0', char16_t{0xd800}, char16_t{0xdc00}};
    std::array fields{c::FieldValueView{c::SerialFieldType::TIME_SECONDS, -1, {}, {}},
                      c::FieldValueView{c::SerialFieldType::SEQUENCE, (123 << 22) | 9, {}, {}},
                      c::FieldValueView{c::SerialFieldType::UTF_CHAR_ARRAY, 0, payload, {}}};
    auto result = mapper->mapView(u"X", {schema->recordContext(0), fields}, {1, 4, 5}, 77);
    REQUIRE(result);
    CHECK(result->getTime() == -877);
    CHECK(result->getSequence() == 9);
    CHECK(result->eventTime == 77);
    std::array owned{c::FieldValue{c::SerialFieldType::TIME_SECONDS, -1},
                     c::FieldValue{c::SerialFieldType::SEQUENCE, (123 << 22) | 9},
                     c::FieldValue{c::SerialFieldType::UTF_CHAR_ARRAY, 0, payload}};
    auto copied = mapper->map(u"X", {schema->recordContext(0), owned}, {1, 4, 5}, 77);
    REQUIRE(copied);
    CHECK(copied->text == result->text);
    CHECK(copied->timeSequence == result->timeSequence);
    REQUIRE(result->text);
    CHECK(result->text->size() == 4);
    CHECK(result->text->at(2) == char16_t{0xd800});
    CHECK(mapper->mapView(u"X", {schema->recordContext(0), fields}, {1, 3, 4}).error() ==
          e::MappingError::RESOURCE_LIMIT);
    payload.clear();
    fields[2].units.reset();
    auto null = mapper->mapView(u"", {schema->recordContext(0), fields}, {0, 0, 0});
    REQUIRE(null);
    CHECK_FALSE(null->text);
    fields[2].units = u"";
    auto empty = mapper->mapView(u"", {schema->recordContext(0), fields}, {0, 0, 0});
    REQUIRE(empty);
    REQUIRE(empty->text);
    CHECK(empty->text->empty());
    fields[2].raw = 1;
    CHECK(mapper->mapView(u"", {schema->recordContext(0), fields}, {0, 0, 0}).error() ==
          e::MappingError::INVALID_VALUES);
}

TEST_CASE("TextConfiguration accepts absent optional fields and validates extras") {
    c::LocalRecord record{0, u"Custom", false, {{10}, {8}}, {{u"Text", 0}, {u"Extra", 1}}};
    auto schema = c::LocalSchema::create(std::span(&record, 1), SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    auto mapper = e::PreparedTextConfiguration::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    CHECK_FALSE(mapper->versionField());
    std::array fields{c::FieldValueView{c::SerialFieldType::UTF_CHAR_ARRAY, 0, u"ok", {}},
                      c::FieldValueView{c::SerialFieldType::COMPACT_INT, 7, {}, {}}};
    auto result = mapper->mapView(u"Q", {schema->recordContext(0), fields}, {1, 2, 3});
    REQUIRE(result);
    CHECK(result->version == 0);
    CHECK(result->getTime() == 0);
    REQUIRE(result->text);
    CHECK(*result->text == u"ok");
    fields[1].type = c::SerialFieldType::TIME_SECONDS;
    CHECK(mapper->mapView(u"Q", {schema->recordContext(0), fields}, {1, 2, 3}).error() ==
          e::MappingError::INVALID_VALUES);
    record.lookups[0].name = u"Other";
    auto missing = c::LocalSchema::create(std::span(&record, 1), SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(missing);
    CHECK(e::PreparedTextConfiguration::prepare(*missing, 0, MAPPING).error() == e::MappingError::MISSING_REQUIRED);
}
