// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/series.hpp"

#include "evidence_cells.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/protocol/describe_records.hpp"
#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/stream_data.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace c = q::core;
namespace e = q::events;
namespace p = q::protocol;
using Type = c::SerialFieldType;

constexpr c::SchemaLimits SCHEMA{1, 32, 32, 64, 512};
constexpr e::MappingLimits MAPPING{32, 32, 16, 16, 128, 64, 1024, 10000};
constexpr p::ContextLimits BINDING_LIMITS{SCHEMA, 1, {64, 64, 2048}};
constexpr p::StreamDataLimits DATA{4096, 16, 256, 16, 256, 128, {4096, 32, 32}, {4096, 128}, 4096, 128, 128,
                                   0,    0,  0,   0,  0,   16};
constexpr std::size_t RECORD_COUNT = 8;
constexpr std::size_t MAX_SYMBOL_UNITS = 32;
constexpr std::int32_t REMOVE_FLAG = 2;

struct Observation {
    std::vector<std::int64_t> values;
    std::vector<std::string> event;
};

struct Scenario {
    c::LocalRecord record{0, u"Series", true, {}, {}, c::IndexedTimeLowInt32{0}};
    std::array<Observation, RECORD_COUNT> samples;
};

/** @param path Directory of one actual Java production parser capture.
 * @return Complete fixture for that scenario, including original logical field order.
 */
Scenario readScenario(const std::filesystem::path &path) {
    std::ifstream input(path / "oracle.tsv");
    REQUIRE(input);
    Scenario result;
    std::vector<std::string> physical;
    std::string row;

    while (std::getline(input, row)) {
        const auto cells = q::test_support::split(row);

        if (cells[0] == "record") {
            REQUIRE(cells.size() == 4);
            CHECK(cells[2] == "Series");
            CHECK(cells[3] == "true");
        } else if (cells[0] == "physical") {
            REQUIRE(cells.size() == 4);
            CHECK(std::stoul(cells[1]) == physical.size());
            physical.push_back(cells[3]);
        } else if (cells[0] == "field") {
            REQUIRE(cells.size() == 4);
            result.record.lookups.push_back(
                {std::u16string(cells[2].begin(), cells[2].end()), result.record.fields.size()});
            result.record.fields.push_back({std::stoi(cells[1])});
            CHECK(cells[2] == cells[3]);
        } else if (cells[0] == "event") {
            REQUIRE(cells.size() == 27);
            result.samples.at(std::stoul(cells[1])).event = cells;
        } else if (cells[0] == "value") {
            REQUIRE(cells.size() == 3);
            result.samples.at(std::stoul(cells[1])).values.push_back(std::stoll(cells[2]));
        } else {
            FAIL("Unexpected oracle row");
        }
    }

    REQUIRE(physical.size() >= 2);
    CHECK(physical[0] == "Void");
    CHECK(physical[1] == "Index");
    REQUIRE_FALSE(result.record.lookups.empty());
    CHECK(result.record.lookups[0].name == u"Index");

    return result;
}

/** @param value Native business double.
 * @return Canonical binary64 bits matching Java Double.doubleToLongBits.
 */
std::int64_t doubleBits(double value) {
    return std::isnan(value) ? 0x7ff8000000000000LL : std::bit_cast<std::int64_t>(value);
}

/** @param event Native owning projection.
 * @param row Actual production delegate observation.
 */
void verifyEvent(const e::Series &event, const std::vector<std::string> &row) {
    REQUIRE(row.size() == 27);
    CHECK(event.symbol == std::u16string(row[2].begin(), row[2].end()));
    CHECK(event.eventTime == std::stoll(row[3]));
    CHECK(event.eventFlags == std::stoi(row[4]));
    CHECK(event.getSource().id() == std::stoi(row[5]));
    CHECK(event.getIndex() == std::stoll(row[6]));
    CHECK(event.timeSequence == std::stoll(row[7]));
    CHECK(event.getTime() == std::stoll(row[8]));
    CHECK(event.getSequence() == std::stoi(row[9]));
    CHECK(event.getExpiration() == std::stoi(row[10]));
    const std::array numeric{event.getVolatility(),   event.getCallVolume(),   event.getPutVolume(),
                             event.getOptionVolume(), event.getPutCallRatio(), event.getForwardPrice(),
                             event.getDividend(),     event.getInterest()};
    const std::array raw{event.volatility.raw,   event.callVolume.raw, event.putVolume.raw, event.putCallRatio.raw,
                         event.forwardPrice.raw, event.dividend.raw,   event.interest.raw};

    for (std::size_t index = 0; index < numeric.size(); ++index) {
        CHECK(doubleBits(numeric[index]) == std::stoll(row[11 + index]));
    }

    for (std::size_t index = 0; index < raw.size(); ++index) {
        CHECK(raw[index] == std::stoll(row[19 + index]));
    }
}

/** @param schema Complete local order and field types.
 * @return Matching canonical defaults.
 */
std::vector<c::FieldValue> defaultValues(const c::LocalSchema &schema) {
    std::vector<c::FieldValue> result;

    for (const auto &field : schema.records()[0].fields) {
        result.push_back({static_cast<Type>(field.serialTypeId), 0});
    }

    return result;
}
} // namespace

TEST_CASE("Series standard and configured schemas match actual QD HISTORY delegates") {
    for (const std::string_view variant : {"standard", "tiny", "optional", "field"}) {
        CAPTURE(variant);
        const auto directory = std::filesystem::path(ORG_TTLDTOR_QDCXX_SERIES_FIXTURE) / variant;
        const auto fixture = readScenario(directory);
        auto schema = variant == "standard" ? e::defaultSeriesSchema(SCHEMA)
                                            : c::LocalSchema::create(std::array{fixture.record}, SCHEMA,
                                                                     c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);
        REQUIRE(schema->records()[0].fields.size() == fixture.record.fields.size());

        for (std::size_t index = 0; index < fixture.record.fields.size(); ++index) {
            CHECK(schema->records()[0].fields[index].serialTypeId == fixture.record.fields[index].serialTypeId);
            CHECK(schema->records()[0].lookups[index].name == fixture.record.lookups[index].name);
        }

        const auto mapper = e::PreparedSeries::prepare(*schema, 0, MAPPING);
        REQUIRE(mapper);
        std::ifstream input(directory / "series.qtp", std::ios::binary);
        REQUIRE(input);
        const std::vector<char> chars((std::istreambuf_iterator<char>(input)), {});
        const auto bytes = std::as_bytes(std::span(chars));
        p::FrameReader reader({4096, 4105});
        std::optional<p::StreamDataContext> context;
        std::size_t offset = 0;
        std::size_t sampleId = 0;

        while (offset < bytes.size()) {
            auto frame = reader.feed(bytes.subspan(offset));
            REQUIRE(frame.accepted > 0);
            offset += frame.accepted;
            REQUIRE(frame.status != p::FrameStatus::FAILED);

            if (!frame.frame) {
                continue;
            }

            if (frame.frame->type == 2) {
                const auto parsed = p::parseDescribeRecords(frame.frame->payload, {4096, 1, 64, 64, {4096, 512}, 4096});
                REQUIRE(parsed.status == p::DescriptionStatus::OK);
                const auto timePolicy = variant == "field" ? p::FieldTimePolicy::READ : p::FieldTimePolicy::DISABLED;
                auto bound = p::StreamDataContext::createWithFieldTimePolicy(*schema, parsed.descriptions,
                                                                             BINDING_LIMITS, timePolicy);
                REQUIRE(bound);
                context.emplace(std::move(*bound));

                continue;
            }

            REQUIRE(frame.frame->type == 20);
            REQUIRE(context);
            const auto decoded = p::decodeStreamDataWithEventPolicy(frame.frame->payload, *context, DATA,
                                                                    p::DataEventPolicy::BINARY_CATALOG_V1);
            INFO("decode stage=", static_cast<int>(decoded.stage), " offset=", decoded.errorOffset);
            REQUIRE(decoded.status == p::StreamDataStatus::OK);

            for (const auto &record : decoded.records) {
                CAPTURE(sampleId);
                REQUIRE(sampleId < fixture.samples.size());
                const auto &sample = fixture.samples[sampleId++];
                REQUIRE(record.typedValues.size() == sample.values.size());

                for (std::size_t index = 0; index < sample.values.size(); ++index) {
                    CHECK(record.typedValues[index].raw == sample.values[index]);
                }

                const auto time = record.fieldTime ? record.fieldTime->eventTimeMs() : 0;
                const auto flags = record.symbol.eventFlags;
                CHECK(record.fieldTime.has_value() == (variant == "field"));
                CHECK(record.removalTime.has_value() == ((flags & REMOVE_FLAG) != 0));

                if (record.removalTime) {
                    CHECK(*record.removalTime == std::stoll(sample.event.at(26)));
                }

                const auto event = mapper->map(record.symbol.units, {record.context, record.typedValues},
                                               MAX_SYMBOL_UNITS, time, flags);
                REQUIRE(event);
                verifyEvent(*event, sample.event);
                std::vector<c::FieldValueView> views;

                for (const auto &value : record.typedValues) {
                    views.push_back({value.type, value.raw, {}, {}});
                }

                const auto fromView =
                    mapper->mapView(record.symbol.units, {record.context, views}, MAX_SYMBOL_UNITS, time, flags);
                REQUIRE(fromView);
                verifyEvent(*fromView, sample.event);
            }
        }

        CHECK(reader.finish() == p::FrameStatus::FINISHED);
        CHECK(sampleId == RECORD_COUNT);
    }
}

TEST_CASE("Series optional defaults and required-field refusals match actual Java mapping") {
    std::ifstream input(std::filesystem::path(ORG_TTLDTOR_QDCXX_SERIES_FIXTURE) / "optional/optional.tsv");
    REQUIRE(input);
    auto minimal = c::LocalRecord{0, u"CustomSeries", false, {}, {}};
    const std::array requiredNames{u"Expiration", u"Volatility", u"PutCallRatio", u"ForwardPrice"};

    for (const auto name : requiredNames) {
        minimal.lookups.push_back({name, minimal.fields.size()});
        minimal.fields.push_back(
            {std::to_underlying(name == std::u16string_view(u"Expiration") ? Type::DATE : Type::WIDE_DECIMAL)});
    }

    const auto schema = c::LocalSchema::create(std::array{minimal}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapper = e::PreparedSeries::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    const auto fields = defaultValues(*schema);
    const auto event = mapper->map(u"SPY", {schema->recordContext(0), fields}, MAX_SYMBOL_UNITS);
    REQUIRE(event);
    std::string row;
    REQUIRE(std::getline(input, row));
    const auto defaults = q::test_support::split(row);
    REQUIRE(defaults.size() == 7);
    CHECK(defaults[0] == "absent");
    CHECK(event->getIndex() == std::stoll(defaults[1]));
    CHECK(event->timeSequence == std::stoll(defaults[2]));
    CHECK(doubleBits(event->getCallVolume()) == std::stoll(defaults[3]));
    CHECK(doubleBits(event->getPutVolume()) == std::stoll(defaults[4]));
    CHECK(doubleBits(event->getDividend()) == std::stoll(defaults[5]));
    CHECK(doubleBits(event->getInterest()) == std::stoll(defaults[6]));
    std::size_t missingCount = 0;

    while (std::getline(input, row)) {
        const auto cells = q::test_support::split(row);
        REQUIRE(cells.size() == 3);
        CHECK(cells[0] == "reject");
        CHECK(cells[2] == "IllegalArgumentException");
        const std::u16string omitted(cells[1].begin(), cells[1].end());
        auto malformed = c::LocalRecord{0, u"Series", false, {}, {}};

        for (const auto &lookup : minimal.lookups) {
            if (lookup.name != omitted) {
                malformed.lookups.push_back({lookup.name, malformed.fields.size()});
                malformed.fields.push_back(minimal.fields[lookup.fieldId]);
            }
        }

        const auto missing = c::LocalSchema::create(std::array{malformed}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(missing);
        CHECK(e::PreparedSeries::prepare(*missing, 0, MAPPING) == std::unexpected(e::MappingError::MISSING_REQUIRED));
        ++missingCount;
    }

    CHECK(missingCount == requiredNames.size());
    minimal.fields[1].serialTypeId = std::to_underlying(Type::UTF_CHAR_ARRAY);
    const auto wrongType = c::LocalSchema::create(std::array{minimal}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(wrongType);
    CHECK(e::PreparedSeries::prepare(*wrongType, 0, MAPPING) == std::unexpected(e::MappingError::INCOMPATIBLE_TYPE));
}

TEST_CASE("Series complete shape, ownership and symbol budget survive mapping input changes") {
    const auto schema = e::defaultSeriesSchema(SCHEMA);
    REQUIRE(schema);
    const auto mapper = e::PreparedSeries::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = defaultValues(*schema);
    std::u16string symbol = u"EXACT-OWNING-UNDERLYING";
    const auto event = mapper->map(symbol, {schema->recordContext(0), fields}, symbol.size());
    REQUIRE(event);
    symbol[0] = u'X';
    CHECK(event->symbol == u"EXACT-OWNING-UNDERLYING");
    CHECK(mapper->map(symbol, {schema->recordContext(0), fields}, symbol.size() - 1) ==
          std::unexpected(e::MappingError::RESOURCE_LIMIT));
    const auto unrelated = e::defaultSeriesSchema(SCHEMA);
    REQUIRE(unrelated);
    CHECK(mapper->map(symbol, {unrelated->recordContext(0), fields}, MAX_SYMBOL_UNITS) ==
          std::unexpected(e::MappingError::WRONG_CONTEXT));
    fields.front().raw = std::numeric_limits<std::int64_t>::max();
    CHECK(mapper->map(symbol, {schema->recordContext(0), fields}, MAX_SYMBOL_UNITS) ==
          std::unexpected(e::MappingError::INVALID_VALUES));

    auto edited = *event;
    edited.callVolume = c::WideDecimal{0x180};
    CHECK(edited.getCallVolume() == 1);
    CHECK(edited.getOptionVolume() == 1);
    CHECK(std::isnan(event->getOptionVolume()));
}
