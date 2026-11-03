// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "event_time_fixture.hpp"

#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/protocol/stream_data.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <bit>
#include <limits>

namespace {
namespace f = event_time_fixture;
namespace core = f::core;
namespace events = org::ttldtor::qdcxx::events;
namespace protocol = org::ttldtor::qdcxx::protocol;
std::filesystem::path fixture; ///< Explicit immutable input selected by CTest, never discovered from ambient state.
constexpr core::SchemaLimits SCHEMA_LIMITS{16, 128, 256, 128, 16384}; ///< Existing complete receiving bounds.
constexpr events::MappingLimits MAPPING_LIMITS{128, 256, 32, 64, 64, 128, 20480, 3000000};

///< Finite property-resolution allowance, independent of corpus row counts.

/** Borrow every canonical field without creating owned intermediate payloads.
 * @param values Complete independently owned cursor fields.
 * @return Field views valid while values and their payloads remain unchanged.
 */
std::vector<core::FieldValueView> views(std::span<const core::FieldValue> values) {
    std::vector<core::FieldValueView> result;

    for (const auto &value : values) {
        result.push_back(
            {value.type, value.raw, value.units ? std::optional<std::u16string_view>(*value.units) : std::nullopt, {}});
    }

    return result;
}

/** Check all Quote time/exchange/identity properties against an actual production delegate observation.
 * @param event Complete owned projection.
 * @param expected Original Java result cells, including wrapped nanosecond getter output.
 */
void compare(const events::Quote &event, const std::vector<std::string> &expected) {
    REQUIRE(expected.size() == 10);
    CHECK(event.symbol == f::units(expected[0]));
    CHECK(event.bidTimeMs == std::stoll(expected[1]));
    CHECK(event.bidExchangeCode == std::stoul(expected[2]));
    CHECK(event.askTimeMs == std::stoll(expected[3]));
    CHECK(event.askExchangeCode == std::stoul(expected[4]));
    CHECK(event.timeMs == std::stoll(expected[5]));
    CHECK(event.sequence == std::stoll(expected[6]));
    CHECK(event.timeNanoPart == std::stoll(expected[7]));
    CHECK(event.eventTime == std::stoll(expected[8]));
    // The separate Java getter corroborates the units even for raw negative/out-of-range nano values.
    constexpr std::uint64_t NANOSECONDS_PER_MILLISECOND = 1000000;
    const auto nanos = static_cast<std::uint64_t>(event.timeMs) * NANOSECONDS_PER_MILLISECOND +
                       static_cast<std::uint64_t>(event.timeNanoPart);
    CHECK(std::bit_cast<std::int64_t>(nanos) == std::stoll(expected[9]));
}

/** Check Profile time/identity/string defaults against the real generated delegate.
 * @param event Complete owned projection.
 * @param expected Original Java result cells.
 */
void compare(const events::Profile &event, const std::vector<std::string> &expected) {
    REQUIRE(expected.size() == 6);
    CHECK(event.symbol == f::units(expected[0]));
    CHECK(event.haltStartTimeMs == std::stoll(expected[1]));
    CHECK(event.haltEndTimeMs == std::stoll(expected[2]));
    CHECK(event.eventTime == std::stoll(expected[3]));

    for (const auto &[actual, cell] :
         std::array{std::pair{event.description, expected[4]}, std::pair{event.statusReason, expected[5]}}) {
        if (cell == "null") {
            CHECK_FALSE(actual);
        } else {
            REQUIRE(actual);
            CHECK(*actual == f::units(cell));
        }
    }
}
} // namespace

TEST_CASE("actual generated optional and regional schemas reproduce production Java delegates") {
    const auto corpus = f::read(fixture);
    std::size_t checked = 0;

    for (const auto &[name, scenario] : corpus) {
        CAPTURE(name);
        auto schema = core::LocalSchema::create(scenario.records, SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);

        for (const auto &[key, sample] : scenario.samples) {
            CAPTURE(key.first);
            CAPTURE(key.second);
            const auto context = schema->recordContext(static_cast<std::int32_t>(key.first));
            const auto borrowed = views(sample.values);
            const protocol::FieldEventTime time{sample.rawEventTime >> 32,
                                                static_cast<std::int32_t>(sample.rawEventTime)};

            if (scenario.records[key.first].name == u"Profile") {
                auto mapping =
                    events::PreparedProfile::prepare(*schema, static_cast<std::int32_t>(key.first), MAPPING_LIMITS);
                REQUIRE(mapping);
                auto event = mapping->map(sample.symbol, {context, sample.values}, {128, 128, 512}, time.eventTimeMs());
                REQUIRE(event);
                compare(*event, sample.expected);
                auto viewEvent =
                    mapping->mapView(sample.symbol, {context, borrowed}, {128, 128, 512}, time.eventTimeMs());
                REQUIRE(viewEvent);
                compare(*viewEvent, sample.expected);
                auto unspecified = mapping->map(sample.symbol, {context, sample.values}, {128, 128, 512});
                REQUIRE(unspecified);
                CHECK(unspecified->eventTime == 0);
            } else {
                auto mapping =
                    events::PreparedQuote::prepare(*schema, static_cast<std::int32_t>(key.first), MAPPING_LIMITS);
                REQUIRE(mapping);
                auto event = mapping->map(sample.symbol, {context, sample.values}, 128, time.eventTimeMs());
                REQUIRE(event);
                compare(*event, sample.expected);
                auto viewEvent = mapping->mapView(sample.symbol, {context, borrowed}, 128, time.eventTimeMs());
                REQUIRE(viewEvent);
                compare(*viewEvent, sample.expected);
                auto unspecified = mapping->map(sample.symbol, {context, sample.values}, 128);
                REQUIRE(unspecified);
                CHECK(unspecified->eventTime == 0);
                const auto exactUnits = event->symbol.size();
                CHECK(mapping->map(sample.symbol, {context, sample.values}, exactUnits));
                CHECK(mapping->map(sample.symbol, {context, sample.values}, exactUnits - 1).error() ==
                      events::MappingError::RESOURCE_LIMIT);
            }

            ++checked;
        }

        for (const auto recordId : scenario.rejected) {
            const auto result =
                scenario.records[recordId].name == u"Profile"
                    ? events::PreparedProfile::prepare(*schema, recordId, MAPPING_LIMITS).transform([](const auto &) {
                          return true;
                      })
                    : events::PreparedQuote::prepare(*schema, recordId, MAPPING_LIMITS).transform([](const auto &) {
                          return true;
                      });
            REQUIRE_FALSE(result);
            CHECK(result.error() == events::MappingError::MISSING_REQUIRED);
        }
    }

    CHECK(checked == 160);
}

TEST_CASE("optional time fields reject present wrong types and preserve exact context and ownership") {
    const auto corpus = f::read(fixture);
    const auto &scenario = corpus.at("millis");

    for (const auto key : {u"Bid.Time", u"Ask.Time", u"Sequence", u"TimeNanoPart"}) {
        auto records = scenario.records;
        const auto found = std::ranges::find_if(records[1].lookups, [&](const auto &lookup) {
            return lookup.name == key;
        });
        REQUIRE(found != records[1].lookups.end());
        records[1].fields[found->fieldId].serialTypeId = std::to_underlying(core::SerialFieldType::DATE);
        auto wrong = core::LocalSchema::create(records, SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(wrong);
        CHECK(events::PreparedQuote::prepare(*wrong, 1, MAPPING_LIMITS).error() ==
              events::MappingError::INCOMPATIBLE_TYPE);
    }

    auto schema = core::LocalSchema::create(scenario.records, SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    auto equivalent = core::LocalSchema::create(scenario.records, SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(equivalent);
    auto mapping = events::PreparedQuote::prepare(*schema, 1, MAPPING_LIMITS);
    REQUIRE(mapping);
    auto sample = scenario.samples.at({1, 1});
    CHECK(mapping->map(sample.symbol, {equivalent->recordContext(1), sample.values}, 128).error() ==
          events::MappingError::WRONG_CONTEXT);
    auto event = mapping->map(sample.symbol, {schema->recordContext(1), sample.values}, 128);
    REQUIRE(event);
    sample.symbol.assign(u"changed");
    sample.values.clear();
    CHECK(event->symbol == u"AAPL&Q");
    CHECK(event->bidExchangeCode == u'Q');

    auto insufficient = MAPPING_LIMITS;
    insufficient.nameUnits = scenario.records[1].name.size() - 1;
    CHECK(events::PreparedQuote::prepare(*schema, 1, insufficient).error() == events::MappingError::RESOURCE_LIMIT);
    insufficient = MAPPING_LIMITS;
    insufficient.comparisonWork = scenario.records[1].name.size() * 3 - 1;
    CHECK(events::PreparedQuote::prepare(*schema, 1, insufficient).error() == events::MappingError::RESOURCE_LIMIT);

    sample = scenario.samples.at({1, 1});
    const auto sequence = std::ranges::find_if(scenario.records[1].lookups, [](const auto &lookup) {
        return lookup.name == u"Sequence";
    });
    REQUIRE(sequence != scenario.records[1].lookups.end());
    sample.values[sequence->fieldId].raw = std::numeric_limits<std::int64_t>::max();
    CHECK(mapping->map(sample.symbol, {schema->recordContext(1), sample.values}, 128).error() ==
          events::MappingError::INVALID_VALUES);
}

TEST_CASE("regional output symbols match actual delegate conversion including suffixes and attributes") {
    const auto corpus = f::read(fixture);
    const auto &scenario = corpus.at("default");
    REQUIRE(scenario.routes.size() == 21);
    REQUIRE(scenario.selections.size() == 14);
    auto schema = core::LocalSchema::create(scenario.records, SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);

    for (const auto &row : scenario.routes) {
        const auto recordId = std::stoi(row.at(0));
        auto mapping = events::PreparedQuote::prepare(*schema, recordId, MAPPING_LIMITS);
        REQUIRE(mapping);
        const auto &sample = scenario.samples.at({static_cast<std::size_t>(recordId), 0});
        const auto symbol = f::units(row.at(1));
        auto event = mapping->map(symbol, {schema->recordContext(recordId), sample.values}, 128);
        REQUIRE(event);
        CHECK(event->symbol == f::units(row.at(3)));
    }
}

/** Run only the explicit oracle file selected by CTest; native failures propagate as a nonzero exit.
 * @param argc Program name plus one fixture filename.
 * @param argv Borrowed command-line arguments.
 * @return Doctest result, or usage exit2 for an invalid invocation.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }

    fixture = argv[1];

    doctest::Context context;
    context.applyCommandLine(argc - 1, argv + 1);

    return context.run();
}
