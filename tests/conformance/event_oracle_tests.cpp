// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "event_oracle.hpp"
#include <doctest/doctest.h>

namespace {
namespace o = event_oracle;
std::filesystem::path fixtures; ///< Immutable new Java observations and original tapes.
constexpr std::array<std::string_view, 21> CASES{"default",
                                                 "sequence",
                                                 "nano",
                                                 "both",
                                                 "explicit",
                                                 "endpoint-file",
                                                 "endpoint-api",
                                                 "mutation",
                                                 "set-before",
                                                 "xml",
                                                 "xml-system",
                                                 "xml-properties",
                                                 "xml-extension",
                                                 "xml-unsupported",
                                                 "reorder",
                                                 "minimal",
                                                 "rejected",
                                                 "numeric-int",
                                                 "integer-representation",
                                                 "tiny-unsupported",
                                                 "invalid-boolean"}; ///< Complete oracle scenario inventory.

/** Select complete replay observations for one original file.
 * @param scenario Parsed fixture.
 * @param phase Exact replay phase.
 * @return Borrowed ordered records.
 */
std::vector<const o::Observation *> observations(const o::Scenario &scenario, std::string_view phase) {
    std::vector<const o::Observation *> selected;

    for (const auto &observation : scenario.observations) {
        if (observation.phase == phase) {
            selected.push_back(&observation);
        }
    }

    return selected;
}

/** Compare the typed projection against every captured Java event cell.
 * @param schema Matching validated local schema.
 * @param record Owned generic record, unchanged by projection.
 * @param expected Exact production event observation.
 * @param time Delivered metadata milliseconds.
 */
void compare(const o::c::LocalSchema &schema, const o::p::StreamDataRecord &record, const o::Observation &expected,
             std::int64_t time) {
    REQUIRE(record.localId == (expected.name == "Profile" ? 0 : 1));
    REQUIRE(record.typedValues == expected.values);
    REQUIRE(record.symbol.units == o::units(expected.event[0]));
    const o::e::RecordValueView view{record.context, record.typedValues};

    if (expected.name == "Profile") {
        auto mapping = o::e::PreparedProfile::prepare(schema, record.localId, o::mappingLimits());
        REQUIRE(mapping);
        auto event = mapping->map(record.symbol.units, view, {1024, 1024, 4096}, time);
        REQUIRE(event);
        CHECK(o::properties(*event) == expected.event);
    } else {
        auto mapping = o::e::PreparedQuote::prepare(schema, record.localId, o::mappingLimits());
        REQUIRE(mapping);
        auto event = mapping->map(record.symbol.units, view, 1024, time);
        REQUIRE(event);
        CHECK(o::properties(*event) == expected.event);
    }

    CHECK(record.typedValues == expected.values);
}
} // namespace

TEST_CASE("every accepted configured schema maps direct Java cursor and event observations") {
    for (auto name : CASES) {
        CAPTURE(name);
        const auto scenario = o::read(fixtures / name / "observations.tsv");
        auto schema = o::schema(scenario);

        if (name == "xml-unsupported" || name == "tiny-unsupported") {
            REQUIRE_FALSE(schema);
            CHECK(schema.error() == o::c::SchemaError::UNSUPPORTED_PROFILE);
            continue;
        }

        REQUIRE(schema);

        if (name == "rejected") {
            continue;
        }

        REQUIRE_FALSE(scenario.observations.empty());

        for (const auto &observation : scenario.observations) {
            CAPTURE(observation.phase);
            CAPTURE(observation.index);
            o::p::StreamDataRecord record;
            record.localId = observation.name == "Profile" ? 0 : 1;
            record.symbol.units = o::units(observation.event[0]);
            record.typedValues = observation.values;
            record.context = schema->recordContext(record.localId);
            const auto time = o::number<std::int64_t>(observation.event[observation.name == "Profile" ? 19 : 12]);
            compare(*schema, record, observation, time);
        }
    }
}

TEST_CASE("original configured tapes reach EOF with exact generic and event values in both modes") {
    for (auto name : CASES) {
        if (name == "rejected" || name == "xml-unsupported" || name == "tiny-unsupported") {
            continue;
        }

        CAPTURE(name);
        const auto scenario = o::read(fixtures / name / "observations.tsv");
        auto schema = o::schema(scenario);
        REQUIRE(schema);

        for (const std::string eventName : {"Profile", "Quote"}) {
            for (const std::string suffix : {"stream", "raw", "wire-omitted"}) {
                const auto stem = eventName + "-" + suffix;
                CAPTURE(stem);
                const auto expected = observations(scenario, stem + "-read");
                REQUIRE(expected.size() == (suffix == "wire-omitted" ? 1 : 3));
                const auto input = o::bytes(fixtures / name / (stem + ".qtp"));

                for (const std::size_t chunk : {std::size_t{1}, input.size()}) {
                    CAPTURE(chunk);
                    o::p::TapeReader reader(*schema, o::tapeLimits(),
                                            suffix == "raw" ? o::p::TapeMode::RAW_MESSAGE_V1 : o::p::TapeMode::LEGACY);
                    std::size_t offset = 0;
                    std::size_t index = 0;

                    while (offset < input.size()) {
                        const auto result =
                            reader.feed(std::span(input).subspan(offset, std::min(chunk, input.size() - offset)));
                        REQUIRE(result.status != o::p::TapeStatus::FAILED);
                        REQUIRE(result.accepted > 0);
                        offset += result.accepted;

                        for (const auto &record : result.records) {
                            REQUIRE(index < expected.size());
                            CHECK((result.timing ? result.timing->eventTimeSequence : 0) ==
                                  expected[index]->timeSequence);
                            compare(*schema, record, *expected[index++],
                                    result.timing ? result.timing->eventTimeMs() : 0);
                        }
                    }

                    CHECK(reader.finish() == o::p::TapeStatus::FINISHED);
                    CHECK(index == expected.size());
                }
            }
        }
    }
}

/** Run strict fixture-backed adapter and file comparisons.
 * @param argc Argument count.
 * @param argv Fixture root followed by optional doctest arguments.
 * @return Nonzero for malformed/missing fixtures or any mismatch.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }

    fixtures = argv[1];
    doctest::Context context;
    context.applyCommandLine(argc - 1, argv + 1);

    return context.run();
}
