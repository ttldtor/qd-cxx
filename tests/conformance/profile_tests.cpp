// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/profile.hpp"
#include <array>
#include <doctest/doctest.h>
#include <limits>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace q = org::ttldtor::qdcxx::events;

/** Supply finite preparation budgets.
 * @return Bounds covering the default and designed schemas.
 */
q::MappingLimits mappingLimits() {
    return {64, 128, 32, 64, 64, 128, 8192, 100000};
}

/** Construct the default owned local schema.
 * @return Validated schema with all sixteen fields.
 */
c::LocalSchema profileSchema() {
    auto result = q::defaultProfileSchema({4, 64, 128, 128, 8192});
    REQUIRE(result);

    return std::move(*result);
}

/** Construct canonical zero/null values in local order.
 * @param schema Validated profile schema.
 * @return Owned values with exact full tags.
 */
std::vector<c::FieldValue> values(const c::LocalSchema &schema) {
    std::vector<c::FieldValue> result;

    for (const auto &field : schema.records()[0].fields) {
        result.push_back({static_cast<c::SerialFieldType>(field.serialTypeId), 0});
    }

    return result;
}
} // namespace

TEST_CASE("Profile preserves raw fields flags strings and independently supplied metadata") {
    auto schema = profileSchema();
    auto prepared = q::PreparedProfile::prepare(schema, 0, mappingLimits());
    REQUIRE(prepared);
    auto fields = values(schema);
    fields[0].raw = 32002; // 1.25 as exact wide decimal.
    fields[4].raw = -17;
    fields[11].raw = std::numeric_limits<std::int32_t>::min();
    fields[12].raw = std::numeric_limits<std::int32_t>::max();
    fields[13].raw = 0x76543216;
    fields[14].units = std::u16string{u'A', u'\0', char16_t{0xd800}, char16_t{0xdfff}};
    fields[15].units = u"";
    auto result = prepared->map(u"IBM", {schema.recordContext(0), fields}, {3, 4, 7}, 123);
    REQUIRE(result);
    CHECK(result->beta.raw == 32002);
    CHECK(result->exDividendDayId == -17);
    CHECK(result->haltStartTimeMs == -2147483648000LL);
    CHECK(result->haltEndTimeMs == 2147483647000LL);
    CHECK(result->flags == 0x76543216);
    CHECK(result->tradingStatus() == q::TradingStatus::ACTIVE);
    CHECK(result->shortSaleRestriction() == q::ShortSaleRestriction::ACTIVE);
    CHECK_FALSE(result->isTradingHalted());
    CHECK(result->isShortSaleRestricted());
    CHECK(result->eventTime == 123);
    fields.clear();
    CHECK(result->symbol == u"IBM");
    REQUIRE(result->description);
    CHECK(result->description->size() == 4);
    REQUIRE(result->statusReason);
    CHECK(result->statusReason->empty());
    result->flags = -1;
    CHECK(result->tradingStatus() == q::TradingStatus::UNDEFINED);
    CHECK(result->shortSaleRestriction() == q::ShortSaleRestriction::UNDEFINED);
    CHECK_FALSE(result->isTradingHalted());
    CHECK_FALSE(result->isShortSaleRestricted());
}

TEST_CASE("Profile preparation requires Description and defaults only absent local fields") {
    c::LocalRecord record{0, u"Configured", false, {{10}}, {{u"Description", 0}}};
    auto schema = c::LocalSchema::create(std::span(&record, 1), {1, 1, 1, 32, 64}, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(schema);
    auto prepared = q::PreparedProfile::prepare(*schema, 0, mappingLimits());
    REQUIRE(prepared);
    std::vector<c::FieldValue> fields{{c::SerialFieldType::UTF_CHAR_ARRAY, 0}};
    auto result = prepared->map(u"", {schema->recordContext(0), fields}, {0, 0, 0});
    REQUIRE(result);
    CHECK(result->beta.kind() == c::DecimalKind::NAN_VALUE);
    CHECK(result->high52WeekPrice.kind() == c::DecimalKind::NAN_VALUE);
    CHECK(result->haltStartTimeMs == 0);
    CHECK(result->flags == 0);
    CHECK_FALSE(result->description);
    CHECK_FALSE(result->statusReason);
    CHECK(result->eventTime == 0);
    record.lookups[0].name = u"Other";
    auto missing =
        c::LocalSchema::create(std::span(&record, 1), {1, 1, 1, 32, 64}, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(missing);
    CHECK(q::PreparedProfile::prepare(*missing, 0, mappingLimits()).error() == q::MappingError::MISSING_REQUIRED);
}

TEST_CASE("Profile rejects wrong contexts and malformed extras before string budgets") {
    auto schema = profileSchema();
    auto other = profileSchema();
    auto prepared = q::PreparedProfile::prepare(schema, 0, mappingLimits());
    REQUIRE(prepared);
    auto fields = values(schema);
    CHECK(prepared->map(u"too long", {other.recordContext(0), fields}, {0, 0, 0}).error() ==
          q::MappingError::WRONG_CONTEXT);
    fields[14].raw = 1;
    CHECK(prepared->map(u"too long", {schema.recordContext(0), fields}, {0, 0, 0}).error() ==
          q::MappingError::INVALID_VALUES);
    fields[14].raw = 0;
    fields[14].units = u"abcd";
    fields[15].units = u"xy";
    CHECK(prepared->map(u"IBM", {schema.recordContext(0), fields}, {2, 4, 9}).error() ==
          q::MappingError::RESOURCE_LIMIT);
    CHECK(prepared->map(u"IBM", {schema.recordContext(0), fields}, {3, 3, 9}).error() ==
          q::MappingError::RESOURCE_LIMIT);
    CHECK(prepared->map(u"IBM", {schema.recordContext(0), fields}, {3, 4, 8}).error() ==
          q::MappingError::RESOURCE_LIMIT);
    REQUIRE(prepared->map(u"IBM", {schema.recordContext(0), fields}, {3, 4, 9}));
    CHECK(q::PreparedProfile::prepare(schema, 0, {}).error() == q::MappingError::RESOURCE_LIMIT);
}

TEST_CASE("Profile reordered aliases and int numerics preserve generic extras and ownership") {
    c::LocalRecord record{0,
                          u"Configured",
                          false,
                          {{8}, {10}, {40}, {8}},
                          {{u"HighPrice52", 0}, {u"Description", 1}, {u"Extra", 2}, {u"Beta", 3}}};
    auto schema =
        c::LocalSchema::create(std::span(&record, 1), {1, 4, 4, 32, 128}, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(schema);
    auto prepared = q::PreparedProfile::prepare(*schema, 0, mappingLimits());
    REQUIRE(prepared);
    auto retained = prepared->context();
    std::vector<c::FieldValue> fields{{c::SerialFieldType::COMPACT_INT, 0},
                                      {c::SerialFieldType::UTF_CHAR_ARRAY, 0, u"text"},
                                      {c::SerialFieldType::SHORT_STRING, 42},
                                      {c::SerialFieldType::COMPACT_INT, std::numeric_limits<std::int32_t>::min()}};
    auto result = prepared->map(u"", {retained, fields}, {0, 4, 4});
    REQUIRE(result);
    CHECK(result->high52WeekPrice.toDouble() == 0.0);
    CHECK(result->beta.toDouble() == -2147483648.0);
    CHECK(fields[0].type == c::SerialFieldType::COMPACT_INT);
    CHECK(fields[2].raw == 42);
    fields[2].units = u"invalid scalar";
    CHECK(prepared->map(u"", {retained, fields}, {0, 4, 4}).error() == q::MappingError::INVALID_VALUES);
}

TEST_CASE("Profile prepared plan survives source destruction and nonzero local record selection") {
    auto prepared = [] {
        const std::array<c::LocalRecord, 2> records{
            c::LocalRecord{0, u"Custom", false, {{8}}, {{u"Counter", 0}}},
            c::LocalRecord{1, u"Profile", false, {{10}}, {{u"Description", 0}}}};
        auto schema = c::LocalSchema::create(records, {2, 2, 2, 32, 128}, c::LocalProfile::EXTENDED_FIELDS_V1);
        REQUIRE(schema);
        auto result = q::PreparedProfile::prepare(*schema, 1, mappingLimits());
        REQUIRE(result);

        return std::move(*result);
    }();
    const std::vector<c::FieldValue> fields{{c::SerialFieldType::UTF_CHAR_ARRAY, 0, u"owned"}};
    auto moved = std::move(prepared);
    auto result = moved.map(u"S", {moved.context(), fields}, {1, 5, 6});
    REQUIRE(result);
    CHECK(result->description == std::optional<std::u16string>{u"owned"});
    CHECK(q::defaultProfileSchema({}).error() == c::SchemaError::RESOURCE_LIMIT);
}

TEST_CASE("Profile business status views preserve reserved flag bits") {
    const std::array<q::TradingStatus, 4> trading{q::TradingStatus::UNDEFINED, q::TradingStatus::HALTED,
                                                  q::TradingStatus::ACTIVE, q::TradingStatus::UNDEFINED};
    const std::array<q::ShortSaleRestriction, 4> restriction{
        q::ShortSaleRestriction::UNDEFINED, q::ShortSaleRestriction::ACTIVE, q::ShortSaleRestriction::INACTIVE,
        q::ShortSaleRestriction::UNDEFINED};
    q::Profile profile{};

    for (std::int32_t raw = 0; raw < 16; ++raw) {
        profile.flags = raw | 0x76543200;
        CHECK(profile.tradingStatus() == trading[static_cast<std::size_t>(raw & 3)]);
        CHECK(profile.shortSaleRestriction() == restriction[static_cast<std::size_t>(raw >> 2)]);
        CHECK(profile.isTradingHalted() == ((raw & 3) == 1));
        CHECK(profile.isShortSaleRestricted() == ((raw >> 2) == 1));
        CHECK(profile.flags == (raw | 0x76543200));
    }
}
