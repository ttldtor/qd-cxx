// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/order_index.hpp"

#include <array>
#include <doctest/doctest.h>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::events;
constexpr c::SchemaLimits SCHEMA_LIMITS{16, 128, 256, 128, 16384}; ///< Complete finite test declarations.
constexpr e::MappingLimits MAPPING_LIMITS{128, 256, 16, 64, 32, 128, 16384, 100000}; ///< Preparation credits.

/** @param text Oracle UTF-16 unit hex, including an empty string.
 * @return Independently owned decoded record/key name.
 */
std::u16string decode(std::string_view text) {
    REQUIRE(text.size() % 4 == 0);
    std::u16string result;

    for (std::size_t offset = 0; offset < text.size(); offset += 4) {
        result.push_back(static_cast<char16_t>(std::stoul(std::string(text.substr(offset, 4)), nullptr, 16)));
    }

    return result;
}

/** @param record Complete independently owned local declaration.
 * @return Validated immutable schema using the full binary logical type catalog, without transport.
 */
c::LocalSchema schema(c::LocalRecord record) {
    record.id = 0;
    auto result =
        c::LocalSchema::create(std::vector{std::move(record)}, SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** @param name Exact record name; suffix interpretation belongs to preparation.
 * @return Owned record with required Index and unrelated nullable text.
 */
c::LocalRecord localRecord(std::u16string name = u"Order#NTV") {
    return {0, std::move(name), false, {{8}, {10}}, {{u"Index", 0}, {u"Note", 1}}};
}

/** @param local Source declarations, retained only during this call.
 * @param wireIndex Signed32 wire Index, including negative bit patterns.
 * @return Complete generic values with neutral payloads in unselected fields.
 */
std::vector<c::FieldValue> recordValues(const c::LocalSchema &local, std::int32_t wireIndex) {
    std::vector<c::FieldValue> values;

    for (const auto &field : local.records()[0].fields) {
        values.push_back({static_cast<c::SerialFieldType>(field.serialTypeId), 0});
    }

    for (const auto &lookup : local.records()[0].lookups) {
        if (lookup.name == u"Index") {
            values[lookup.fieldId].raw = wireIndex;
        }
    }

    return values;
}
} // namespace

TEST_CASE("actual Java mapping delegate and order-index observations") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_ORDER_INDEX_FIXTURE);
    REQUIRE(input.is_open());
    std::map<std::u16string, c::LocalRecord> records;
    std::string line;
    std::size_t rows = 0;
    std::size_t delegates = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream stream(line);
        std::vector<std::string> cells;
        std::string cell;

        while (std::getline(stream, cell, '\t')) {
            cells.push_back(cell);
        }

        REQUIRE(cells.size() >= 3);
        INFO(line);
        ++rows;

        if (cells[0] == "SPECIAL") {
            CHECK(e::isSpecialOrderSourceId(static_cast<std::int32_t>(std::stol(cells[1]))) == (cells[2] == "true"));
        } else if (cells[0] == "GET") {
            const auto index = std::stoll(cells[1]);
            CHECK(e::orderIndexSourceId(index) == std::stol(cells[2]));
            CHECK(e::orderIndexSource(index).id() == std::stol(cells[2]));
        } else if (cells[0] == "GET_INVALID") {
            CHECK_THROWS_AS(static_cast<void>(e::orderIndexSource(std::stoll(cells[1]))), std::invalid_argument);
        } else if (cells[0] == "CHANGE") {
            REQUIRE(cells.size() == 4);
            const auto source = e::OrderSource::valueOf(static_cast<std::int32_t>(std::stol(cells[2])));
            CHECK(e::changeOrderIndexSource(std::stoll(cells[1]), source) == std::stoll(cells[3]));
        } else if (cells[0] == "FIELD") {
            REQUIRE(cells.size() == 4);
            const auto name = decode(cells[1]);
            auto &record = records[name];
            record.name = name;
            record.lookups.push_back({decode(cells[2]), record.fields.size()});
            record.fields.push_back({static_cast<std::int32_t>(std::stol(cells[3]))});
        } else if (cells[0] == "RECORD") {
            auto local = schema(records.at(decode(cells[1])));
            const auto prepared = e::PreparedOrderSource::prepare(local, 0, MAPPING_LIMITS);
            REQUIRE(prepared);
            CHECK(prepared->source().id() == std::stol(cells[2]));
        } else if (cells[0] == "RECORD_INVALID") {
            auto local = schema(localRecord(decode(cells[1])));
            const auto prepared = e::PreparedOrderSource::prepare(local, 0, MAPPING_LIMITS);
            REQUIRE_FALSE(prepared);
            CHECK(prepared.error() == e::MappingError::INVALID_INPUT);
        } else if (cells[0] == "DELEGATE") {
            REQUIRE(cells.size() == 5);
            auto local = schema(records.at(decode(cells[1])));
            const auto prepared = e::PreparedOrderSource::prepare(local, 0, MAPPING_LIMITS);
            REQUIRE(prepared);
            const auto values = recordValues(local, static_cast<std::int32_t>(std::stol(cells[2])));
            const auto actual = prepared->mapIndex({local.recordContext(0), values});
            REQUIRE(actual);
            CHECK(*actual == std::stoll(cells[3]));
            CHECK(e::orderIndexSourceId(*actual) == std::stol(cells[4]));
            std::vector<c::FieldValueView> borrowed;

            for (const auto &value : values) {
                borrowed.push_back({value.type, value.raw, {}, {}});
            }

            CHECK(prepared->mapIndexView({local.recordContext(0), borrowed}) == actual);
            ++delegates;
        } else if (cells[0] == "PHYSICAL_INT") {
            // Java accepts physical int slots under other full identities; this explicit profile does not mask them.
            auto record = localRecord();
            record.fields[0].serialTypeId = static_cast<std::int32_t>(std::stol(cells[1]));
            auto local = schema(std::move(record));
            const auto prepared = e::PreparedOrderSource::prepare(local, 0, MAPPING_LIMITS);

            if (cells[1] == "8") {
                REQUIRE(prepared);
            } else {
                REQUIRE_FALSE(prepared);
                CHECK(prepared.error() == e::MappingError::INCOMPATIBLE_TYPE);
            }
        } else {
            CHECK((cells[0] == "SET_INDEX" || cells[0] == "SET_INDEX_INVALID" || cells[0] == "PHYSICAL_INT_INVALID" ||
                   cells[0] == "DELEGATE_UNAVAILABLE"));
        }
    }

    CHECK(rows >= 300);
    CHECK(delegates >= 30);
}

TEST_CASE("preparation owns source and context without symbol exchange or endpoint identity") {
    auto prepared = [] {
        auto local = schema(localRecord(u"Caller&Q#NTV"));

        return e::PreparedOrderSource::prepare(local, 0, MAPPING_LIMITS);
    }();
    REQUIRE(prepared);
    CHECK(prepared->source() == e::OrderSource::NTV);
    const auto context = prepared->context();
    const std::array values{c::FieldValue{c::SerialFieldType::COMPACT_INT, -1},
                            c::FieldValue{c::SerialFieldType::UTF_CHAR_ARRAY, 0, u"owned elsewhere"}};
    const auto index = prepared->mapIndex({context, values});
    REQUIRE(index);
    CHECK(e::orderIndexSourceId(*index) == e::OrderSource::NTV.id());
    CHECK(static_cast<std::uint32_t>(*index) == 0xffffffffU);
    auto copy = *prepared;
    CHECK(copy.context().matches(context));
    CHECK(copy.source() == prepared->source());
    auto foreign = schema(localRecord(u"Caller&Q#NTV"));
    CHECK(copy.mapIndex({foreign.recordContext(0), values}).error() == e::MappingError::WRONG_CONTEXT);
    CHECK(copy.mapIndex({{}, {}}).error() == e::MappingError::WRONG_CONTEXT);
    CHECK(copy.mapIndex({context, std::span(values).first(1)}).error() == e::MappingError::INVALID_VALUES);
    auto malformed = values;
    malformed[1].raw = 1;
    CHECK(copy.mapIndex({context, malformed}).error() == e::MappingError::INVALID_VALUES);
    malformed = values;
    malformed[0].raw = 2147483648LL;
    CHECK(copy.mapIndex({context, malformed}).error() == e::MappingError::INVALID_VALUES);
    const std::array borrowed{c::FieldValueView{c::SerialFieldType::COMPACT_INT, -1, {}, {}},
                              c::FieldValueView{c::SerialFieldType::UTF_CHAR_ARRAY, 0, u"borrowed", {}}};
    CHECK(copy.mapIndexView({context, borrowed}) == index);
}

TEST_CASE("record name field and work bounds refuse atomically at exact boundaries") {
    auto local = schema(localRecord());
    constexpr e::MappingLimits EXACT_LIMITS{2, 2, 1, 1, 1, 9, 23, 22}; ///< Record + two lookups + one Index key.
    REQUIRE(e::PreparedOrderSource::prepare(local, 0, EXACT_LIMITS));

    for (unsigned coordinate = 0; coordinate < 8; ++coordinate) {
        auto limits = EXACT_LIMITS;
        const std::array<std::size_t *, 8> counters{
            &limits.fields,           &limits.lookups,   &limits.selections,     &limits.keys,
            &limits.typeAlternatives, &limits.nameUnits, &limits.totalNameUnits, &limits.comparisonWork};
        --*counters[coordinate];
        const auto refused = e::PreparedOrderSource::prepare(local, 0, limits);
        REQUIRE_FALSE(refused);
        CHECK(refused.error() == e::MappingError::RESOURCE_LIMIT);
    }

    CHECK(e::PreparedOrderSource::prepare(local, -1, MAPPING_LIMITS).error() == e::MappingError::INVALID_INPUT);
    CHECK(e::PreparedOrderSource::prepare(local, 1, MAPPING_LIMITS).error() == e::MappingError::INVALID_INPUT);
    auto noIndex = localRecord();
    noIndex.lookups[0].name = u"Other";
    auto missing = schema(std::move(noIndex));
    CHECK(e::PreparedOrderSource::prepare(missing, 0, MAPPING_LIMITS).error() == e::MappingError::MISSING_REQUIRED);
    auto wrongType = localRecord();
    wrongType.fields[0].serialTypeId = 72;
    auto incompatible = schema(std::move(wrongType));
    CHECK(e::PreparedOrderSource::prepare(incompatible, 0, MAPPING_LIMITS).error() ==
          e::MappingError::INCOMPATIBLE_TYPE);
}
