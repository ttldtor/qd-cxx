// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/analytic_order.hpp"
#include "qd/events/spread_order.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <doctest/doctest.h>
#include <limits>
#include <type_traits>

namespace {
namespace q = org::ttldtor::qdcxx;
constexpr q::core::SchemaLimits SCHEMA{8, 128, 256, 128, 32768};
constexpr q::events::MappingLimits MAPPING{128, 256, 32, 64, 256, 128, 65536, 1000000};
constexpr q::events::EventValueLimits VALUES{128, 128, 256};

/** Create complete canonical zero/null values in exact local order. */
std::vector<q::core::FieldValue> values(const q::core::LocalSchema &schema) {
    std::vector<q::core::FieldValue> result;

    for (const auto &field : schema.records()[0].fields) {
        result.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});
    }

    return result;
}

/** Borrow one explicit test property; absence is a malformed fixture. */
q::core::FieldValue &field(const q::core::LocalSchema &schema, std::vector<q::core::FieldValue> &v,
                           std::u16string_view key) {
    for (const auto &lookup : schema.records()[0].lookups) {
        if (lookup.name == key) {
            return v.at(lookup.fieldId);
        }
    }

    throw std::runtime_error("Required variant test key absent");
}

/** Construct an explicit custom receive record with every optional analytic property. */
q::core::LocalSchema richAnalytic() {
    auto base = q::events::defaultAnalyticOrderSchema(SCHEMA);

    if (!base) {
        throw std::runtime_error("Cannot create analytic fixture");
    }

    auto record = base->records()[0];
    record.name = u"AnalyticOrder#NTV";
    constexpr std::array names{u"IcebergPeakSize", u"IcebergHiddenSize", u"IcebergExecutedSize", u"IcebergFlags"};

    for (std::size_t i = 0; i < names.size(); ++i) {
        record.lookups.push_back({names[i], record.fields.size()});
        record.fields.push_back({i == 3 ? 8 : 120});
    }

    auto schema = q::core::LocalSchema::create(std::array{record}, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);

    if (!schema) {
        throw std::runtime_error("Cannot create explicit analytic fixture");
    }

    return std::move(*schema);
}
} // namespace

TEST_CASE("SpreadOrder owns nullable exact UTF16 with complete precharged text bounds") {
    auto schema = q::events::defaultSpreadOrderSchema(SCHEMA);
    REQUIRE(schema);
    auto prepared = q::events::PreparedSpreadOrder::prepare(*schema, 0, MAPPING);
    REQUIRE(prepared);
    auto v = values(*schema);
    const std::u16string exact{u'A', u'\0', static_cast<char16_t>(0xd800), static_cast<char16_t>(0xdfff)};
    field(*schema, v, u"SpreadSymbol").units = exact;
    field(*schema, v, u"Index").raw = -1;
    field(*schema, v, u"Time").raw = -1;
    field(*schema, v, u"Sequence").raw = -1;
    auto mapped = prepared->map(u"XYZ", {schema->recordContext(0), v}, {3, 4, 7}, -2147483648000LL, 3);
    REQUIRE(mapped);
    CHECK(mapped->spreadSymbol == exact);
    CHECK(mapped->symbol == u"XYZ");
    CHECK(mapped->index == 0xffffffffLL);
    CHECK(mapped->timeSequence == -1);
    CHECK(mapped->getTime() == 23);
    CHECK(mapped->eventTime == -2147483648000LL);
    CHECK(mapped->eventFlags == 3);
    field(*schema, v, u"SpreadSymbol").units->assign(u"changed");
    CHECK(mapped->spreadSymbol == exact);
    field(*schema, v, u"SpreadSymbol").units = exact;

    for (const auto limits : std::array<q::events::EventValueLimits, 3>{{{2, 4, 7}, {3, 3, 7}, {3, 4, 6}}}) {
        auto refused = prepared->map(u"XYZ", {schema->recordContext(0), v}, limits);
        REQUIRE_FALSE(refused);
        CHECK(refused.error() == q::events::MappingError::RESOURCE_LIMIT);
    }

    field(*schema, v, u"SpreadSymbol").units.reset();
    auto nullValue = prepared->map(u"", {schema->recordContext(0), v}, {0, 0, 0});
    REQUIRE(nullValue);
    CHECK_FALSE(nullValue->spreadSymbol);
    field(*schema, v, u"SpreadSymbol").units.emplace();
    auto empty = prepared->map(u"", {schema->recordContext(0), v}, {0, 0, 0});
    REQUIRE(empty);
    REQUIRE(empty->spreadSymbol);
    CHECK(empty->spreadSymbol->empty());
}

TEST_CASE("variant mappings validate complete context and canonical payload before resource projection") {
    auto schema = q::events::defaultSpreadOrderSchema(SCHEMA);
    auto other = q::events::defaultSpreadOrderSchema(SCHEMA);
    REQUIRE(schema);
    REQUIRE(other);
    auto prepared = q::events::PreparedSpreadOrder::prepare(*schema, 0, MAPPING);
    REQUIRE(prepared);
    auto v = values(*schema);
    auto wrong = prepared->map(u"X", {other->recordContext(0), v}, {0, 0, 0});
    REQUIRE_FALSE(wrong);
    CHECK(wrong.error() == q::events::MappingError::WRONG_CONTEXT);
    field(*schema, v, u"SpreadSymbol").raw = 1;
    auto invalid = prepared->map(u"X", {schema->recordContext(0), v}, {0, 0, 0});
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error() == q::events::MappingError::INVALID_VALUES);
    field(*schema, v, u"SpreadSymbol").raw = 0;
    std::vector<q::core::FieldValueView> views;

    for (const auto &f : v) {
        views.push_back({f.type, f.raw, std::nullopt, std::nullopt});
    }

    std::u16string borrowed = u"temporary";
    views.back().units = borrowed;
    auto owning = prepared->mapView(u"X", {schema->recordContext(0), views}, VALUES);
    REQUIRE(owning);
    borrowed.clear();
    CHECK(owning->spreadSymbol == u"temporary");
    auto small = MAPPING;
    small.selections = 15;
    auto refused = q::events::PreparedSpreadOrder::prepare(*schema, 0, small);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == q::events::MappingError::RESOURCE_LIMIT);
}

TEST_CASE("required SpreadSymbol declaration is distinct from its nullable value and unsupported object forms") {
    auto base = q::events::defaultSpreadOrderSchema(SCHEMA);
    REQUIRE(base);
    auto record = base->records()[0];
    record.lookups.pop_back();
    record.fields.pop_back();
    auto missing = q::core::LocalSchema::create(std::array{record}, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(missing);
    auto refused = q::events::PreparedSpreadOrder::prepare(*missing, 0, MAPPING);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == q::events::MappingError::MISSING_REQUIRED);
    record = base->records()[0];
    record.fields.back().serialTypeId = std::to_underlying(q::core::SerialFieldType::SERIAL_OBJECT);
    auto unsupported =
        q::core::LocalSchema::create(std::array{record}, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(unsupported);
    refused = q::events::PreparedSpreadOrder::prepare(*unsupported, 0, MAPPING);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == q::events::MappingError::INCOMPATIBLE_TYPE);
}

TEST_CASE("AnalyticOrder optional defaults preserve inherited Order without slicing or publication guard") {
    static_assert(std::is_base_of_v<q::events::Order, q::events::AnalyticOrder>);
    auto schema = q::events::defaultAnalyticOrderSchema(SCHEMA);
    REQUIRE(schema);
    auto prepared = q::events::PreparedAnalyticOrder::prepare(*schema, 0, MAPPING);
    REQUIRE(prepared);
    auto v = values(*schema);
    field(*schema, v, u"MMID").raw = 0x41420043;
    field(*schema, v, u"Flags").raw = std::bit_cast<std::int32_t>(0x87654321U);
    auto result = prepared->map(u"A", {schema->recordContext(0), v}, {1, 3, 4});
    REQUIRE(result);
    CHECK(result->marketMaker == u"ABC");
    CHECK(result->flags == std::bit_cast<std::int32_t>(0x87654321U));
    CHECK(std::isnan(result->getIcebergPeakSize()));
    CHECK(std::isnan(result->getIcebergHiddenSize()));
    CHECK(std::isnan(result->getIcebergExecutedSize()));
    CHECK(result->icebergFlags == 0);
    CHECK(result->getIcebergType() == q::events::IcebergType::UNDEFINED);
    auto refused = prepared->map(u"A", {schema->recordContext(0), v}, {1, 3, 3});
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == q::events::MappingError::RESOURCE_LIMIT);
    auto custom = richAnalytic();
    prepared = q::events::PreparedAnalyticOrder::prepare(custom, 0, MAPPING);
    REQUIRE(prepared);
    CHECK(prepared->sourceId() == q::events::OrderSource::NTV.id());
    CHECK_FALSE(q::events::OrderSource::NTV.isPublishable(q::events::OrderEventType::ANALYTIC_ORDER));
}

TEST_CASE("Analytic iceberg and base direct numeric snapshots remain independent after public mutation") {
    auto schema = richAnalytic();
    auto prepared = q::events::PreparedAnalyticOrder::prepare(schema, 0, MAPPING);
    REQUIRE(prepared);
    auto v = values(schema);
    field(schema, v, u"Price").raw = q::core::WideDecimal{1725 * 256 + 130}.raw;
    field(schema, v, u"IcebergPeakSize").raw = q::core::WideDecimal{95 * 256 + 129}.raw;
    field(schema, v, u"IcebergHiddenSize").raw = q::core::WideDecimal{-25 * 256 + 129}.raw;
    field(schema, v, u"IcebergFlags").raw = std::bit_cast<std::int32_t>(0x87654323U);
    auto result = prepared->map(u"A", {schema.recordContext(0), v}, VALUES);
    REQUIRE(result);
    CHECK(result->getPrice() == 17.25);
    CHECK(result->getIcebergPeakSize() == 9.5);
    CHECK(result->getIcebergHiddenSize() == -2.5);
    CHECK(result->getIcebergType() == q::events::IcebergType::UNDEFINED);
    CHECK(result->icebergFlags == std::bit_cast<std::int32_t>(0x87654323U));
    result->icebergPeakSize = q::core::WideDecimal{22 * 256 + 128};
    CHECK(result->getIcebergPeakSize() == 22);
    CHECK(result->getPrice() == 17.25);
    result->price = q::core::WideDecimal{-3 * 256 + 128};
    CHECK(result->getPrice() == -3);
    CHECK(result->getIcebergHiddenSize() == -2.5);

    for (std::int32_t low = 0; low < 4; ++low) {
        result->icebergFlags = std::bit_cast<std::int32_t>(0xfffffff0U | low);
        CHECK(static_cast<int>(result->getIcebergType()) == (low == 3 ? 0 : low));
    }
}

#include "order_variants_fixture.hpp"

TEST_CASE("all production Java variant observations remain bounded and traceable") {
    using namespace order_variants_fixture;
    auto corpus = load(ORG_TTLDTOR_QDCXX_ORDER_VARIANTS_FIXTURE);
    CHECK(corpus.rows == 200862);
    std::size_t mapped = 0, rejected = 0, capabilities = 0, available = 0, setters = 0;

    for (const auto &[name, scenario] : corpus.scenarios) {
        INFO(name);
        auto schema = q::core::LocalSchema::create(scenario.records, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);

        for (std::size_t id = 0; id < scenario.records.size(); ++id) {
            INFO(id);
            const auto check = [&]<class Prepared>() {
                auto prepared = Prepared::prepare(*schema, static_cast<std::int32_t>(id), MAPPING);

                if (scenario.rejected.contains(id)) {
                    CHECK_FALSE(prepared);
                    ++rejected;

                    return;
                }

                REQUIRE(prepared);
                std::size_t count = 0;

                for (const auto &[key, sample] : scenario.samples) {
                    if (key.first != id) {
                        continue;
                    }

                    INFO(key.second);
                    auto event = prepared->map(
                        sample.symbol, {schema->recordContext(static_cast<std::int32_t>(id)), sample.values}, VALUES,
                        sample.fieldTime, static_cast<std::int32_t>(std::stoll(sample.getters.at("getEventFlags"))));
                    REQUIRE(event);
                    CHECK(matchesBase(*event, sample));
                    const auto low = sample.values.at(position(scenario.records[id], "Index")).raw;
                    // Removal collector time retains an independent high word; public index uses the low contributor.
                    CHECK(static_cast<std::uint32_t>(sample.collectorTime) == static_cast<std::uint32_t>(low));
                    ++count;
                    ++mapped;
                }

                CHECK(count == scenario.parsed.at(id));
            };

            if (scenario.records[id].name.starts_with(u"SpreadOrder")) {
                check.template operator()<q::events::PreparedSpreadOrder>();
            } else {
                check.template operator()<q::events::PreparedAnalyticOrder>();
            }
        }
    }

    for (const auto &row : corpus.metadata) {
        if (row[0] == "capability") {
            const auto source = q::events::OrderSource::valueOf(static_cast<std::int32_t>(std::stoll(row[2])));
            const auto type = row[3] == "Order"         ? q::events::OrderEventType::ORDER
                              : row[3] == "SpreadOrder" ? q::events::OrderEventType::SPREAD_ORDER
                                                        : q::events::OrderEventType::ANALYTIC_ORDER;
            CHECK(source.isPublishable(type) == (row[4] == "true"));
            CHECK(source.isFullOrderBook() == (row[5] == "true"));
            ++capabilities;
        } else if (row[0] == "icebergSetter") {
            q::events::AnalyticOrder value;
            value.icebergFlags = static_cast<std::int32_t>(std::stoll(row[3]));
            CHECK(std::to_underlying(value.getIcebergType()) == std::stoi(row[4]));
            CHECK((static_cast<std::uint32_t>(value.icebergFlags) & ~3U) ==
                  (static_cast<std::uint32_t>(std::stoll(row[1])) & ~3U));
            ++setters;
        } else {
            ++available;
        }
    }

    CHECK(mapped == 3504);
    CHECK(rejected == 52);
    CHECK(capabilities == 192);
    CHECK(available == 252);
    CHECK(setters == 24);
}

TEST_CASE("actual Spread and Analytic binary values and removal times use existing native decoder") {
    using namespace order_variants_fixture;
    const auto corpus = load(ORG_TTLDTOR_QDCXX_ORDER_VARIANTS_FIXTURE);
    std::size_t checked = 0, removals = 0;

    for (const auto &[name, scenario] : corpus.scenarios) {
        auto schema = q::core::LocalSchema::create(scenario.records, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);

        for (std::size_t id = 0; id < scenario.records.size(); ++id) {
            if (scenario.rejected.contains(id)) {
                continue;
            }

            INFO(name, id);
            const auto wirePath = std::filesystem::path(ORG_TTLDTOR_QDCXX_ORDER_VARIANTS_WIRE) /
                                  (name + "-" + std::to_string(id) + ".qtp");
            const auto wire = readWire(wirePath, &*schema);
            std::size_t sampleId = 0;

            for (const auto &record : wire.records) {
                const auto &sample = scenario.samples.at({id, sampleId++});
                REQUIRE(record.typedValues.size() == sample.values.size());

                for (std::size_t f = 0; f < sample.values.size(); ++f) {
                    INFO("sample=", sampleId - 1, " field=", f, " actual=", record.typedValues[f].raw,
                         " expected=", sample.values[f].raw);
                    CHECK(record.typedValues[f] == sample.values[f]);
                }

                CHECK(record.symbol.units == sample.symbol);

                if (record.removalTime) {
                    CHECK(*record.removalTime == sample.collectorTime);
                    ++removals;
                }

                const auto check = [&]<class Prepared>() {
                    auto prepared = Prepared::prepare(*schema, static_cast<std::int32_t>(id), MAPPING);
                    REQUIRE(prepared);
                    auto event =
                        prepared->map(record.symbol.units, {record.context, record.typedValues}, VALUES,
                                      record.fieldTime ? record.fieldTime->eventTimeMs() : 0, record.symbol.eventFlags);
                    REQUIRE(event);
                    CHECK(matchesBase(*event, sample));
                };

                if (scenario.records[id].name.starts_with(u"SpreadOrder")) {
                    check.template operator()<q::events::PreparedSpreadOrder>();
                } else {
                    check.template operator()<q::events::PreparedAnalyticOrder>();
                }

                ++checked;
            }

            CHECK(sampleId == scenario.parsed.at(id));
        }
    }

    CHECK(checked == 3504);
    CHECK(removals > 0);
}

TEST_CASE("nonzero signed FIELD precision survives actual binary and remains independent of business time") {
    using namespace order_variants_fixture;
    std::ifstream input(ORG_TTLDTOR_QDCXX_ORDER_VARIANTS_FIELD_FIXTURE, std::ios::binary);
    REQUIRE(input);
    std::map<std::pair<std::string, std::size_t>, std::vector<std::string>> expected;
    std::string line;
    std::size_t rows = 0;

    while (std::getline(input, line)) {
        REQUIRE(++rows <= 64);

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const auto row = cells(line);

        if (row[0] == "fieldTime" && row[1] == "on") {
            expected[{row[2], std::stoull(row[3])}] = row;
        }
    }

    REQUIRE(input.eof());
    CHECK(rows == 51);
    std::size_t checked = 0;

    for (const std::string file : {"with-time.qtp", "without-time.qtp"}) {
        const auto wire = readWire(std::filesystem::path(ORG_TTLDTOR_QDCXX_ORDER_VARIANTS_FIELD_WIRE) / file);
        REQUIRE(wire.schema);
        std::array<std::size_t, 2> samples{};

        for (const auto &record : wire.records) {
            const auto id = static_cast<std::size_t>(record.localId);
            REQUIRE(id < 2);
            const auto &name = wire.schema->records()[id].name;
            const std::string ascii = id == 0 ? "SpreadOrder" : "AnalyticOrder";
            CHECK(name == (id == 0 ? u"SpreadOrder" : u"AnalyticOrder"));
            const auto &row = expected.at({ascii, samples[id]++});
            const auto packed = file == "with-time.qtp" ? std::stoll(row[5]) : 0;
            const auto millis = file == "with-time.qtp" ? std::stoll(row[7]) : 0;
            CHECK((record.fieldTime ? record.fieldTime->packed() : 0) == packed);
            const auto check = [&]<class Prepared>() {
                auto prepared = Prepared::prepare(*wire.schema, static_cast<std::int32_t>(id), MAPPING);
                REQUIRE(prepared);
                auto event =
                    prepared->map(record.symbol.units, {record.context, record.typedValues}, VALUES,
                                  record.fieldTime ? record.fieldTime->eventTimeMs() : 0, record.symbol.eventFlags);
                REQUIRE(event);
                CHECK(event->eventTime == millis);
                CHECK(event->getTime() == 7123);
                CHECK(event->getSequence() == 17);
            };

            if (id == 0) {
                check.template operator()<q::events::PreparedSpreadOrder>();
            } else {
                check.template operator()<q::events::PreparedAnalyticOrder>();
            }

            ++checked;
        }
    }

    CHECK(checked == 32);
}
