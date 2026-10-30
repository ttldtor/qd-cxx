// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "indexed_test_support.hpp"

#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

namespace {
using namespace indexed_test;
std::filesystem::path fixtureRoot; ///< Explicit governed indexed capture path from CTest.

/** Read one unchanged original Java binary artifact.
 * @param name Fixture file name.
 * @return Owned bytes.
 */
std::vector<std::byte> fixture(std::string_view name) {
    std::ifstream input(fixtureRoot / name, std::ios::binary);
    REQUIRE(input.good());
    const std::vector<char> characters((std::istreambuf_iterator<char>(input)), {});
    const auto bytes = std::as_bytes(std::span(characters));

    return {bytes.begin(), bytes.end()};
}

/** Preserve exact ASCII source keys from the finite synthetic/standard capture.
 * @param value Captured source key.
 * @return Owned UTF-16; this fixture utility is not a schema importer.
 */
std::u16string key(std::string_view value) {
    std::u16string result;

    for (const auto unit : value) {
        REQUIRE(static_cast<unsigned char>(unit) < 128);
        result.push_back(static_cast<char16_t>(unit));
    }

    return result;
}

/** Supply explicit indexing and use actual source metadata for fields/aliases.
 * @param name Selected finite source record.
 * @param layout Caller-supplied proven logical time mapping, never inferred from wire names.
 * @return Validated manual local schema retaining all source logical fields.
 */
c::LocalSchema sourceSchema(std::string_view name, c::IndexedTimeLayout layout) {
    c::LocalRecord record{0, key(name), true, {}, {}, layout};
    std::ifstream input(fixtureRoot / "observations.tsv");
    REQUIRE(input.good());
    std::string line;

    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::vector<std::string> cells;
        std::string cell;

        while (std::getline(row, cell, '\t')) {
            cells.push_back(cell);
        }

        if (cells.size() == 5 && cells[0] == "source-map" && cells[1] == name) {
            REQUIRE(std::stoull(cells[3]) == record.fields.size());
            record.fields.push_back({std::stoi(cells[4])});
        } else if (cells.size() == 4 && cells[0] == "source-lookup" && cells[1] == name &&
                   cells[3].starts_with("field:")) {
            record.lookups.push_back({key(cells[2]), static_cast<std::size_t>(std::stoull(cells[3].substr(6)))});
        }
    }

    REQUIRE_FALSE(record.fields.empty());
    const std::array records{record};
    auto result = c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Run any explicit chunk schedule without keeping input/schema references in results.
 * @param reader Single-threaded owning reader.
 * @param bytes Borrowed full tape.
 * @param chunk Maximum feed size.
 * @param eof Whether the complete source is being closed after these bytes.
 * @return Completed owning records and explicit final status.
 */
std::pair<std::vector<p::StreamDataRecord>, p::TapeStatus>
decode(p::TapeReader &reader, std::span<const std::byte> bytes, std::size_t chunk, bool eof = true) {
    std::vector<p::StreamDataRecord> records;
    std::size_t offset = 0;

    while (offset < bytes.size()) {
        auto result = reader.feed(bytes.subspan(offset, std::min(chunk, bytes.size() - offset)));
        offset += result.accepted;

        for (auto &record : result.records) {
            records.push_back(std::move(record));
        }

        if (result.status == p::TapeStatus::FAILED) {
            return {std::move(records), result.status};
        }

        REQUIRE(result.accepted != 0);
    }

    return {std::move(records), eof ? reader.finish() : p::TapeStatus::EVENT};
}
} // namespace

TEST_CASE("explicit indexed layout validates full-width contributors and presence") {
    for (const auto first : {4, 8, 24, 40, 56, 72, 88}) {
        for (const auto second : {4, 8, 24, 40, 56, 72, 88}) {
            auto record = pairRecord();
            record.fields[1].serialTypeId = first;
            record.fields[0].serialTypeId = second;
            const std::array records{record};
            CHECK(c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1));
        }
    }

    for (const auto type : {104, 120, 152}) {
        const std::array<c::LocalRecord, 1> records{
            {{0, u"Single", true, {{8}, {type}}, {{u"Sentinel", 0}, {u"Index", 1}}, c::IndexedTimeSingleInt64{1}}}};
        CHECK(c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1));
    }

    for (const auto type : {0, 1, 2, 3, 9, 10, 104, 120, 137, 152, 233, 249}) {
        auto record = pairRecord();
        record.fields[0].serialTypeId = type;
        const std::array records{record};
        const auto result = c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE_FALSE(result);
        CHECK(result.error() == c::SchemaError::UNSUPPORTED_PROFILE);
    }

    for (const auto layout : {c::IndexedTimePairInt32{0, 0}, c::IndexedTimePairInt32{0, 3},
                              c::IndexedTimePairInt32{std::numeric_limits<std::size_t>::max(), 0}}) {
        auto record = pairRecord();
        record.indexedTimeLayout = layout;
        const std::array records{record};
        const auto result = c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE_FALSE(result);
        CHECK(result.error() == c::SchemaError::INVALID_SCHEMA);
    }

    auto record = pairRecord();
    record.indexedTimeLayout.reset();
    const std::array missing{record};
    const auto absent = c::LocalSchema::create(missing, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE_FALSE(absent);
    CHECK(absent.error() == c::SchemaError::INVALID_SCHEMA);
    record.indexedTimeLayout = c::IndexedTimePairInt32{1, 0};
    record.hasTime = false;
    const std::array ordinaryWithLayout{record};
    const auto extra =
        c::LocalSchema::create(ordinaryWithLayout, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE_FALSE(extra);
    CHECK(extra.error() == c::SchemaError::INVALID_SCHEMA);
}

TEST_CASE("old profiles refuse explicit indexing and ordinary tape modes retain their gate") {
    const std::array records{pairRecord()};

    for (const auto profile :
         {c::LocalProfile::COMPACT_INT_V1, c::LocalProfile::QUOTE_FIELDS_V1, c::LocalProfile::EXTENDED_FIELDS_V1}) {
        const auto result = c::LocalSchema::create(records, limits().context.schema, profile);
        REQUIRE_FALSE(result);
        CHECK(result.error() == c::SchemaError::UNSUPPORTED_PROFILE);
    }

    const auto local = schema();
    CHECK_THROWS_AS(p::TapeReader(local, limits(), p::TapeMode::LEGACY), std::invalid_argument);
    CHECK_THROWS_AS(p::TapeReader(local, limits(), p::TapeMode::RAW_MESSAGE_V1), std::invalid_argument);
    CHECK_THROWS_AS(p::TapeReader(limits(), p::TapeMode::BINARY_CATALOG_V1), std::invalid_argument);
    auto ordinary = pairRecord();
    ordinary.hasTime = false;
    ordinary.indexedTimeLayout.reset();
    const std::array ordinaryRecords{ordinary};
    auto oldSchema =
        c::LocalSchema::create(ordinaryRecords, limits().context.schema, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(oldSchema);
    CHECK_THROWS_AS(p::TapeReader(*oldSchema, limits(), p::TapeMode::BINARY_CATALOG_V1), std::invalid_argument);
}

TEST_CASE("owned resolved positions survive schema destruction and wire reorder with missing words") {
    std::optional<p::StreamDataContext> retained;
    c::RecordContext provenance;

    {
        const auto local = schema();
        auto reordered = description(std::numeric_limits<std::int32_t>::max());
        std::swap(reordered.fields[0], reordered.fields[1]);
        const std::array descriptions{reordered};
        auto context = p::StreamDataContext::create(local, descriptions, limits().context);
        REQUIRE(context);
        retained = std::move(*context);
        provenance = local.recordContext(0);
    }

    const auto *plan = retained->find(std::numeric_limits<std::int32_t>::max());
    REQUIRE(plan != nullptr);
    REQUIRE(plan->indexedTimeLayout);
    const auto &pair = std::get<p::ResolvedIndexedTimePairInt32>(*plan->indexedTimeLayout);
    CHECK(pair.firstValueIndex == 1);
    CHECK(pair.secondValueIndex == 0);
    CHECK(plan->binding.fields[0].targetFieldId == 0);
    CHECK(plan->binding.fields[1].targetFieldId == 1);
    CHECK(plan->context.matches(provenance));

    const auto local = schema();
    auto missing = description();
    missing.fields.erase(missing.fields.begin());
    const std::array missingDescription{missing};
    auto context = p::StreamDataContext::create(local, missingDescription, limits().context);
    REQUIRE(context);
    std::vector<std::byte> body{std::byte{0xfe}};
    compact(body, 0);
    compact(body, -1985229329);
    compact(body, 913);
    const auto output = p::decodeStreamData(body, *context, limits().data);
    REQUIRE(output.status == p::StreamDataStatus::OK);
    REQUIRE(output.records.size() == 1);
    CHECK(output.records[0].typedValues[0].raw == -1985229329);
    CHECK(output.records[0].typedValues[1].raw == 0);
    CHECK(output.records[0].typedValues[2].raw == 913);
    const auto bits = static_cast<std::uint64_t>(static_cast<std::uint32_t>(output.records[0].typedValues[1].raw))
                          << 32 |
                      static_cast<std::uint32_t>(output.records[0].typedValues[0].raw);
    CHECK(std::bit_cast<std::int64_t>(bits) == 2309737967LL);
}

TEST_CASE("production indexed FileWriter fixtures preserve every time bit and actual standard schema") {
    const std::array<std::int64_t, 5> times{std::numeric_limits<std::int64_t>::min(), -1, 0, 0x1234567889abcdefLL,
                                            std::numeric_limits<std::int64_t>::max()};

    struct Case {
        std::string_view name;       ///< Exact finite source selection.
        c::IndexedTimeLayout layout; ///< Explicit source proof, independent of wire text.
    };

    const std::array<Case, 11> cases{{{"Pair4", c::IndexedTimePairInt32{0, 1}},
                                      {"Pair8", c::IndexedTimePairInt32{0, 1}},
                                      {"Pair24", c::IndexedTimePairInt32{0, 1}},
                                      {"Pair40", c::IndexedTimePairInt32{0, 1}},
                                      {"Pair56", c::IndexedTimePairInt32{0, 1}},
                                      {"Pair72", c::IndexedTimePairInt32{0, 1}},
                                      {"Pair88", c::IndexedTimePairInt32{0, 1}},
                                      {"Single104", c::IndexedTimeSingleInt64{0}},
                                      {"Single120", c::IndexedTimeSingleInt64{0}},
                                      {"Single152", c::IndexedTimeSingleInt64{0}},
                                      {"TimeAndSale", c::IndexedTimePairInt32{0, 1}}}};

    for (const auto &selected : cases) {
        CAPTURE(selected.name);
        const auto local = sourceSchema(selected.name, selected.layout);
        const auto bytes = fixture(std::string(selected.name) + ".qtp");

        for (const auto chunk : {std::size_t{1}, std::size_t{7}, std::size_t{65536}}) {
            CAPTURE(chunk);
            std::vector<p::StreamDataRecord> retained;

            {
                p::TapeReader reader(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
                auto [records, status] = decode(reader, bytes, chunk);
                REQUIRE(status == p::TapeStatus::FINISHED);
                REQUIRE(records.size() == times.size());
                REQUIRE_FALSE(reader.descriptions().empty());
                const auto *plan = reader.context()->find(records[0].senderId);
                REQUIRE(plan != nullptr);
                REQUIRE(plan->indexedTimeLayout);

                for (std::size_t index = 0; index < records.size(); ++index) {
                    const auto &record = records[index];
                    CHECK(record.context.matches(reader.schema().recordContext(0)));
                    CHECK(record.typedValues.size() == local.records()[0].fields.size());

                    if (const auto *pair = std::get_if<p::ResolvedIndexedTimePairInt32>(&*plan->indexedTimeLayout)) {
                        const auto high = record.typedValues[pair->firstValueIndex].raw;
                        const auto low = record.typedValues[pair->secondValueIndex].raw;
                        const auto bits = static_cast<std::uint64_t>(static_cast<std::uint32_t>(high)) << 32 |
                                          static_cast<std::uint32_t>(low);
                        CHECK(std::bit_cast<std::int64_t>(bits) == times[index]);
                    } else {
                        const auto value =
                            std::get<p::ResolvedIndexedTimeSingleInt64>(*plan->indexedTimeLayout).valueIndex;
                        CHECK(record.typedValues[value].raw == times[index]);
                    }

                    if (selected.name != "TimeAndSale") {
                        CHECK(record.typedValues.back().raw == 913);
                    } else {
                        CHECK(record.typedValues.size() == 10);

                        for (std::size_t field = 2; field < record.typedValues.size(); ++field) {
                            CHECK(record.typedValues[field].raw == 0);
                        }
                    }
                }

                retained = std::move(records);
            }

            CHECK(retained.back().context.valid());
            CHECK(retained.back().typedValues[0].raw != 0);
        }
    }
}

TEST_CASE("production same-message sender replacements retain exact policy and atomic failure") {
    const auto source = sourceSchema("Pair8", c::IndexedTimePairInt32{0, 1});
    std::vector<c::LocalRecord> records(source.records().begin(), source.records().end());
    records[0].lookups.push_back({u"HighAlias", 0});
    records.push_back({1,
                       u"Other",
                       true,
                       {{8}, {8}, {8}},
                       {{u"High", 0}, {u"Low", 1}, {u"Sentinel", 2}},
                       c::IndexedTimePairInt32{0, 1}});
    auto local = c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);

    for (const auto name :
         {"reordered-alias.qtp", "missing-high.qtp", "batch-subset-last.qtp", "batch-renamed-exact-last.qtp"}) {
        CAPTURE(name);
        p::TapeReader reader(*local, limits(), p::TapeMode::BINARY_CATALOG_V1);
        accept(reader, header());
        auto [output, status] = decode(reader, fixture(name), 7);
        REQUIRE(status == p::TapeStatus::FINISHED);
        REQUIRE(output.size() == 1);
        CHECK(output[0].typedValues[2].raw == 913);
        CHECK(output[0].typedValues[1].raw == -1985229329);
        const auto missing =
            std::string_view(name) == "missing-high.qtp" || std::string_view(name) == "batch-subset-last.qtp";
        CHECK(output[0].typedValues[0].raw == (missing ? 0 : 305419896));
        CHECK(output[0].localId == (std::string_view(name) == "batch-renamed-exact-last.qtp" ? 1 : 0));
    }

    for (const auto name : {"batch-retyped-last.qtp", "batch-renamed-last.qtp"}) {
        CAPTURE(name);
        p::TapeReader reader(*local, limits(), p::TapeMode::BINARY_CATALOG_V1);
        accept(reader, header());
        const auto identity = reader.schema().recordContext(0);
        auto [output, status] = decode(reader, fixture(name), 65536);
        CHECK(status == p::TapeStatus::FAILED);
        CHECK(reader.diagnostic().error == p::TapeError::UNSUPPORTED);
        CHECK(output.empty());
        CHECK(reader.descriptions().empty());
        CHECK(reader.schema().recordContext(0).matches(identity));
    }

    for (const auto name : {"batch-late-invalid.qtp", "batch-middle-invalid.qtp"}) {
        p::TapeReader reader(*local, limits(), p::TapeMode::BINARY_CATALOG_V1);
        accept(reader, header());
        auto [oldOutput, initialStatus] = decode(reader, fixture("batch-late-invalid-prelude.qtp"), 1, false);
        REQUIRE(initialStatus == p::TapeStatus::EVENT);
        REQUIRE(oldOutput.size() == 1);
        const auto *oldDescriptions = reader.descriptions().data();
        const auto *oldPlan = reader.context()->find(0);
        const auto identity = reader.schema().recordContext(0);
        auto [failedOutput, failedStatus] = decode(reader, fixture(name), 1);
        CHECK(failedStatus == p::TapeStatus::FAILED);
        CHECK(reader.diagnostic().error == p::TapeError::MALFORMED);
        CHECK(failedOutput.empty());
        CHECK(reader.descriptions().data() == oldDescriptions);
        CHECK(reader.context()->find(0) == oldPlan);
        CHECK(reader.schema().recordContext(0).matches(identity));
        CHECK(oldOutput[0].context.matches(identity));
    }
}

/** Execute core checks or governed production-fixture checks with explicit paths.
 * @param argc Argument count.
 * @param argv Capture directory followed by optional doctest filters.
 * @return Nonzero on missing arguments or failed assertions.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }

    fixtureRoot = argv[1];
    doctest::Context context;
    context.applyCommandLine(argc - 1, argv + 1);

    return context.run();
}
