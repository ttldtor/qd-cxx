// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/configuration.hpp"
#include "qd/protocol/tape_reader.hpp"
#include <array>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::events;
namespace p = org::ttldtor::qdcxx::protocol;
constexpr c::SchemaLimits SCHEMA{16, 128, 256, 128, 16384};
constexpr e::MappingLimits MAPPING{128, 256, 32, 64, 128, 128, 20480, 256 * 64 * 129 + 32 * 128};
constexpr e::AttachmentEventLimits VALUES{128, 4096, 8192};

/** Independent finite tape admission for small immutable production witnesses.
 * @return Explicit framing/schema/field/payload bounds.
 */
p::TapeLimits limits() {
    p::TapeLimits value{{8192, 8201},
                        65536,
                        128,
                        {16, 8, {1024, 256}, 2048},
                        {{8, 64, 128, 128, 8192}, 32, {256, 128, 8192}},
                        100000,
                        {8192, 128, 1024, 128, 2048, 4096, {8192, 128, 128}, {1024, 256}, 4096, 1024, 1024},
                        128};
    value.data.fieldPayloadEncodedBytes = value.data.fieldPayloadBytes = value.data.payloadEncodedBytes =
        value.data.payloadWorkBytes = value.data.ownedPayloadBytes = 4096;

    return value;
}

/** Read exact original FileWriter bytes with no output normalization.
 * @param path Immutable fixture.
 * @return Owning complete bytes.
 */
std::vector<std::byte> input(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.good());
    const std::vector<char> chars((std::istreambuf_iterator<char>(file)), {});
    const auto bytes = std::as_bytes(std::span(chars));

    return {bytes.begin(), bytes.end()};
}

/** Compare exact nullable serialization with an independent Java body cell.
 * @param attachment Native owning value.
 * @param expected Original nullable/empty/hex body cell.
 */
void body(const e::SerializedAttachment &attachment, const std::string &expected) {
    CHECK(attachment.wireKind() == c::SerialFieldType::SERIAL_OBJECT);

    if (expected == "null") {
        CHECK(attachment.isNull());

        return;
    }

    REQUIRE(attachment.bytes());
    const auto bytes = *attachment.bytes();

    if (expected == "empty") {
        CHECK(bytes.empty());

        return;
    }

    REQUIRE(bytes.size() * 2 == expected.size());

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        CHECK(std::to_integer<unsigned>(bytes[i]) == std::stoul(expected.substr(i * 2, 2), nullptr, 16));
    }
}

/** Decode an actual tape with short fragments, retaining complete independent records only.
 * @param schema Exact caller schema; reader makes an independent copy with this origin.
 * @param path Original FileWriter capture.
 * @return Owning completed records after input and reader destruction.
 */
std::vector<p::StreamDataRecord> read(const c::LocalSchema &schema, const std::filesystem::path &path) {
    auto bytes = input(path);
    p::TapeReader reader(schema, limits());
    auto remaining = std::span<const std::byte>(bytes);
    std::vector<p::StreamDataRecord> saved;

    while (!remaining.empty()) {
        auto next = reader.feed(remaining.first(std::min(std::size_t{7}, remaining.size())));
        REQUIRE(next.status != p::TapeStatus::FAILED);
        REQUIRE(next.accepted > 0);
        remaining = remaining.subspan(next.accepted);
        saved.insert(saved.end(), std::make_move_iterator(next.records.begin()),
                     std::make_move_iterator(next.records.end()));
    }

    REQUIRE(reader.finish() == p::TapeStatus::FINISHED);

    return saved;
}

TEST_CASE(
    "typed Message and Configuration projections consume actual serialized Java wire witnesses without decoding") {
    const auto root = std::filesystem::path(ORG_TTLDTOR_QDCXX_MISC_FIXTURES);
    std::ifstream table(root / "oracle.tsv");
    REQUIRE(table.good());
    std::string line;
    std::size_t count = 0;

    while (std::getline(table, line)) {
        std::stringstream stream(line);
        std::vector<std::string> row;
        std::string cell;

        while (std::getline(stream, cell, '\t')) {
            row.push_back(cell);
        }

        if (row.at(0) != "event") {
            continue;
        }

        REQUIRE(row.size() == 9);
        const bool configuration = row[1].starts_with("Configuration-");
        auto schema = configuration ? e::defaultConfigurationSchema(SCHEMA) : e::defaultMessageSchema(SCHEMA);
        REQUIRE(schema);
        auto records = read(*schema, root / (row[1] + ".tape"));
        REQUIRE(records.size() == 1);
        const auto &record = records[0];
        const e::RecordValueView view{record.context, record.typedValues};

        if (configuration) {
            auto mapping = e::PreparedConfiguration::prepare(*schema, 0, MAPPING);
            REQUIRE(mapping);
            CHECK(mapping->versionField() == 0);
            auto result = mapping->map(record.symbol.units, view, VALUES, 1234);
            REQUIRE(result);
            records.clear();
            CHECK(result->symbol == u"ATTACH");
            CHECK(result->getVersion() == -7);
            CHECK(result->getEventTime() == 1234);
            body(result->getAttachment(), row[5]);
        } else {
            auto mapping = e::PreparedMessage::prepare(*schema, 0, MAPPING);
            REQUIRE(mapping);
            auto result = mapping->map(record.symbol.units, view, VALUES, 1234);
            REQUIRE(result);
            records.clear();
            CHECK(result->symbol == u"ATTACH");
            CHECK(result->getEventTime() == 1234);
            body(result->getAttachment(), row[5]);
        }

        ++count;
    }

    CHECK(count == 14);
}

TEST_CASE("misc mappings preserve explicit aliases extra-field validation context and ownership bounds") {
    const std::array declarations{
        c::LocalRecord{0,
                       u"Custom",
                       false,
                       {{249}, {104}, {10}},
                       {{u"Configuration", 0}, {u"Version", 1}, {u"Other", 2}, {u"UnselectedAlias", 0}}}};
    auto local = c::LocalSchema::create(declarations, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    auto mapping = e::PreparedConfiguration::prepare(*local, 0, MAPPING);
    REQUIRE(mapping);
    CHECK(mapping->versionField() == 1);
    std::vector<c::FieldValue> fields{{c::SerialFieldType::SERIAL_OBJECT, 0, {}, std::vector{std::byte{'A'}}},
                                      {c::SerialFieldType::LONG, std::int64_t{5} << 32},
                                      {c::SerialFieldType::UTF_CHAR_ARRAY, 0, std::u16string(u"extra")}};
    auto result = mapping->map(u"SYMBOL", {local->recordContext(0), fields}, VALUES);
    REQUIRE(result);
    CHECK(result->version == 5);
    fields[0].bytes->front() = std::byte{'X'};
    CHECK((*result->attachment.bytes())[0] == std::byte{'A'});
    CHECK(mapping->map(u"SYMBOL", {local->recordContext(0), fields}, {5, 4096, 8192}).error() ==
          e::MappingError::RESOURCE_LIMIT);
    CHECK(mapping->map(u"SYMBOL", {local->recordContext(0), fields}, {128, 0, 8192}).error() ==
          e::MappingError::RESOURCE_LIMIT);
    CHECK(mapping->map(u"SYMBOL", {local->recordContext(0), fields}, {128, 4096, 12}).error() ==
          e::MappingError::RESOURCE_LIMIT);
    auto foreign = c::LocalSchema::create(declarations, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(foreign);
    CHECK(mapping->map(u"SYMBOL", {foreign->recordContext(0), fields}, VALUES).error() ==
          e::MappingError::WRONG_CONTEXT);
    fields[2].bytes.emplace();
    CHECK(mapping->map(u"SYMBOL", {local->recordContext(0), fields}, VALUES).error() ==
          e::MappingError::INVALID_VALUES);
    const std::array absent{c::LocalRecord{0, u"Configuration", false, {{249}}, {{u"Configuration", 0}}}};
    auto noVersion = c::LocalSchema::create(absent, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(noVersion);
    auto disabled = e::PreparedConfiguration::prepare(*noVersion, 0, MAPPING);
    REQUIRE(disabled);
    CHECK_FALSE(disabled->versionField());
    const std::array nullable{c::FieldValue{c::SerialFieldType::SERIAL_OBJECT, 0}};
    auto zero = disabled->map(u"", {noVersion->recordContext(0), nullable}, {0, 0, 0});
    REQUIRE(zero);
    CHECK(zero->getVersion() == 0);
    CHECK(zero->getAttachment().isNull());
}
} // namespace
