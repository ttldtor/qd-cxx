// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "../../tools/tape-inspect/inspect.hpp"
#include "event_oracle.hpp"
#include "qd/schema/json_schema.hpp"
#include <doctest/doctest.h>
#include <sstream>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::events;
namespace p = org::ttldtor::qdcxx::protocol;
namespace s = org::ttldtor::qdcxx::schema;
namespace ti = org::ttldtor::qdcxx::tape_inspect;
namespace display = org::ttldtor::qdcxx::inspect;
namespace o = event_oracle;
std::filesystem::path root; ///< Explicit committed capture root.

/** Owned record and the container metadata returned with it. */
struct CapturedRecord : p::StreamDataRecord {
    p::TapeTiming timing; ///< Independent Java cursor time, zero in legacy mode.
};

/** Read a bounded text capture.
 * @param path Capture path supplied by the test.
 * @return Exact owned bytes.
 */
std::string text(const std::filesystem::path &path) {
    const auto bytes = o::bytes(path);

    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

/** Load actual exporter bytes, then overwrite and destroy the original storage.
 * @param scenario Named independent capture.
 * @return Owned immutable native schema.
 */
c::LocalSchema load(std::string_view scenario) {
    auto json = text(root / scenario / "schema.json");
    auto schema = s::loadJsonSchema(json);
    REQUIRE(schema);
    json.assign(json.size(), 'x');

    return std::move(*schema);
}

/** Decode production tape with explicit chunking and retain independently owned records.
 * @param schema Owned schema context.
 * @param input Finite production tape.
 * @param mode Container policy.
 * @param chunk Positive input scheduling bound.
 * @return Owned completed records after EOF.
 */
std::vector<CapturedRecord> decode(const c::LocalSchema &schema, std::span<const std::byte> input, p::TapeMode mode,
                                   std::size_t chunk) {
    p::TapeReader reader(schema, ti::Limits{}.tape, mode);
    std::vector<CapturedRecord> records;

    while (!input.empty()) {
        auto result = reader.feed(input.first(std::min(chunk, input.size())));
        REQUIRE(result.status != p::TapeStatus::FAILED);
        REQUIRE(result.accepted > 0);
        input = input.subspan(result.accepted);

        for (auto &record : result.records) {
            records.push_back({std::move(record), result.timing.value_or(p::TapeTiming{})});
        }
    }

    REQUIRE(reader.finish() == p::TapeStatus::FINISHED);

    return records;
}

/** Render generic values in the independent Java observation grammar.
 * @param records Production-decoded owned records.
 * @return Exact logical field observations, distinct from native JSON syntax.
 */
std::string observations(const std::vector<CapturedRecord> &records) {
    std::ostringstream out;

    for (const auto &record : records) {
        out << "value\t" << o::hexUnits(record.symbol.units) << '\t' << record.timing.eventTimeSequence;

        for (const auto &value : record.typedValues) {
            out << '\t' << std::to_underlying(value.type) << ':' << value.raw << ':' << o::nullable(value.units);
        }

        out << '\n';
    }

    return out.str();
}
} // namespace

TEST_CASE("exported configured events retain exact Java production fields and event projections") {
    auto schema = load("set-before");
    REQUIRE(schema.records().size() == 2);
    REQUIRE(schema.records()[0].name == u"Quote");
    REQUIRE(schema.records()[1].name == u"Profile");
    const auto quote = e::PreparedQuote::prepare(schema, 0, o::mappingLimits());
    const auto profile = e::PreparedProfile::prepare(schema, 1, o::mappingLimits());
    REQUIRE(quote);
    REQUIRE(profile);
    const auto oracle = o::read(root / "set-before/observations.tsv");

    for (const std::string name : {"Quote", "Profile"}) {
        for (const std::string suffix : {"stream", "raw"}) {
            const auto mode = suffix == "raw" ? p::TapeMode::RAW_MESSAGE_V1 : p::TapeMode::LEGACY;
            const auto input = o::bytes(root / "set-before" / (name + "-" + suffix + ".qtp"));
            std::vector<const o::Observation *> expected;

            for (const auto &observed : oracle.observations) {
                if (observed.phase == name + "-" + suffix + "-read") {
                    expected.push_back(&observed);
                }
            }

            REQUIRE(expected.size() == 3);

            for (const auto chunk : {std::size_t{1}, input.size()}) {
                auto records = decode(schema, input, mode, chunk);
                REQUIRE(records.size() == expected.size());

                for (std::size_t index = 0; index < records.size(); ++index) {
                    const auto &record = records[index];
                    const auto &observed = *expected[index];
                    CHECK(record.typedValues == observed.values);
                    CHECK(record.timing.eventTimeSequence == observed.timeSequence);

                    if (name == "Quote") {
                        const auto event = quote->map(record.symbol.units, {record.context, record.typedValues}, 1024,
                                                      record.timing.eventTimeMs());
                        REQUIRE(event);
                        CHECK(o::properties(*event) == observed.event);
                    } else {
                        const auto event = profile->map(record.symbol.units, {record.context, record.typedValues},
                                                        {1024, 1024, 4096}, record.timing.eventTimeMs());
                        REQUIRE(event);
                        CHECK(o::properties(*event) == observed.event);
                    }
                }
            }
        }
    }

    // Both mappings and captured records retain the context after the schema owner is replaced.
    const auto retainedQuote = decode(schema, o::bytes(root / "set-before/Quote-stream.qtp"), p::TapeMode::LEGACY, 1);
    const auto retainedProfile =
        decode(schema, o::bytes(root / "set-before/Profile-stream.qtp"), p::TapeMode::LEGACY, 1);
    schema = load("custom");
    REQUIRE(quote->map(retainedQuote[0].symbol.units, {retainedQuote[0].context, retainedQuote[0].typedValues}, 1024));
    REQUIRE(profile->map(retainedProfile[0].symbol.units, {retainedProfile[0].context, retainedProfile[0].typedValues},
                         {1024, 1024, 4096}));
}

TEST_CASE("XML import overlay and Java storage slots survive exported generic inspection") {
    const auto schema = load("custom");
    REQUIRE(schema.records().size() == 1);
    const auto &record = schema.records()[0];
    REQUIRE(record.fields.size() == 4);
    CHECK(record.fields[0].serialTypeId == 120);
    CHECK(record.fields[1].serialTypeId == 8);
    CHECK(record.fields[2].serialTypeId == 8);
    CHECK(record.fields[3].serialTypeId == 10);

    for (const std::string suffix : {"stream", "raw"}) {
        const auto mode = suffix == "raw" ? p::TapeMode::RAW_MESSAGE_V1 : p::TapeMode::LEGACY;
        const auto input = o::bytes(root / "custom" / (suffix + ".qtp"));
        const auto expected = text(root / "custom" / (suffix + ".tsv"));

        for (const auto chunk : {std::size_t{1}, input.size()}) {
            const auto decoded = decode(schema, input, mode, chunk);
            REQUIRE(decoded.size() == 3);
            CHECK(observations(decoded) == expected);
            auto changed = decoded;
            ++changed[0].typedValues[1].raw;
            CHECK(observations(changed) != expected);

            for (const auto format : {display::OutputFormat::TSV, display::OutputFormat::HUMAN}) {
                std::istringstream source(std::string(reinterpret_cast<const char *>(input.data()), input.size()));
                std::ostringstream output, error;
                ti::Limits limits;
                limits.chunkSize = chunk;
                REQUIRE(ti::inspect(source, output, error, schema, limits, format,
                                    display::DisplayProfile::EXTERNAL_SCHEMA_V1, 2, mode) == 0);
                CHECK(error.str().empty());
                CHECK(output.str().find("1234567890123") != std::string::npos);
                CHECK(output.str().find("-7") != std::string::npos);
            }
        }
    }
}

TEST_CASE("changed exported type and alias target cannot preserve observed production values") {
    const auto original = text(root / "custom/schema.json");
    const auto input = o::bytes(root / "custom/stream.qtp");
    const auto expected = text(root / "custom/stream.tsv");
    auto changed = original;
    const std::string originalType = "\"serialTypeId\":120";
    const auto typePosition = changed.find(originalType);
    REQUIRE(typePosition != changed.npos);
    changed.replace(typePosition, originalType.size(), "\"serialTypeId\":8");
    const auto schema = s::loadJsonSchema(changed);
    REQUIRE(schema);
    p::TapeReader reader(*schema, ti::Limits{}.tape, p::TapeMode::LEGACY);
    auto remaining = std::span(input);
    std::vector<CapturedRecord> decoded;
    bool rejected = false;

    while (!remaining.empty()) {
        auto result = reader.feed(remaining.first(1));

        if (result.status == p::TapeStatus::FAILED) {
            rejected = true;
            break;
        }

        REQUIRE(result.accepted > 0);
        remaining = remaining.subspan(result.accepted);

        for (auto &record : result.records) {
            decoded.push_back({std::move(record), {}});
        }
    }

    CHECK((rejected || observations(decoded) != expected));
    auto aliasJson = text(root / "set-before/schema.json");
    const std::string key = "\"name\":\"BidPrice\",\"fieldId\":4";
    const auto position = aliasJson.find(key);
    REQUIRE(position != aliasJson.npos);
    aliasJson.replace(position, key.size(), "\"name\":\"BidPrice\",\"fieldId\":8");
    const auto ambiguous = s::loadJsonSchema(aliasJson);
    REQUIRE(ambiguous);
    CHECK_FALSE(e::PreparedQuote::prepare(*ambiguous, 0, o::mappingLimits()));
}

TEST_CASE("actual exporter empty and escaped scalar names roundtrip through native loader") {
    CHECK(load("empty").records().empty());
    const auto schema = load("names");
    REQUIRE(schema.records().size() == 1);
    const std::u16string expected = std::u16string(u"Control") + u'\0' + u"\"\\\U0001f600";
    const std::u16string alias = std::u16string(u"Alias") + u'\0' + u"\"\\\U0001f600";
    CHECK(schema.records()[0].name == expected);
    CHECK(std::ranges::any_of(schema.records()[0].lookups, [&](const auto &lookup) {
        return lookup.name == alias && lookup.fieldId == 0;
    }));
}

/** Execute committed-fixture checks without a Java runtime or generator.
 * @param argc Argument count.
 * @param argv Repository root and optional doctest arguments.
 * @return Nonzero on missing root or failed checks.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }

    root = std::filesystem::path(argv[1]) / "tests/fixtures/schema/export/expected";
    doctest::Context context;
    context.applyCommandLine(argc - 1, argv + 1);

    return context.run();
}
