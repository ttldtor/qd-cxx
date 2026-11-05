// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/event_flags.hpp"
#include "qd/events/order.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <doctest/doctest.h>
#include <limits>

namespace {
namespace q = org::ttldtor::qdcxx;
constexpr q::core::SchemaLimits SCHEMA{4, 128, 256, 128, 16384};
constexpr q::events::MappingLimits MAPPING{128, 256, 32, 64, 256, 128, 32768, 1000000};
constexpr q::events::EventValueLimits VALUES{128, 8, 136};

/** @param schema Exact validated source.
 * @return Complete canonical defaults in the source's local field order.
 */
std::vector<q::core::FieldValue> values(const q::core::LocalSchema &schema) {
    std::vector<q::core::FieldValue> result;

    for (const auto &field : schema.records()[0].fields) {
        result.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});
    }

    return result;
}

/** @param schema Source declarations with exact lookup keys.
 * @param fields Complete owning values to modify.
 * @param key Required local key.
 * @param raw New canonical scalar representation.
 */
void set(const q::core::LocalSchema &schema, std::vector<q::core::FieldValue> &fields, std::u16string_view key,
         std::int64_t raw) {
    for (const auto &lookup : schema.records()[0].lookups) {
        if (lookup.name == key) {
            fields[lookup.fieldId].raw = raw;

            return;
        }
    }

    FAIL("Required test key absent");
}
} // namespace

TEST_CASE("standard Order source index and complete owning fields remain independent") {
    auto schema = q::events::defaultOrderSchema(SCHEMA);
    REQUIRE(schema);
    REQUIRE(schema->records()[0].hasTime);
    REQUIRE(std::holds_alternative<q::core::IndexedTimeLowInt32>(*schema->records()[0].indexedTimeLayout));
    auto prepared = q::events::PreparedOrder::prepare(*schema, 0, MAPPING);
    REQUIRE(prepared);
    auto fields = values(*schema);
    set(*schema, fields, u"Time", -1);
    set(*schema, fields, u"Sequence", -1);
    set(*schema, fields, u"Flags", std::bit_cast<std::int32_t>(0x87654321U));
    set(*schema, fields, u"MMID", 0x41424344);

    for (auto index : std::array<std::int32_t, 4>{0, -1, std::numeric_limits<std::int32_t>::min(),
                                                  std::numeric_limits<std::int32_t>::max()}) {
        set(*schema, fields, u"Index", index);
        auto event = prepared->map(u"AAPL", {schema->recordContext(0), fields}, VALUES, 42, 3);
        REQUIRE(event);
        CHECK(event->index == static_cast<std::uint32_t>(index));
        CHECK(event->getSourceId() == 0);
        CHECK(event->eventTime == 42);
        CHECK(event->eventFlags == 3);
        CHECK(event->timeSequence == -1);
        CHECK(event->getTime() == 23);
        CHECK(event->getSequence() == 4194303);
        CHECK(event->flags == std::bit_cast<std::int32_t>(0x87654321U));
        CHECK(event->marketMaker == std::optional<std::u16string>{u"ABCD"});
        CHECK(event->getOrderSide() == q::events::Side::UNDEFINED);
        CHECK(event->getScope() == q::events::Scope::REGIONAL);
    }

    auto other = q::events::defaultOrderSchema(SCHEMA);
    REQUIRE(other);
    auto wrong = prepared->map(u"AAPL", {other->recordContext(0), fields}, VALUES);
    REQUIRE_FALSE(wrong);
    CHECK(wrong.error() == q::events::MappingError::WRONG_CONTEXT);
    auto tiny = prepared->map(u"AAPL", {schema->recordContext(0), fields}, {3, 8, 136});
    REQUIRE_FALSE(tiny);
    CHECK(tiny.error() == q::events::MappingError::RESOURCE_LIMIT);
}

TEST_CASE("Order native low layout requires an in-range exact COMPACT_INT contributor") {
    const q::core::LocalRecord original{0, u"Order", true, {{8}}, {{u"Index", 0}}, q::core::IndexedTimeLowInt32{0}};
    auto wrongType = original;
    wrongType.fields[0].serialTypeId = 4;
    auto result = q::core::LocalSchema::create(std::array{wrongType}, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE_FALSE(result);
    CHECK(result.error() == q::core::SchemaError::UNSUPPORTED_PROFILE);
    auto badIndex = original;
    badIndex.indexedTimeLayout = q::core::IndexedTimeLowInt32{1};
    result = q::core::LocalSchema::create(std::array{badIndex}, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE_FALSE(result);
    CHECK(result.error() == q::core::SchemaError::INVALID_SCHEMA);
}

#include "order_fixture.hpp"
#include "qd/protocol/describe_records.hpp"
#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/stream_data.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>

TEST_CASE("Order complete public fields match actual standard factory and production parser delegates") {
    const auto corpus = order_fixture::load(ORG_TTLDTOR_QDCXX_ORDER_FIXTURE);
    std::size_t checked = 0;
    std::size_t refused = 0;

    for (const auto &[name, scenario] : corpus) {
        INFO("scenario=", name);
        auto schema = q::core::LocalSchema::create(scenario.records, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);

        if (!schema) {
            CHECK(scenario.rejected.size() == scenario.records.size());
            refused += scenario.rejected.size();
            continue;
        }

        for (std::size_t id = 0; id < scenario.records.size(); ++id) {
            auto prepared = q::events::PreparedOrder::prepare(*schema, static_cast<std::int32_t>(id), MAPPING);
            const bool rejected = std::ranges::find(scenario.rejected, id) != scenario.rejected.end();
            CHECK(bool(prepared) != rejected);

            if (!prepared) {
                ++refused;
                continue;
            }

            for (const auto &[identity, sample] : scenario.samples) {
                if (identity.first != id) {
                    continue;
                }

                INFO("record=", id, " sample=", identity.second);
                auto event =
                    prepared->map(sample.symbol, {schema->recordContext(static_cast<std::int32_t>(id)), sample.values},
                                  VALUES, std::stoll(sample.expected.at(1)), std::stoi(sample.expected.at(4)));
                REQUIRE(event);
                CHECK(order_fixture::matches(*event, sample));
                ++checked;
            }
        }
    }

    CHECK(checked > 1000);
    CHECK(refused == 18);
}

TEST_CASE("actual QD Order binary physical VOID and low index bind without a synthetic logical high field") {
    const auto corpus = order_fixture::load(ORG_TTLDTOR_QDCXX_ORDER_FIXTURE);
    std::size_t decodedCount = 0;

    for (const auto &[name, scenario] : corpus) {
        if (!scenario.rejected.empty()) {
            continue;
        }

        auto schema = q::core::LocalSchema::create(scenario.records, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);

        for (std::size_t id = 0; id < scenario.records.size(); ++id) {
            INFO("wire scenario=", name, " record=", id);
            const auto path = std::filesystem::path(ORG_TTLDTOR_QDCXX_ORDER_FIXTURE).parent_path() / "wire" /
                              (name + "-" + std::to_string(id) + ".qtp");
            std::ifstream input(path, std::ios::binary);
            REQUIRE(input);
            const std::vector<char> chars((std::istreambuf_iterator<char>(input)), {});
            const auto bytes = std::as_bytes(std::span(chars));
            q::protocol::FrameReader reader({65536, 65545});
            std::vector<q::protocol::WireRecordDescription> descriptions;
            std::optional<q::protocol::StreamDataContext> context;
            std::size_t offset = 0;
            std::size_t sampleId = 0;
            auto prepared = q::events::PreparedOrder::prepare(*schema, static_cast<std::int32_t>(id), MAPPING);
            REQUIRE(prepared);

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
                } else if (frame.frame->type == 20) {
                    REQUIRE(context);
                    q::protocol::StreamDataLimits limits{
                        65536, 128,   8192,  128,   8192,  8192, {65536, 128, 128}, {65536, 8192}, 65536, 8192, 8192,
                        65536, 65536, 65536, 65536, 65536, 128};
                    auto decoded = q::protocol::decodeStreamDataWithEventPolicy(
                        frame.frame->payload, *context, limits, q::protocol::DataEventPolicy::BINARY_CATALOG_V1);
                    INFO("decode stage=", static_cast<int>(decoded.stage), " offset=", decoded.errorOffset);
                    REQUIRE(decoded.status == q::protocol::StreamDataStatus::OK);

                    for (const auto &record : decoded.records) {
                        const auto &sample = scenario.samples.at({id, sampleId++});
                        REQUIRE(record.typedValues.size() == sample.values.size());

                        for (std::size_t f = 0; f < sample.values.size(); ++f) {
                            CHECK(record.typedValues[f].raw == sample.values[f].raw);
                        }

                        auto event = prepared->map(record.symbol.units, {record.context, record.typedValues}, VALUES, 0,
                                                   record.symbol.eventFlags);
                        REQUIRE(event);
                        CHECK(order_fixture::matches(*event, sample));

                        if (record.removalTime) {
                            CHECK(*record.removalTime == std::stoll(sample.expected.at(33)));
                        }

                        ++decodedCount;
                    }
                } else {
                    FAIL("Unexpected actual Order composer frame");
                }
            }

            CHECK(reader.finish() == q::protocol::FrameStatus::FINISHED);
            CHECK(sampleId > 0);
        }
    }

    CHECK(decodedCount > 1000);
}
