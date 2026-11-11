// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "derived_order_fixture.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/protocol/describe_records.hpp"
#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/stream_data.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <doctest/doctest.h>
#include <iostream>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace f = derived_fixture;
constexpr q::core::SchemaLimits SCHEMA{4, 128, 256, 128, 32768};
constexpr q::events::MappingLimits MAPPING{128, 256, 32, 64, 256, 128, 32768, 1000000};
constexpr q::events::EventValueLimits VALUES{128, 8, 136};

/** @param value Native source double. @return Exact signed IEEE representation. */
bool sameDouble(double value, const std::string &expected) {
    const auto reference = std::stoll(expected);

    return std::bit_cast<std::int64_t>(value) == reference ||
           (std::isnan(value) && std::isnan(std::bit_cast<double>(reference)));
}

/** @param text Exact nullable production units. @return Independent nullable text. */
std::optional<std::u16string> nullable(const std::string &text) {
    return text == "null" ? std::nullopt : std::optional{f::units(text)};
}

/** Compare every actual derived delegate result, including full indexed/time identities.
 * @param event Native owning result. @param row Original generated delegate cells.
 */
void order(const q::events::Order &event, const std::vector<std::string> &row) {
    CHECK(event.symbol == f::units(row.at(6)));
    CHECK(event.eventTime == std::stoll(row.at(7)));
    CHECK(event.getIndex() == std::stoll(row.at(8)));
    CHECK(event.getSourceId() == std::stoi(row.at(9)));
    CHECK(event.eventFlags == std::stoi(row.at(10)));
    CHECK(event.timeSequence == std::stoll(row.at(11)));
    CHECK(event.getTime() == std::stoll(row.at(12)));
    CHECK(event.getSequence() == std::stoi(row.at(13)));
    CHECK(event.timeNanoPart == std::stoi(row.at(14)));
    CHECK(event.getExchangeCode() == std::stoi(row.at(15)));
    CHECK(event.marketMaker == nullable(row.at(16)));
    CHECK(std::to_underlying(event.getOrderSide()) == std::stoi(row.at(17)));
    CHECK(std::to_underlying(event.getScope()) == std::stoi(row.at(18)));
    CHECK(sameDouble(event.getPrice(), row.at(19)));
    CHECK(sameDouble(event.getSizeAsDouble(), row.at(20)));
    CHECK(event.getCount() == std::stoll(row.at(21)));
}

/** Compare the complete raw MarketMaker DTO and direct numeric versus wide accessors.
 * @param event Native owning raw DTO. @param row Original generated raw delegate cells.
 */
void maker(const q::events::MarketMaker &event, const std::vector<std::string> &row) {
    CHECK(event.symbol == f::units(row.at(5)));
    CHECK(event.eventTime == std::stoll(row.at(6)));
    CHECK(event.getSourceId() == std::stoi(row.at(7)));
    CHECK(event.getIndex() == std::stoll(row.at(8)));
    CHECK(event.eventFlags == std::stoi(row.at(9)));
    CHECK(event.getExchangeCode() == std::stoi(row.at(10)));
    CHECK(event.getMarketMaker() == nullable(row.at(11)));
    CHECK(event.getBidTime() == std::stoll(row.at(12)));
    CHECK(event.getAskTime() == std::stoll(row.at(13)));
    CHECK(event.getTime() == std::stoll(row.at(14)));
    CHECK(sameDouble(event.getBidPrice(), row.at(15)));
    CHECK(sameDouble(event.getAskPrice(), row.at(16)));
    CHECK(sameDouble(event.getBidSize(), row.at(17)));
    CHECK(sameDouble(event.getAskSize(), row.at(18)));
    CHECK(event.getBidCount() == std::stoll(row.at(19)));
    CHECK(event.getAskCount() == std::stoll(row.at(20)));
    CHECK(event.bidPrice.raw == std::stoll(row.at(21)));
    CHECK(event.bidSize.raw == std::stoll(row.at(22)));
    CHECK(event.askPrice.raw == std::stoll(row.at(23)));
    CHECK(event.askSize.raw == std::stoll(row.at(24)));
}

/** Project and compare a complete native cursor against the actual production observation.
 * @param schema Exact immutable original declarations. @param id Physical record identity.
 * @param sample Original/parser expected outputs. @param values Native original or parser values.
 * @param flags Native original or parser flags. @param rawTime Native exact original/parser indexed time.
 */
std::size_t compare(const q::core::LocalSchema &schema, std::size_t id, const f::Sample &sample,
                    std::span<const q::core::FieldValue> values, std::int32_t flags,
                    std::optional<std::int64_t> rawTime) {
    const bool mm = schema.records()[id].name == u"MarketMaker";
    auto derived = q::events::PreparedDerivedOrder::prepare(
        schema, static_cast<std::int32_t>(id),
        mm ? q::events::DerivedOrderKind::MARKET_MAKER : q::events::DerivedOrderKind::QUOTE, MAPPING);
    REQUIRE(derived);
    std::size_t checked = 0;
    const auto removal = mm && (flags & q::events::indexed_event_flags::REMOVE_EVENT) ? rawTime : std::nullopt;

    for (const auto &[role, row] : sample.orders) {
        auto event = derived->map(sample.symbol, {schema.recordContext(static_cast<std::int32_t>(id)), values}, role,
                                  VALUES, 0, flags, removal);
        REQUIRE(event);
        order(*event, row);
        ++checked;
    }

    for (const auto role : sample.refused) {
        auto event = derived->map(sample.symbol, {schema.recordContext(static_cast<std::int32_t>(id)), values}, role,
                                  VALUES, 0, flags, removal);
        CHECK_FALSE(event);
        ++checked;
    }

    if (mm) {
        auto prepared = q::events::PreparedMarketMaker::prepare(schema, static_cast<std::int32_t>(id), MAPPING);
        REQUIRE(prepared);
        auto event = prepared->map(sample.symbol, {schema.recordContext(static_cast<std::int32_t>(id)), values}, VALUES,
                                   0, flags, removal);
        REQUIRE(event);
        maker(*event, sample.maker);
        ++checked;
    }

    return checked;
}
} // namespace

TEST_CASE("actual full-type MarketMaker and derived Order original plus parsed cursors match") {
    const auto corpus = f::load(ORG_TTLDTOR_QDCXX_DERIVED_ORDER_FIXTURE);
    std::size_t checked = 0, refused = 0;

    for (const auto &[name, scenario] : corpus) {
        INFO(name);
        auto schema = q::core::LocalSchema::create(scenario.records, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);

        for (std::size_t id = 0; id < scenario.records.size(); ++id) {
            if (!scenario.prepareRefused[id]) {
                continue;
            }

            auto mapping = q::events::PreparedDerivedOrder::prepare(
                *schema, static_cast<std::int32_t>(id),
                id == 2 ? q::events::DerivedOrderKind::MARKET_MAKER : q::events::DerivedOrderKind::QUOTE, MAPPING);
            CHECK_FALSE(mapping);
            ++refused;
        }

        for (const auto &[identity, sample] : scenario.samples) {
            const auto &[id, phase, sampleId] = identity;
            INFO("id=", id, " phase=", phase, " sample=", sampleId);
            checked += compare(*schema, id, sample, sample.values, sample.flags, sample.rawTime);
        }
    }

    CHECK(checked > 10000);
    CHECK(refused > 0);
    std::cout << "DERIVED_ORACLE results=" << checked << " preparationRefusals=" << refused << '\n';
}

TEST_CASE("actual generated QTP bytes bind original physical time slots and feed the same mappers") {
    const auto corpus = f::load(ORG_TTLDTOR_QDCXX_DERIVED_ORDER_FIXTURE);
    std::size_t checked = 0, records = 0;

    for (const auto &[name, scenario] : corpus) {
        auto schema = q::core::LocalSchema::create(scenario.records, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);

        for (std::size_t id = 0; id < scenario.records.size(); ++id) {
            if (scenario.prepareRefused[id]) {
                continue;
            }

            INFO(name, " id=", id);
            const auto path = std::filesystem::path(ORG_TTLDTOR_QDCXX_DERIVED_ORDER_FIXTURE).parent_path() / "wire" /
                              (name + "-" + std::to_string(id) + ".qtp");
            std::ifstream input(path, std::ios::binary);
            REQUIRE(input);
            const std::vector<char> chars((std::istreambuf_iterator<char>(input)), {});
            const auto bytes = std::as_bytes(std::span(chars));
            q::protocol::FrameReader reader({65536, 65545});
            std::vector<q::protocol::WireRecordDescription> descriptions;
            std::optional<q::protocol::StreamDataContext> context;
            std::size_t offset = 0, sampleId = 0;

            while (offset < bytes.size()) {
                auto frame = reader.feed(bytes.subspan(offset));
                offset += frame.accepted;
                REQUIRE(frame.status != q::protocol::FrameStatus::FAILED);

                if (!frame.frame) {
                    continue;
                }

                if (frame.frame->type == 2) {
                    auto parsed = q::protocol::parseDescribeRecords(frame.frame->payload,
                                                                    {65536, 16, 128, 128, {65536, 8192}, 65536});
                    REQUIRE(parsed.status == q::protocol::DescriptionStatus::OK);
                    descriptions.insert(descriptions.end(), parsed.descriptions.begin(), parsed.descriptions.end());
                    auto bound =
                        q::protocol::StreamDataContext::create(*schema, descriptions, {SCHEMA, 16, {256, 128, 65536}});
                    REQUIRE(bound);
                    context.emplace(std::move(*bound));
                } else if (frame.frame->type == 10 || frame.frame->type == 20) {
                    REQUIRE(context);
                    q::protocol::StreamDataLimits limits{
                        65536, 128,   8192,  128,   8192,  8192, {65536, 128, 128}, {65536, 8192}, 65536, 8192, 8192,
                        65536, 65536, 65536, 65536, 65536, 128};
                    auto decoded = q::protocol::decodeStreamDataWithEventPolicy(
                        frame.frame->payload, *context, limits, q::protocol::DataEventPolicy::BINARY_CATALOG_V1);
                    INFO("decodeStage=", static_cast<int>(decoded.stage), " offset=", decoded.errorOffset);
                    REQUIRE(decoded.status == q::protocol::StreamDataStatus::OK);

                    for (const auto &record : decoded.records) {
                        const auto &sample = scenario.samples.at({id, "WIRE", sampleId++});
                        REQUIRE(record.typedValues.size() == sample.values.size());

                        for (std::size_t field = 0; field < sample.values.size(); ++field) {
                            CHECK(record.typedValues[field].raw == sample.values[field].raw);
                            CHECK(record.typedValues[field].type == sample.values[field].type);
                        }

                        CHECK(record.symbol.units == sample.symbol);
                        CHECK(record.symbol.eventFlags == sample.flags);

                        if (record.removalTime) {
                            CHECK(*record.removalTime == sample.rawTime.value_or(0));
                        }

                        checked += compare(*schema, id, sample, record.typedValues, record.symbol.eventFlags,
                                           record.removalTime);
                        ++records;
                    }
                } else {
                    FAIL("Unexpected actual composer frame");
                }
            }

            CHECK(reader.finish() == q::protocol::FrameStatus::FINISHED);
            CHECK(sampleId > 0);
        }
    }

    CHECK(records > 1000);
    CHECK(checked > 10000);
    std::cout << "DERIVED_QTP records=" << records << " results=" << checked << '\n';
}
