// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/event_mapping.hpp"
#include "qd/protocol/stream_data.hpp"
#include "qd/protocol/tape_reader.hpp"
#include <array>
#include <doctest/doctest.h>
#include <limits>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::events;
namespace p = org::ttldtor::qdcxx::protocol;
constexpr c::SchemaLimits SCHEMA_LIMITS{8, 32, 64, 64, 4096};                            ///< Finite schema test bounds.
constexpr e::MappingLimits MAPPING_LIMITS{32, 64, 16, 64, 32, 64, 4096, 100000};         ///< Finite preparation bounds.
constexpr std::array<c::SerialFieldType, 1> INTS{c::SerialFieldType::COMPACT_INT};       ///< Exact scalar request.
constexpr std::array<c::SerialFieldType, 1> STRINGS{c::SerialFieldType::UTF_CHAR_ARRAY}; ///< Exact string request.

/** Construct a custom record after an unrelated record; keep extra fields and aliases.
 * @param swapped Exchange meanings while preserving IDs, counts and full type shape.
 * @return Independent owned schema.
 */
c::LocalSchema schema(bool swapped = false) {
    auto result = c::LocalSchema::create(
        std::vector<c::LocalRecord>{
            {0, u"Other", false, {{8}}, {{u"Value", 0}}},
            {1,
             u"Custom",
             false,
             {{8}, {8}, {10}},
             {{u"B", swapped ? 1u : 0u}, {u"A", swapped ? 0u : 1u}, {u"AliasA", swapped ? 0u : 1u}, {u"Extra", 2}}}},
        SCHEMA_LIMITS, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Prepare one explicit logical property.
 * @param local Validated schema.
 * @param keys Borrowed accepted aliases.
 * @param required Missing-field policy.
 * @param limits Preparation bounds.
 * @return Immutable plan or the exact error.
 */
std::expected<e::EventMapping, e::MappingError> prepare(const c::LocalSchema &local,
                                                        std::span<const std::u16string_view> keys, bool required = true,
                                                        e::MappingLimits limits = MAPPING_LIMITS) {
    const std::array requests{e::FieldRequest{keys, INTS, required}};

    return e::EventMapping::prepare(local, 1, requests, limits);
}

/** Supply a complete canonical shape, including unselected string contents.
 * @return Owned exact local values.
 */
std::vector<c::FieldValue> values() {
    return {{c::SerialFieldType::COMPACT_INT, 12},
            {c::SerialFieldType::COMPACT_INT, 34},
            {c::SerialFieldType::UTF_CHAR_ARRAY, 0, u"extra"}};
}
} // namespace

TEST_CASE("prepared mapping selects aliases and optional absence without dropping extras") {
    auto local = schema();
    const std::array<std::u16string_view, 2> aliases{u"A", u"AliasA"};
    const std::array<std::u16string_view, 1> absent{u"Absent"};
    const std::array requests{e::FieldRequest{aliases, INTS, true}, e::FieldRequest{absent, STRINGS, false}};
    auto mapping = e::EventMapping::prepare(local, 1, requests, MAPPING_LIMITS);
    REQUIRE(mapping);
    REQUIRE(mapping->selections().size() == 2);
    CHECK(mapping->selections()[0] == 1);
    CHECK_FALSE(mapping->selections()[1]);
    auto input = values();
    CHECK(mapping->validate({local.recordContext(1), input}));
    CHECK(input[2].units == u"extra");
    CHECK(mapping->context().matches(local.recordContext(1)));
}

TEST_CASE("context uses owned schema identity and record identity") {
    auto local = schema();
    auto equal = schema();
    auto swapped = schema(true);
    auto copy = local;
    auto bounded = local.boundedCopy(SCHEMA_LIMITS);
    REQUIRE(bounded);
    const std::array<std::u16string_view, 1> key{u"A"};
    auto mapping = prepare(local, key);
    REQUIRE(mapping);
    auto input = values();
    CHECK(mapping->validate({copy.recordContext(1), input}));
    CHECK(mapping->validate({bounded->recordContext(1), input}));

    for (const auto &context : {equal.recordContext(1), swapped.recordContext(1), local.recordContext(0),
                                local.recordContext(-1), local.recordContext(2), c::RecordContext{}}) {
        CHECK(mapping->validate({context, input}).error() == e::MappingError::WRONG_CONTEXT);
    }

    CHECK_FALSE(c::RecordContext{}.matches(c::RecordContext{}));
    CHECK_FALSE(local.recordContext(-1).valid());
    CHECK(local.recordContext(1).valid());
    auto moved = std::move(copy);
    CHECK(mapping->validate({moved.recordContext(1), input}));
    CHECK_FALSE(copy.recordContext(1).valid());
}

TEST_CASE("preparation inputs may move and disappear without dangling mapping names or context") {
    auto mapping = [] {
        auto local = schema();
        std::u16string name = u"AliasA";
        const std::array<std::u16string_view, 1> keys{name};
        auto prepared = prepare(local, keys);
        REQUIRE(prepared);
        name.assign(100, u'Z');

        return std::move(*prepared);
    }();
    auto input = values();
    CHECK(mapping.validate({mapping.context(), input}));
    REQUIRE(mapping.selections().size() == 1);
    CHECK(mapping.selections()[0] == 1);
}

TEST_CASE("missing ambiguous incompatible and malformed field requests reject atomically") {
    auto local = schema();
    const std::array<std::u16string_view, 1> missing{u"Missing"};
    CHECK(prepare(local, missing).error() == e::MappingError::MISSING_REQUIRED);
    CHECK(prepare(local, missing, false));
    const std::array<std::u16string_view, 2> ambiguous{u"A", u"B"};
    CHECK(prepare(local, ambiguous).error() == e::MappingError::AMBIGUOUS_FIELD);
    const std::array<std::u16string_view, 1> wrong{u"Extra"};
    CHECK(prepare(local, wrong).error() == e::MappingError::INCOMPATIBLE_TYPE);
    CHECK(prepare(local, wrong, false).error() == e::MappingError::INCOMPATIBLE_TYPE);

    for (const std::u16string &key : {std::u16string{}, std::u16string(1, 0xd800), std::u16string(1, 0xdc00),
                                      std::u16string{char16_t{0xdc00}, char16_t{0xd800}}}) {
        const std::array<std::u16string_view, 1> keys{key};
        CHECK(prepare(local, keys).error() == e::MappingError::INVALID_INPUT);
    }

    CHECK(prepare(local, {}).error() == e::MappingError::INVALID_INPUT);
    const std::array<std::u16string_view, 1> keys{u"A"};
    const std::array<c::SerialFieldType, 1> unknown{static_cast<c::SerialFieldType>(24)};

    for (const auto types : {std::span<const c::SerialFieldType>{}, std::span<const c::SerialFieldType>(unknown)}) {
        const std::array requests{e::FieldRequest{keys, types, true}};
        CHECK(e::EventMapping::prepare(local, 1, requests, MAPPING_LIMITS).error() == e::MappingError::INVALID_INPUT);
    }

    CHECK(e::EventMapping::prepare(local, -1, {}, MAPPING_LIMITS).error() == e::MappingError::INVALID_INPUT);
    CHECK(e::EventMapping::prepare(local, 2, {}, MAPPING_LIMITS).error() == e::MappingError::INVALID_INPUT);
}

TEST_CASE("schema and mapping keys preserve NUL and paired UTF-16 units") {
    for (const std::u16string &name : {std::u16string(1, u'\0'), std::u16string{char16_t{0xd800}, char16_t{0xdc00}}}) {
        const std::array<c::LocalRecord, 1> records{{{0, name, false, {{8}}, {{name, 0}}}}};
        auto local = c::LocalSchema::create(records, SCHEMA_LIMITS);
        REQUIRE(local);
        CHECK(local->records()[0].name == name);
        CHECK(local->records()[0].lookups[0].name == name);
        const std::array<std::u16string_view, 1> keys{name};
        const std::array requests{e::FieldRequest{keys, INTS, true}};
        auto mapping = e::EventMapping::prepare(*local, 0, requests, MAPPING_LIMITS);
        REQUIRE(mapping);
        REQUIRE(mapping->selections().size() == 1);
        CHECK(mapping->selections()[0] == 0);
    }
}

TEST_CASE("schema rejects malformed record and lookup names before their text budgets") {
    for (const std::u16string &name : {std::u16string{}, std::u16string(1, 0xd800), std::u16string(1, 0xdc00),
                                       std::u16string{char16_t{0xdc00}, char16_t{0xd800}}}) {
        for (const bool recordName : {true, false}) {
            c::LocalRecord record{0, u"R", false, {{8}}, {{u"A", 0}}};

            if (recordName) {
                record.name = name;
            } else {
                record.lookups[0].name = name;
            }

            for (const bool exhausted : {false, true}) {
                auto limits = SCHEMA_LIMITS;

                if (exhausted) {
                    // A valid record consumes the one available unit before the invalid lookup.
                    limits.totalNameUnits = recordName ? 0 : 1;
                }

                const auto result = c::LocalSchema::create(std::span(&record, 1), limits);
                REQUIRE_FALSE(result);
                CHECK(result.error() == c::SchemaError::INVALID_SCHEMA);
            }
        }
    }
}

TEST_CASE("mapping text budget precedes malformed request key validation") {
    auto local = schema();
    const std::u16string malformed(1, 0xd800);
    const std::array<std::u16string_view, 1> keys{malformed};
    auto limits = MAPPING_LIMITS;
    // B, A, AliasA and Extra exhaust thirteen source lookup units before the request key.
    limits.totalNameUnits = 13;
    CHECK(prepare(local, keys, true, limits).error() == e::MappingError::RESOURCE_LIMIT);
    limits.totalNameUnits = 14;
    CHECK(prepare(local, keys, true, limits).error() == e::MappingError::INVALID_INPUT);
}

TEST_CASE("exact full identity alternatives are explicit and cannot use shared serialization bits") {
    auto local = schema();
    const std::array<std::u16string_view, 1> keys{u"A"};
    const std::array alternatives{c::SerialFieldType::WIDE_DECIMAL, c::SerialFieldType::COMPACT_INT};
    const std::array requests{e::FieldRequest{keys, alternatives, true}};
    CHECK(e::EventMapping::prepare(local, 1, requests, MAPPING_LIMITS));
    const std::array other{c::SerialFieldType::TIME_SECONDS};
    const std::array bad{e::FieldRequest{keys, other, true}};
    CHECK(e::EventMapping::prepare(local, 1, bad, MAPPING_LIMITS).error() == e::MappingError::INCOMPATIBLE_TYPE);
}

TEST_CASE("preparation error precedence and literal lookup matching are deterministic") {
    auto local = schema();
    const std::array<std::u16string_view, 1> key{u"A"};
    const std::array requests{e::FieldRequest{key, INTS, true}};
    CHECK(e::EventMapping::prepare(local, -1, requests, {}).error() == e::MappingError::INVALID_INPUT);
    const std::array<std::u16string_view, 1> emptyKey{u""};
    const std::array invalid{e::FieldRequest{emptyKey, INTS, true}};
    CHECK(e::EventMapping::prepare(local, 1, invalid, {}).error() == e::MappingError::RESOURCE_LIMIT);
    auto noWork = MAPPING_LIMITS;
    noWork.comparisonWork = 0;
    const std::array<std::u16string_view, 1> missing{u"Missing"};
    CHECK(prepare(local, missing, true, noWork).error() == e::MappingError::RESOURCE_LIMIT);
    const std::array<std::u16string_view, 2> ambiguous{u"A", u"Extra"};
    CHECK(prepare(local, ambiguous).error() == e::MappingError::AMBIGUOUS_FIELD);

    for (const std::u16string_view text : {u"a", u"Custom.A", u"Alias.A"}) {
        const std::array keys{text};
        CHECK(prepare(local, keys).error() == e::MappingError::MISSING_REQUIRED);
    }

    const std::array<std::u16string_view, 2> duplicates{u"A", u"A"};
    CHECK(prepare(local, duplicates));
    auto enormousWork = MAPPING_LIMITS;
    enormousWork.comparisonWork = std::numeric_limits<std::size_t>::max();
    CHECK(prepare(local, key, true, enormousWork));
}

TEST_CASE("projection checks context before complete shape including unselected canonical values") {
    auto local = schema();
    const std::array<std::u16string_view, 1> key{u"A"};
    auto mapping = prepare(local, key);
    REQUIRE(mapping);
    CHECK(mapping->validate({{}, {}}).error() == e::MappingError::WRONG_CONTEXT);
    CHECK(mapping->validate({local.recordContext(1), {}}).error() == e::MappingError::INVALID_VALUES);
    auto input = values();
    input[2].raw = 1;
    CHECK(mapping->validate({local.recordContext(1), input}).error() == e::MappingError::INVALID_VALUES);
    input = values();
    input[0].units = u"not scalar";
    CHECK(mapping->validate({local.recordContext(1), input}).error() == e::MappingError::INVALID_VALUES);
    input = values();
    input[0].raw = std::numeric_limits<std::int64_t>::max();
    CHECK(mapping->validate({local.recordContext(1), input}).error() == e::MappingError::INVALID_VALUES);
    input = values();
    input[0].type = c::SerialFieldType::TIME_SECONDS;
    CHECK(mapping->validate({local.recordContext(1), input}).error() == e::MappingError::INVALID_VALUES);
}

TEST_CASE("mapping and identity-preserving schema copies honor every logical bound") {
    const auto local = schema();
    const std::array<std::u16string_view, 1> key{u"A"};

    for (auto member : {&e::MappingLimits::fields, &e::MappingLimits::lookups, &e::MappingLimits::selections,
                        &e::MappingLimits::keys, &e::MappingLimits::typeAlternatives, &e::MappingLimits::nameUnits,
                        &e::MappingLimits::totalNameUnits, &e::MappingLimits::comparisonWork}) {
        auto limits = MAPPING_LIMITS;
        limits.*member = 0;
        CHECK(prepare(local, key, true, limits).error() == e::MappingError::RESOURCE_LIMIT);
    }

    for (auto member : {&c::SchemaLimits::records, &c::SchemaLimits::fields, &c::SchemaLimits::lookups,
                        &c::SchemaLimits::nameUnits, &c::SchemaLimits::totalNameUnits}) {
        auto limits = SCHEMA_LIMITS;
        limits.*member = 0;
        CHECK(local.boundedCopy(limits).error() == c::SchemaError::RESOURCE_LIMIT);
    }

    const std::array<c::LocalRecord, 1> single{{{0, u"R", false, {{8}}, {{u"A", 0}}}}};
    auto simple = c::LocalSchema::create(single, {1, 1, 1, 1, 2}, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(simple);
    const std::array requests{e::FieldRequest{key, INTS, true}};
    // One schema lookup and one request key; one type alternative plus max(1, 1)+1 comparison units.
    const e::MappingLimits exact{1, 1, 1, 1, 1, 1, 2, 3};
    CHECK(e::EventMapping::prepare(*simple, 0, requests, exact));

    for (auto member : {&e::MappingLimits::fields, &e::MappingLimits::lookups, &e::MappingLimits::selections,
                        &e::MappingLimits::keys, &e::MappingLimits::typeAlternatives, &e::MappingLimits::nameUnits,
                        &e::MappingLimits::totalNameUnits, &e::MappingLimits::comparisonWork}) {
        auto below = exact;
        --(below.*member);
        CHECK(e::EventMapping::prepare(*simple, 0, requests, below).error() == e::MappingError::RESOURCE_LIMIT);
    }

    CHECK(simple->boundedCopy({1, 1, 1, 1, 2}));
    auto empty = c::LocalSchema::create(std::array<c::LocalRecord, 1>{{{0, u"E", false, {}, {}}}}, {1, 0, 0, 1, 1});
    REQUIRE(empty);
    auto mapping = e::EventMapping::prepare(*empty, 0, {}, {});
    REQUIRE(mapping);
    CHECK(mapping->validate({empty->recordContext(0), {}}));
}

TEST_CASE("STREAM and RAW plans publish source context independently of source lifetime") {
    auto local = schema();
    const std::array<std::u16string_view, 1> key{u"A"};
    auto mapping = prepare(local, key);
    REQUIRE(mapping);
    const std::array<p::WireRecordDescription, 1> descriptions{{{7, u"Custom", {{u"A", 8}}}}};
    const p::ContextLimits contextLimits{SCHEMA_LIMITS, 4, {32, 64, 4096}};
    auto context = p::StreamDataContext::create(local, descriptions, contextLimits);
    REQUIRE(context);
    REQUIRE(context->find(7));
    CHECK(context->find(7)->context.matches(local.recordContext(1)));
    // Empty symbol marker, sender 7, and one compact-int value.
    const std::array payload{std::byte{0xfe}, std::byte{7}, std::byte{42}};
    const p::StreamDataLimits limits{64, 4, 8, 4, 16, 64, {64, 16, 16}};

    for (const auto decode : {p::decodeStreamData, p::decodeRawData}) {
        auto decoded = decode(payload, *context, limits);
        REQUIRE(decoded.status == p::StreamDataStatus::OK);
        REQUIRE(decoded.records.size() == 1);
        CHECK(mapping->validate({decoded.records[0].context, decoded.records[0].typedValues}));
        CHECK(decoded.records[0].typedValues[1].raw == 42);
        CHECK_FALSE(decoded.records[0].typedValues[2].units);
    }
}

TEST_CASE("tape rebuild preserves source identity and completed records outlive all preparation inputs") {
    auto retained = [] {
        auto local = schema();
        const std::array<std::u16string_view, 1> key{u"A"};
        auto mapping = prepare(local, key);
        REQUIRE(mapping);
        const p::TapeLimits limits{{256, 265},
                                   4096,
                                   16,
                                   {8, 4, {64, 64}, 256},
                                   {SCHEMA_LIMITS, 8, {64, 64, 4096}},
                                   100000,
                                   {256, 16, 64, 16, 64, 256, {256, 64, 64}}};
        p::TapeReader reader(local, limits);
        std::vector<p::StreamDataRecord> records;
        /** Feed one deliberately small complete frame.
         * @param payload Message ID followed by exact diagnostic bytes, fewer than 64 bytes.
         */
        auto feed = [&](std::initializer_list<unsigned char> payload) {
            REQUIRE(payload.size() < 64);
            std::vector<std::byte> bytes{static_cast<std::byte>(payload.size())};

            for (auto byte : payload) {
                bytes.push_back(static_cast<std::byte>(byte));
            }

            auto result = reader.feed(bytes);
            REQUIRE(result.status == p::TapeStatus::EVENT);
            REQUIRE(result.accepted == bytes.size());

            for (auto &record : result.records) {
                records.push_back(std::move(record));
            }
        };
        // Restricted descriptor: type=tape, opt="", no send/receive entries.
        feed({1, 'D', 'X', 'P', '3', 2, 4, 't', 'y', 'p', 'e', 4, 't', 'a', 'p', 'e', 3, 'o', 'p', 't', 0, 0, 0});
        feed({2, 7, 6, 'C', 'u', 's', 't', 'o', 'm', 1, 1, 'A', 8});
        feed({15, 0xfe, 7, 42});
        REQUIRE(records.size() == 1);
        CHECK(mapping->validate({records[0].context, records[0].typedValues}));
        // An additional unrelated sender description forces an immutable snapshot rebuild.
        feed({2, 8, 5, 'O', 't', 'h', 'e', 'r', 1, 5, 'V', 'a', 'l', 'u', 'e', 8});
        feed({15, 0xfe, 7, 43});
        CHECK(reader.finish() == p::TapeStatus::FINISHED);

        return std::pair{std::move(*mapping), std::move(records)};
    }();
    REQUIRE(retained.second.size() == 2);

    for (const auto &record : retained.second) {
        CHECK(retained.first.validate({record.context, record.typedValues}));
    }

    CHECK(retained.second[0].typedValues[1].raw == 42);
    CHECK(retained.second[1].typedValues[1].raw == 43);
    CHECK(retained.second[0].context.matches(retained.second[1].context));
}
