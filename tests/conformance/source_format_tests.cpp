// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "../../tools/qtp-inspect/formatter.hpp"
#include "../../tools/tape-inspect/source_format.hpp"
#include "indexed_test_support.hpp"
#include <algorithm>
#include <array>
#include <doctest/doctest.h>
#include <fstream>
#include <limits>
#include <sstream>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace f = org::ttldtor::qdcxx::inspect;
namespace s = org::ttldtor::qdcxx::tape_inspect;
constexpr std::size_t BUDGET = 65536;
std::array<std::filesystem::path, 2> fixtureRoots; ///< Explicit immutable baseline/current producer captures.

/** Split all TSV positions without treating empty typed-value cells as missing. */
std::vector<std::string> cells(std::string text) {
    REQUIRE(!text.empty());
    REQUIRE(text.back() == '\n');
    text.pop_back();
    std::vector<std::string> result;
    std::istringstream input(text);
    std::string value;

    while (std::getline(input, value, '\t')) {
        result.push_back(value);
    }

    return result;
}

/** Own an authoritative nineteen-identity schema and a matching record. */
struct Sample {
    c::LocalSchema schema;
    p::StreamDataRecord record;
};

/** Construct exact scalars, nulls, empty payloads and unsafe UTF-16 without parsing a fixture. */
Sample sample() {
    const std::array<std::int32_t, 19> types{0, 1, 2, 3, 4, 8, 9, 10, 24, 40, 56, 72, 88, 104, 120, 152, 137, 233, 249};
    c::LocalRecord declaration{0, u"Record\t\n", false, {}, {}};
    p::StreamDataRecord record{};
    record.localId = 0;
    record.senderId = 17;
    record.symbol.units = std::u16string{u'A', u'\t', u'\n', char16_t{0xd800}, char16_t{0x202e}};
    record.removalTime = 0;

    for (std::size_t i = 0; i < types.size(); ++i) {
        declaration.fields.push_back({types[i]});
        declaration.lookups.push_back({u"Field" + std::u16string(1, static_cast<char16_t>(u'A' + i)), i});
        record.typedValues.push_back({static_cast<c::SerialFieldType>(types[i]), 0});
    }

    record.typedValues[7].units = std::u16string{u'\t', u'\n', char16_t{0xd800}};
    record.typedValues[10].raw = 700;
    record.typedValues[16].units = u"";
    record.typedValues[17].bytes = std::vector<std::byte>{std::byte{0}, std::byte{0xff}};
    record.typedValues[18].bytes = std::vector<std::byte>{};
    auto schema =
        c::LocalSchema::create(std::span(&declaration, 1), {1, 19, 19, 64, 1024}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    record.context = schema->recordContext(0);

    return {std::move(*schema), std::move(record)};
}

/** Check literal zero, below-exact, exact and above-exact row allowances. */
template <typename Compose> void limits(Compose compose) {
    const auto expected = compose(BUDGET);
    REQUIRE(!expected.empty());
    CHECK_THROWS_AS(compose(0), f::OutputLimit);
    CHECK_THROWS_AS(compose(expected.size() - 1), f::OutputLimit);
    CHECK(compose(expected.size()) == expected);
    CHECK(compose(expected.size() + 1) == expected);
}
} // namespace

TEST_CASE("source output preserves every typed identity and exact 27 positions") {
    auto value = sample();
    const f::Formatter formatter(f::OutputFormat::TSV, f::DisplayProfile::BINARY_CATALOG_V1);
    const s::SourceRecordTiming timing{s::SourceTimePolicy::FIELD, 7};
    value.record.fieldTime = p::FieldEventTime{4294967295LL, 4294967295LL, 19};
    const auto original = value.record.typedValues;
    const auto row = formatter.sourceCatalogRecord(value.record, value.schema, 2, 3, 15, 1, BUDGET, timing);
    const auto parts = cells(row);
    REQUIRE(parts.size() == 27);
    CHECK(parts[0] == "wire-data-v9");
    CHECK(parts[1] == "7");
    CHECK(parts[2] == "2");
    CHECK(parts[3] == "3");
    CHECK(parts[4] == "15");
    CHECK(parts[5] == "1");
    CHECK(parts[6] == "17");
    CHECK(parts[7] == "0");
    CHECK(parts[8] == "005200650063006f007200640009000a");
    CHECK(parts[9] == "00410009000ad800202e");
    CHECK(parts[10].find("9:0:-:null") != std::string::npos);
    CHECK(parts[10].find("10:0:0009000ad800:-") != std::string::npos);
    CHECK(parts[10].find("137:0:empty:-") != std::string::npos);
    CHECK(parts[10].find("233:0:-:00ff") != std::string::npos);
    CHECK(parts[10].find("249:0:-:empty") != std::string::npos);
    CHECK(parts[11] == "-");
    CHECK(parts[12] == "0");
    CHECK(parts[15] == "0");
    CHECK(parts[16] == "1");
    CHECK(parts[17] == "4294967295");
    CHECK(parts[18] == "4294967295");
    CHECK(parts[19] == "19");
    CHECK(parts[20] == "-1");
    CHECK(parts[21] == "23");
    CHECK(parts[22] == "field");
    CHECK(parts[23] == "none");
    CHECK(parts[24] == "field");
    CHECK(parts[25] == "-1");
    CHECK(parts[26] == "23");
    CHECK(value.record.typedValues == original);
    CHECK(value.record.fieldTime->sourceOrdinal == 19);
    CHECK_THROWS_AS(formatter.catalogRecord(value.record, value.schema, 2, 3, BUDGET, nullptr, 1),
                    std::invalid_argument);
    value.record.fieldTime.reset();
    const auto old = cells(formatter.catalogRecord(value.record, value.schema, 2, 3, BUDGET, nullptr, 1));
    CHECK(parts[10] == old[8]);
    limits([&](std::size_t budget) {
        return formatter.sourceCatalogRecord(value.record, value.schema, 2, 3, 15, 1, budget, timing);
    });
}

TEST_CASE("raw FIELD absence sequence absence and assigned external zero stay distinct") {
    auto value = sample();
    const f::Formatter formatter(f::OutputFormat::TSV, f::DisplayProfile::BINARY_CATALOG_V1);
    s::SourceRecordTiming timing{s::SourceTimePolicy::MESSAGE, 0};
    auto parts = cells(formatter.sourceCatalogRecord(value.record, value.schema, 0, 0, 0, 1, BUDGET, timing));
    REQUIRE(parts.size() == 27);
    CHECK(parts[16] == "0");

    for (std::size_t i = 17; i <= 21; ++i) {
        CHECK(parts[i] == "-");
    }

    CHECK(parts[24] == "zero");
    value.record.fieldTime = p::FieldEventTime{0, std::nullopt, 9};
    timing.external = p::TapeTiming{0, 0};
    timing.externalOrigin = s::ExternalTimeOrigin::MESSAGE;
    parts = cells(formatter.sourceCatalogRecord(value.record, value.schema, 0, 0, 0, 1, BUDGET, timing));
    CHECK(parts[11] == "0");
    CHECK(parts[16] == "1");
    CHECK(parts[17] == "0");
    CHECK(parts[18] == "-");
    CHECK(parts[23] == "message");
    CHECK(parts[24] == "field");
    CHECK(parts[25] == "0");
    value.record.fieldTime->rawSequence = 0;
    timing.external = p::TapeTiming{-1000, -4294967296LL};
    timing.externalOrigin = s::ExternalTimeOrigin::SIDECAR;
    parts = cells(formatter.sourceCatalogRecord(value.record, value.schema, 0, 0, 0, 1, BUDGET, timing));
    CHECK(parts[18] == "0");
    CHECK(parts[24] == "external");
    CHECK(parts[25] == "-4294967296");
    CHECK(parts[26] == "-1000");
    timing.policy = s::SourceTimePolicy::NONE;
    parts = cells(formatter.sourceCatalogRecord(value.record, value.schema, 0, 0, 0, 1, BUDGET, timing));
    CHECK(parts[11] == "-1000");
    CHECK(parts[24] == "zero");
    CHECK(parts[25] == "0");
}

TEST_CASE("source records validate authoritative borrowed schema before budget") {
    auto value = sample();
    const auto other = sample();
    const f::Formatter formatter(f::OutputFormat::TSV, f::DisplayProfile::BINARY_CATALOG_V1);
    s::SourceRecordTiming timing;
    CHECK_THROWS_AS(formatter.sourceCatalogRecord(value.record, other.schema, 0, 0, 0, 1, 0, timing),
                    std::invalid_argument);
    value.record.typedValues[0].raw = 1;
    CHECK_THROWS_AS(formatter.sourceCatalogRecord(value.record, value.schema, 0, 0, 0, 1, 0, timing),
                    std::invalid_argument);
    value.record.typedValues[0].raw = 0;
    timing.externalOrigin = s::ExternalTimeOrigin::MESSAGE;
    CHECK_THROWS_AS(formatter.sourceCatalogRecord(value.record, value.schema, 0, 0, 0, 1, 0, timing),
                    std::invalid_argument);
    timing.external = p::TapeTiming{};
    CHECK_THROWS_AS(formatter.sourceCatalogRecord(value.record, value.schema, 0, 0, 0, 1, 0, timing),
                    std::invalid_argument);
}

TEST_CASE("repeated source occurrences preserve segment-local schema and FIELD identity") {
    auto value = sample();
    value.record.fieldTime = p::FieldEventTime{1, 0, 5};
    const f::Formatter formatter(f::OutputFormat::TSV, f::DisplayProfile::BINARY_CATALOG_V1);

    for (std::size_t ordinal = 0; ordinal < 2; ++ordinal) {
        const s::SourceRecordTiming timing{s::SourceTimePolicy::FIELD, ordinal};
        const auto parts =
            cells(formatter.sourceCatalogRecord(value.record, value.schema, 0, 0, ordinal, 1, BUDGET, timing));
        CHECK(parts[1] == std::to_string(ordinal));
        CHECK(parts[2] == "0");
        CHECK(parts[3] == "0");
        CHECK(parts[4] == std::to_string(ordinal));
        CHECK(parts[5] == "1");
        CHECK(parts[6] == "17");
        CHECK(parts[7] == "0");
        CHECK(parts[19] == "5");
    }
}

TEST_CASE("HUMAN source records escape unsafe units and retain all metadata") {
    const auto value = sample();
    const f::Formatter formatter(f::OutputFormat::HUMAN, f::DisplayProfile::BINARY_CATALOG_V1);
    const auto row = formatter.sourceCatalogRecord(value.record, value.schema, 0, 0, 0, 1, BUDGET, {});
    CHECK(row.find("\\t") != std::string::npos);
    CHECK(row.find("\\n") != std::string::npos);
    CHECK(row.find("\\uD800") != std::string::npos);
    CHECK(row.find("\\u202E") != std::string::npos);
    CHECK(row.find('\t') == std::string::npos);
    CHECK(std::count(row.begin(), row.end(), '\n') == 1);
}

TEST_CASE("source rows have exact grammar and safe native path identity") {
    CHECK(s::sourceHeader(BUDGET) == "format\tqdcxx-tape-inspect\t9\tbinary-catalog-v1\tself-contained-segments-v1\n");
    CHECK(s::sourcePolicy(s::SourceTimePolicy::TEXT, s::ContainerKind::GZIP, s::ContainerKind::ZIP, BUDGET) ==
          "source-policy\ttext\tgzip\tzip\n");
    const std::filesystem::path path = std::filesystem::path::string_type{'A', '\t', '\n'};
    const auto identity = cells(s::sourceIdentity(3, path, nullptr, BUDGET));
    REQUIRE(identity.size() == 5);
    CHECK(identity[0] == "source");
    CHECK(identity[1] == "3");
    CHECK(identity[2] == (sizeof(std::filesystem::path::value_type) == 2 ? "utf16le" : "bytes"));
    CHECK(identity[3] == (sizeof(std::filesystem::path::value_type) == 2 ? "410009000a00" : "41090a"));
    CHECK(identity[4] == "-");
    CHECK(s::sourceIdentityBytes(3, path, nullptr) == s::sourceIdentity(3, path, nullptr, BUDGET).size());
    CHECK(s::sourceIdentityBytes(std::numeric_limits<std::size_t>::max(), path, &path) ==
          s::sourceIdentity(std::numeric_limits<std::size_t>::max(), path, &path, BUDGET).size());
    CHECK(cells(s::sourceEnd({3, 1, 2, 3, 4, 5, 6, 7}, BUDGET)).size() == 9);
    CHECK(cells(s::sourceComplete({1, 2, 3, 4, 5, 6}, BUDGET)).size() == 7);
    CHECK(cells(s::sourceResource("data", 3, "physicalFetched", 4, std::nullopt, BUDGET)).size() == 6);
    limits(s::sourceHeader);
    limits([&](std::size_t budget) {
        return s::sourceIdentity(3, path, &path, budget);
    });
    limits([](std::size_t budget) {
        return s::sourcePolicy(s::SourceTimePolicy::NONE, s::ContainerKind::NONE, std::nullopt, budget);
    });
    limits([](std::size_t budget) {
        return s::sourceBegin(3, budget);
    });
    limits([](std::size_t budget) {
        return s::sourceEnd({}, budget);
    });
    limits([](std::size_t budget) {
        return s::sourceComplete({}, budget);
    });
    limits([](std::size_t budget) {
        return s::sourceResource("sequence", std::nullopt, "accepted", 0, 0, budget);
    });
}

TEST_CASE("diagnostics have exact thirteen cells and unknown coordinates remain absent") {
    const s::SourceDiagnostic diagnostic{3, "read", "io", 7, "data", 10, std::nullopt, 12, 9, 0};
    const auto parts = cells(s::sourceDiagnostic(diagnostic, false, BUDGET));
    REQUIRE(parts.size() == 13);
    CHECK(parts[0] == "incomplete-v3");
    CHECK(parts[1] == "3");
    CHECK(parts[4] == "7");
    CHECK(parts[6] == "10");
    CHECK(parts[7] == "-");
    CHECK(parts[8] == "12");
    CHECK(parts[9] == "9");
    CHECK(parts[10] == "0");
    CHECK(parts[11] == "-");
    CHECK(parts[12] == "-");
    CHECK(cells(s::sourceDiagnostic(diagnostic, true, BUDGET))[0] == "cleanup-v3");
    limits([&](std::size_t budget) {
        return s::sourceDiagnostic(diagnostic, false, budget);
    });
    auto invalid = diagnostic;
    invalid.staticReason = "raw\tinput";
    CHECK_THROWS_AS(s::sourceDiagnostic(invalid, false, 0), std::invalid_argument);
    CHECK_THROWS_AS(s::sourceResource("input", 0, "unknown", 0, 0, 0), std::invalid_argument);
}

TEST_CASE("terminal reserve equals maximum builders and rejects arithmetic overflow") {
    constexpr auto MAX = std::numeric_limits<std::uint64_t>::max();
    constexpr auto SIZE_MAXIMUM = std::numeric_limits<std::size_t>::max();
    constexpr std::string_view TOKEN = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    const s::SequenceSummary summary{MAX, MAX, MAX, MAX, MAX, MAX};
    const s::SourceDiagnostic diagnostic{
        std::numeric_limits<int>::max(), TOKEN, TOKEN, SIZE_MAXIMUM, "sequence", MAX, MAX, MAX, MAX, MAX, MAX, MAX};
    const auto marker = s::sourceComplete(summary, BUDGET);
    const auto resource = s::sourceResource("sequence", SIZE_MAXIMUM, TOKEN, MAX, MAX, BUDGET);
    const auto error = s::sourceDiagnostic(diagnostic, false, BUDGET);
    CHECK(s::terminalReserveWorstCase(0, 0) == marker.size());
    CHECK(s::terminalReserveWorstCase(92, 4) == marker.size() + 92 * resource.size() + 4 * error.size());
    CHECK_FALSE(s::terminalReserveWorstCase(SIZE_MAXIMUM, 0));
    CHECK_FALSE(s::terminalReserveWorstCase(0, SIZE_MAXIMUM));
}

TEST_CASE("source descriptions preserve actual FIELD prefixes from both production captures") {
    const f::Formatter formatter(f::OutputFormat::TSV, f::DisplayProfile::BINARY_CATALOG_V1);

    for (const auto &root : fixtureRoots) {
        for (const auto name : {"pair.qtp", "first-only.qtp", "standard-indexed.qtp"}) {
            CAPTURE(root.string());
            CAPTURE(name);
            std::ifstream input(root / name, std::ios::binary);
            REQUIRE(input);
            const std::vector<char> bytes{std::istreambuf_iterator<char>(input), {}};
            auto remaining = std::as_bytes(std::span(bytes));
            auto limits = indexed_test::limits();
            limits.data.ownedFieldTimeRecords = 100;
            p::TapeReader reader(limits, p::TapeDiscovery::BINARY_CATALOG_V1, p::FieldTimePolicy::READ);
            std::size_t descriptions = 0;

            while (!remaining.empty()) {
                const auto result = reader.feed(remaining);
                REQUIRE(result.status != p::TapeStatus::FAILED);
                REQUIRE(result.accepted > 0);
                remaining = remaining.subspan(result.accepted);

                if (!result.frame || result.frame->type != 2) {
                    continue;
                }

                REQUIRE(reader.context());

                for (const auto &description : reader.descriptions()) {
                    const auto *plan = reader.context()->find(description.senderId);
                    REQUIRE(plan);
                    REQUIRE(plan->eventTimeFieldCount > 0);
                    const auto rows = formatter.sourceWireDescription(1, descriptions++, description, *plan, BUDGET);
                    CHECK_THROWS_AS(formatter.wireDescription(1, 0, description, *plan, BUDGET), std::invalid_argument);
                    std::istringstream lines(rows);
                    std::string line;
                    REQUIRE(static_cast<bool>(std::getline(lines, line)));
                    const auto recordCells = cells(line + "\n");
                    REQUIRE(recordCells.size() == 7);
                    CHECK(recordCells[0] == "wire-record");
                    CHECK(recordCells[6] == std::to_string(description.fields.size()));

                    for (std::size_t field = 0; field < description.fields.size(); ++field) {
                        REQUIRE(static_cast<bool>(std::getline(lines, line)));
                        const auto fieldCells = cells(line + "\n");
                        REQUIRE(fieldCells.size() == 7);
                        CHECK(fieldCells[0] == "wire-field");
                        CHECK(fieldCells[3] == std::to_string(field));
                        CHECK(fieldCells[5] == std::to_string(description.fields[field].serialTypeId));

                        if (field < plan->eventTimeFieldCount) {
                            CHECK(fieldCells[6] == "-");
                            CHECK(fieldCells[4] == (field == 0
                                                        ? "004500760065006e007400540069006d0065"
                                                        : "004500760065006e007400530065007100750065006e00630065"));
                        } else {
                            REQUIRE(plan->binding.fields[field].targetFieldId);
                            CHECK(fieldCells[6] == std::to_string(*plan->binding.fields[field].targetFieldId));
                        }
                    }

                    CHECK_FALSE(static_cast<bool>(std::getline(lines, line)));

                    if (description.name == u"TimeAndSale") {
                        REQUIRE(description.fields.size() > 2);
                        CHECK(description.fields[2].name == u"Time");
                        CHECK(description.fields[2].serialTypeId == 56);
                        CHECK(plan->binding.fields[2].targetFieldId.has_value());
                    }

                    ::limits([&](std::size_t budget) {
                        return formatter.sourceWireDescription(1, 0, description, *plan, budget);
                    });
                }
            }

            CHECK(reader.finish() == p::TapeStatus::FINISHED);
            CHECK(descriptions > 0);
        }
    }
}

TEST_CASE("source descriptions refuse malformed virtual prefixes and ordinary unbound actions") {
    const c::LocalRecord declaration{0, u"Test", false, {{8}}, {{u"Value", 0}}};
    auto schema =
        c::LocalSchema::create(std::span(&declaration, 1), {1, 1, 1, 64, 128}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const p::WireRecordDescription description{
        12, u"Test", {{u"EventTime", 56}, {u"EventSequence", 72}, {u"Value", 8}}};
    const auto context = p::StreamDataContext::createWithFieldTimePolicy(
        *schema, std::span(&description, 1), {{1, 1, 1, 64, 128}, 1, {3, 64, 128}}, p::FieldTimePolicy::READ);
    REQUIRE(context);
    const auto *plan = context->find(12);
    REQUIRE(plan);
    const f::Formatter formatter(f::OutputFormat::TSV, f::DisplayProfile::BINARY_CATALOG_V1);
    const auto valid = formatter.sourceWireDescription(1, 0, description, *plan, BUDGET);
    CHECK(!valid.empty());

    for (std::size_t mutation = 0; mutation < 11; ++mutation) {
        CAPTURE(mutation);
        auto changedDescription = description;
        auto changedPlan = *plan;

        switch (mutation) {
        case 0:
            changedDescription.fields[0].name = u"EventTimeExtra";
            break;
        case 1:
            changedDescription.fields[0].serialTypeId = 8;
            changedPlan.binding.fields[0].type = c::SerialFieldType::COMPACT_INT;
            break;
        case 2:
            changedDescription.fields[1].serialTypeId = 8;
            changedPlan.binding.fields[1].type = c::SerialFieldType::COMPACT_INT;
            break;
        case 3:
            changedDescription.fields[1].name = u"eventSequence";
            break;
        case 4:
            std::swap(changedDescription.fields[0], changedDescription.fields[1]);
            std::swap(changedPlan.binding.fields[0], changedPlan.binding.fields[1]);
            break;
        case 5:
            changedPlan.eventTimeFieldCount = 1;
            break;
        case 6:
            changedPlan.eventTimeFieldCount = 3;
            break;
        case 7:
            changedPlan.binding.fields[0].targetFieldId = 0;
            break;
        case 8:
            changedPlan.binding.fields[2].targetFieldId.reset();
            break;
        case 9:
            changedPlan.binding.fields[2].targetFieldId = changedPlan.localFieldCount;
            break;
        case 10:
            changedPlan.eventTimeFieldCount = 0;
            break;
        }

        CHECK_THROWS_AS(formatter.sourceWireDescription(1, 0, changedDescription, changedPlan, 0),
                        std::invalid_argument);
    }

    auto ordinaryDescription = description;
    auto ordinaryPlan = *plan;
    ordinaryPlan.eventTimeFieldCount = 0;

    for (std::size_t field = 0; field < ordinaryDescription.fields.size(); ++field) {
        ordinaryDescription.fields[field].serialTypeId = 8;
        ordinaryPlan.binding.fields[field] = {0, c::SerialFieldType::COMPACT_INT};
    }

    CHECK(formatter.sourceWireDescription(1, 0, ordinaryDescription, ordinaryPlan, BUDGET) ==
          formatter.wireDescription(1, 0, ordinaryDescription, ordinaryPlan, BUDGET));
    std::swap(ordinaryDescription.fields[0], ordinaryDescription.fields[2]);
    ordinaryDescription.fields[2].serialTypeId = 56;
    ordinaryPlan.binding.fields[2] = {std::nullopt, c::SerialFieldType::TIME_SECONDS};
    CHECK_THROWS_AS(formatter.sourceWireDescription(1, 0, ordinaryDescription, ordinaryPlan, 0), std::invalid_argument);
}

/** Run formatter checks against explicitly selected immutable producer fixtures.
 * @param argc Argument count.
 * @param argv Baseline/current fixture roots followed by optional doctest switches.
 * @return Doctest status, or usage error before tests when roots are missing.
 */
int main(int argc, char **argv) {
    if (argc < 3) {
        return 2;
    }

    fixtureRoots = {argv[1], argv[2]};
    doctest::Context context;
    context.applyCommandLine(argc - 2, argv + 2);

    return context.run();
}
