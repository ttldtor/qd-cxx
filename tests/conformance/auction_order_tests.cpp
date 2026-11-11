// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "event_time_fixture.hpp"
#include "qd/events/order_imbalance.hpp"
#include "qd/events/otc_markets_order.hpp"
#include "qd/protocol/describe_records.hpp"
#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/stream_data.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <doctest/doctest.h>

namespace {
namespace q = org::ttldtor::qdcxx;
using event_time_fixture::cells;
using event_time_fixture::Sample;
using event_time_fixture::units;
using q::core::FieldValue;
using q::core::LocalRecord;
using q::core::LocalSchema;
using q::core::SerialFieldType;
using q::events::MappingError;
using q::events::OrderImbalance;
using q::events::OtcMarketsOrder;
using q::events::PreparedOrderImbalance;
using q::events::PreparedOtcMarketsOrder;

constexpr q::core::SchemaLimits SCHEMA{8, 256, 256, 128, 16384};
constexpr q::events::MappingLimits MAPPING{128, 256, 32, 64, 256, 128, 32768, 1000000};
constexpr q::events::EventValueLimits VALUES{128, 8, 136};
constexpr q::protocol::StreamDataLimits DATA_LIMITS{65536,         128,   8192, 128,  8192,  8192,  {65536, 128, 128},
                                                    {65536, 8192}, 65536, 8192, 8192, 65536, 65536, 65536,
                                                    65536,         65536, 128};
constexpr std::int32_t DESCRIBE_RECORDS = 2;
constexpr std::int32_t TICKER_DATA = 10;
constexpr std::int32_t HISTORY_DATA = 20;

struct Scenario {
    LocalRecord record{};
    std::map<std::size_t, Sample> samples;
    std::size_t parsed{};
    bool rejected{};
};

/** Read an exact ASCII Java record/key identifier into the native schema.
 * @param cell Pinned fixture identifier.
 * @return Owning UTF-16 equivalent.
 */
std::u16string key(const std::string &cell) {
    return {cell.begin(), cell.end()};
}

/** Read complete fixture observations without replacing any captured bytes.
 * @param path Explicit pinned TSV path.
 * @return Independently owned schema, physical-value and typed observations.
 */
std::map<std::string, Scenario> load(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input);
    std::map<std::string, Scenario> result;
    std::size_t lineCount = 0;
    std::string line;

    while (std::getline(input, line)) {
        REQUIRE(++lineCount <= 10000);
        REQUIRE(line.size() <= 8192);
        const auto row = cells(line);
        auto &scenario = result[row.at(1)];
        auto &record = scenario.record;

        if (row[0] == "record") {
            REQUIRE(record.name.empty());
            record.name = key(row.at(2));
            REQUIRE((row.at(3) == "history" || row.at(3) == "ticker"));
            record.hasTime = row[3] == "history";

            if (record.hasTime) {
                record.indexedTimeLayout = q::core::IndexedTimeLowInt32{0};
            }
        } else if (row[0] == "field") {
            const auto field = std::stoull(row.at(2));
            REQUIRE(field == record.fields.size());
            REQUIRE(field < 32);
            record.fields.push_back({std::stoi(row.at(3))});
            record.lookups.push_back({key(row.at(4)), field});

            if (row.at(4) != row.at(5)) {
                record.lookups.push_back({key(row.at(5)), field});
            }
        } else if (row[0] == "parsed") {
            scenario.parsed = std::stoull(row.at(2));
        } else if (row[0] == "reject") {
            scenario.rejected = true;
        } else {
            const auto sampleId = std::stoull(row.at(2));
            REQUIRE(sampleId < 64);
            auto &sample = scenario.samples[sampleId];

            if (row[0] == "symbol") {
                sample.symbol = units(row.at(3));
                sample.rawEventTime = std::stoll(row.at(4));
            } else if (row[0] == "value") {
                const auto field = std::stoull(row.at(3));
                REQUIRE(field == sample.values.size());
                sample.values.push_back(
                    {static_cast<SerialFieldType>(record.fields.at(field).serialTypeId), std::stoll(row.at(4))});
            } else {
                REQUIRE((row[0] == "otc" || row[0] == "imbalance"));
                sample.expected.assign(row.begin() + 3, row.end());
            }
        }
    }

    REQUIRE(input.eof());

    return result;
}

/** @param value One direct business binary64 value.
 * @return Java doubleToLongBits with canonical NaN.
 */
std::int64_t doubleBits(double value) noexcept {
    constexpr std::int64_t JAVA_NAN_BITS = 0x7ff8000000000000;

    return std::isnan(value) ? JAVA_NAN_BITS : std::bit_cast<std::int64_t>(value);
}

/** Compare all business and independent identity fields against the actual OTC delegate.
 * @param event Owning projected native event.
 * @param sample Exact Java values for the matching cursor.
 */
void compare(const OtcMarketsOrder &event, const Sample &sample) {
    const auto &order = event.order;
    const auto &row = sample.expected;
    REQUIRE(row.size() == 36);
    CHECK(order.symbol == units(row[0]));
    CHECK(order.eventTime == std::stoll(row[1]));
    CHECK(event.getIndex() == std::stoll(row[2]));
    CHECK(event.getSource().id() == std::stoi(row[3]));
    CHECK(order.eventFlags == std::stoi(row[4]));
    CHECK(order.timeSequence == std::stoll(row[5]));
    CHECK(order.getTime() == std::stoll(row[6]));
    CHECK(order.getTimeNanos() == std::stoll(row[7]));
    CHECK(order.getSequence() == std::stoi(row[8]));
    CHECK(order.timeNanoPart == std::stoi(row[9]));
    CHECK(order.actionTime == std::stoll(row[10]));
    CHECK(order.orderId == std::stoll(row[11]));
    CHECK(order.auxOrderId == std::stoll(row[12]));
    CHECK(order.getCount() == std::stoll(row[13]));
    CHECK(order.flags == std::stoi(row[14]));
    CHECK(order.tradeId == std::stoll(row[15]));

    if (row[16] == "null") {
        CHECK_FALSE(order.marketMaker);
    } else {
        REQUIRE(order.marketMaker);
        CHECK(*order.marketMaker == units(row[16]));
    }

    CHECK(order.getExchangeCode() == std::stoi(row[17]));
    CHECK(q::events::marketEnumCode(order.getOrderSide()) == std::stoi(row[18]));
    CHECK(q::events::marketEnumCode(order.getScope()) == std::stoi(row[19]));
    CHECK(q::events::marketEnumCode(order.getAction()) == std::stoi(row[20]));
    CHECK(order.getSize() == std::stoll(row[21]));
    CHECK(order.hasSize() == (row[22] == "true"));
    CHECK(doubleBits(order.getPrice()) == std::stoll(row[23]));
    CHECK(doubleBits(order.getSizeAsDouble()) == std::stoll(row[24]));
    CHECK(doubleBits(order.getExecutedSize()) == std::stoll(row[25]));
    CHECK(doubleBits(order.getTradePrice()) == std::stoll(row[26]));
    CHECK(doubleBits(order.getTradeSize()) == std::stoll(row[27]));
    CHECK(event.getQuoteAccessPayment() == std::stoi(row[28]));
    CHECK(event.otcMarketsFlags == std::stoi(row[29]));
    CHECK(event.isOpen() == (row[30] == "true"));
    CHECK(event.isUnsolicited() == (row[31] == "true"));
    CHECK(q::events::marketEnumCode(event.getOtcMarketsPriceType()) == std::stoi(row[32]));
    CHECK(event.isSaturated() == (row[33] == "true"));
    CHECK(event.isAutoExecution() == (row[34] == "true"));
    CHECK(event.isNmsConditional() == (row[35] == "true"));
}

/** Compare auction and independent identity fields against the actual imbalance delegate.
 * @param event Owning projected native event.
 * @param sample Exact Java values for the matching cursor.
 */
void compare(const OrderImbalance &event, const Sample &sample) {
    const auto &row = sample.expected;
    REQUIRE(row.size() == 14);
    CHECK(event.symbol == units(row[0]));
    CHECK(event.eventTime == std::stoll(row[1]));
    CHECK(event.getSource().id() == std::stoi(row[2]));
    CHECK(event.timeSequence == std::stoll(row[3]));
    CHECK(event.getTime() == std::stoll(row[4]));
    CHECK(event.getSequence() == std::stoi(row[5]));
    CHECK(doubleBits(event.getRefPrice()) == std::stoll(row[6]));
    CHECK(doubleBits(event.getPairedSize()) == std::stoll(row[7]));
    CHECK(doubleBits(event.getImbalanceSize()) == std::stoll(row[8]));
    CHECK(doubleBits(event.getNearPrice()) == std::stoll(row[9]));
    CHECK(doubleBits(event.getFarPrice()) == std::stoll(row[10]));
    CHECK(event.flags == std::stoi(row[11]));
    CHECK(q::events::marketEnumCode(event.getImbalanceSide()) == std::stoi(row[12]));
    CHECK(q::events::marketEnumCode(event.getAuctionType()) == std::stoi(row[13]));
}

/** Decode the original Java QTP wire and retain independently owned native record values.
 * @param file Exact checked-in QTP bytes from the scenario.
 * @param schema Schema built from generated source types, names and aliases.
 * @param hasTime Select HISTORY instead of the TICKER contract.
 * @return Owned native decoded records in wire order.
 */
std::vector<q::protocol::StreamDataRecord> decode(const std::filesystem::path &file, const LocalSchema &schema,
                                                  bool hasTime) {
    REQUIRE(std::filesystem::file_size(file) <= 65536);
    std::ifstream input(file, std::ios::binary);
    REQUIRE(input);
    const std::vector<char> chars((std::istreambuf_iterator<char>(input)), {});
    const auto bytes = std::as_bytes(std::span(chars));
    q::protocol::FrameReader reader({65536, 65545});
    std::optional<q::protocol::StreamDataContext> context;
    std::vector<q::protocol::WireRecordDescription> descriptions;
    std::vector<q::protocol::StreamDataRecord> result;
    std::size_t offset = 0;

    while (offset < bytes.size()) {
        auto frame = reader.feed(bytes.subspan(offset));
        REQUIRE(frame.accepted != 0);
        offset += frame.accepted;
        REQUIRE(frame.status != q::protocol::FrameStatus::FAILED);

        if (!frame.frame) {
            continue;
        }

        if (frame.frame->type == DESCRIBE_RECORDS) {
            auto parsed =
                q::protocol::parseDescribeRecords(frame.frame->payload, {65536, 16, 128, 128, {65536, 8192}, 65536});
            REQUIRE(parsed.status == q::protocol::DescriptionStatus::OK);
            descriptions.insert(descriptions.end(), parsed.descriptions.begin(), parsed.descriptions.end());
            auto bound = q::protocol::StreamDataContext::create(schema, descriptions, {SCHEMA, 16, {256, 128, 65536}});
            REQUIRE(bound);
            context.emplace(std::move(*bound));
        } else {
            REQUIRE(frame.frame->type == (hasTime ? HISTORY_DATA : TICKER_DATA));
            REQUIRE(context);
            auto decoded = q::protocol::decodeStreamDataWithEventPolicy(
                frame.frame->payload, *context, DATA_LIMITS, q::protocol::DataEventPolicy::BINARY_CATALOG_V1);
            REQUIRE(decoded.status == q::protocol::StreamDataStatus::OK);
            result.insert(result.end(), std::make_move_iterator(decoded.records.begin()),
                          std::make_move_iterator(decoded.records.end()));
        }
    }

    CHECK(reader.finish() == q::protocol::FrameStatus::FINISHED);

    return result;
}

/** @param schema Exact source shape for record zero.
 * @return Complete native scalar defaults in local field order.
 */
std::vector<FieldValue> defaultValues(const LocalSchema &schema) {
    std::vector<FieldValue> values;

    for (const auto &field : schema.records()[0].fields) {
        values.push_back({static_cast<SerialFieldType>(field.serialTypeId), 0});
    }

    return values;
}
} // namespace

TEST_CASE("original Java binary QTP produces distinct typed OTC and auction observations") {
    const auto fixture = std::filesystem::path(ORG_TTLDTOR_QDCXX_AUCTION_ORDER_FIXTURE);
    std::size_t projected = 0;
    std::size_t rejected = 0;

    for (const auto &[name, scenario] : load(fixture)) {
        INFO(name);
        auto schema =
            LocalSchema::create(std::span(&scenario.record, 1), SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);

        if (scenario.rejected) {
            if (scenario.record.hasTime) {
                const auto result = PreparedOtcMarketsOrder::prepare(*schema, 0, MAPPING);
                REQUIRE_FALSE(result);
                CHECK(result.error() == MappingError::MISSING_REQUIRED);
            } else {
                const auto result = PreparedOrderImbalance::prepare(*schema, 0, MAPPING);
                REQUIRE_FALSE(result);
                CHECK(result.error() == MappingError::MISSING_REQUIRED);
            }

            ++rejected;
            continue;
        }

        const auto records = decode(fixture.parent_path() / "wire" / (name + ".qtp"), *schema, scenario.record.hasTime);
        REQUIRE(records.size() == scenario.parsed);
        REQUIRE(records.size() == scenario.samples.size());

        for (std::size_t i = 0; i < records.size(); ++i) {
            INFO("sample=", i);
            const auto &record = records[i];
            const auto &sample = scenario.samples.at(i);
            REQUIRE(record.typedValues.size() == sample.values.size());
            CHECK(record.symbol.units == sample.symbol);

            for (std::size_t j = 0; j < sample.values.size(); ++j) {
                CHECK(record.typedValues[j].type == sample.values[j].type);
                CHECK(record.typedValues[j].raw == sample.values[j].raw);
            }

            if (scenario.record.hasTime) {
                const auto prepared = PreparedOtcMarketsOrder::prepare(*schema, 0, MAPPING);
                REQUIRE(prepared);
                const auto event = prepared->map(record.symbol.units, {record.context, record.typedValues}, VALUES, 0,
                                                 record.symbol.eventFlags);
                REQUIRE(event);
                compare(*event, sample);
            } else {
                const auto prepared = PreparedOrderImbalance::prepare(*schema, 0, MAPPING);
                REQUIRE(prepared);
                const auto event = prepared->map(record.symbol.units, {record.context, record.typedValues}, VALUES);
                REQUIRE(event);
                compare(*event, sample);
            }

            ++projected;
        }
    }

    CHECK(projected == 400);
    CHECK(rejected == 6);
}

TEST_CASE("default source schemas are exact and typed mappers reject foreign families") {
    const auto otc = q::events::defaultOtcMarketsOrderSchema(SCHEMA);
    const auto imbalance = q::events::defaultOrderImbalanceSchema(SCHEMA);
    REQUIRE(otc);
    REQUIRE(imbalance);
    REQUIRE(otc->records().size() == 2);
    REQUIRE(imbalance->records().size() == 4);
    const std::array schemas{&*otc, &*imbalance};
    const auto fixture = std::filesystem::path(ORG_TTLDTOR_QDCXX_AUCTION_ORDER_FIXTURE);
    const auto scenarios = load(fixture);

    for (const auto schema : schemas) {
        for (const auto &record : schema->records()) {
            const auto match = std::ranges::find_if(scenarios, [&](const auto &entry) {
                return entry.first.starts_with("default-") && entry.second.record.name == record.name;
            });
            REQUIRE(match != scenarios.end());
            const auto &expected = match->second.record;
            CHECK(record.name == expected.name);
            CHECK(record.hasTime == expected.hasTime);
            REQUIRE(record.fields.size() == expected.fields.size());
            REQUIRE(record.lookups.size() == expected.lookups.size());

            for (std::size_t i = 0; i < record.fields.size(); ++i) {
                CHECK(record.fields[i].serialTypeId == expected.fields[i].serialTypeId);
            }

            for (std::size_t i = 0; i < record.lookups.size(); ++i) {
                CHECK(record.lookups[i].name == expected.lookups[i].name);
                CHECK(record.lookups[i].fieldId == expected.lookups[i].fieldId);
            }
        }
    }

    const auto wrongImbalance = PreparedOrderImbalance::prepare(*otc, 0, MAPPING);
    const auto wrongOtc = PreparedOtcMarketsOrder::prepare(*imbalance, 0, MAPPING);
    REQUIRE_FALSE(wrongImbalance);
    REQUIRE_FALSE(wrongOtc);
    CHECK(wrongImbalance.error() == MappingError::INVALID_INPUT);
    CHECK(wrongOtc.error() == MappingError::INVALID_INPUT);

    auto malformed = imbalance->records()[0];
    malformed.name = u"OrderImbalance#X#A";
    const auto invalidSchema =
        LocalSchema::create(std::span(&malformed, 1), SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(invalidSchema);
    const auto invalidName = PreparedOrderImbalance::prepare(*invalidSchema, 0, MAPPING);
    REQUIRE_FALSE(invalidName);
    CHECK(invalidName.error() == MappingError::INVALID_INPUT);
}

TEST_CASE("borrowed mapper results own their symbol and preserve independent FIELD time") {
    const auto otc = q::events::defaultOtcMarketsOrderSchema(SCHEMA);
    const auto imbalance = q::events::defaultOrderImbalanceSchema(SCHEMA);
    REQUIRE(otc);
    REQUIRE(imbalance);
    const auto order = PreparedOtcMarketsOrder::prepare(*otc, 0, MAPPING);
    const auto auction = PreparedOrderImbalance::prepare(*imbalance, 0, MAPPING);
    REQUIRE(order);
    REQUIRE(auction);
    std::vector<q::core::FieldValueView> orderFields;
    std::vector<q::core::FieldValueView> imbalanceFields;

    for (const auto &field : defaultValues(*otc)) {
        orderFields.push_back({field.type, field.raw, {}, {}});
    }

    for (const auto &field : defaultValues(*imbalance)) {
        imbalanceFields.push_back({field.type, field.raw, {}, {}});
    }

    std::u16string symbol = u"AAPL&Q";
    constexpr std::int64_t FIELD_TIME = -123;
    const auto mappedOrder = order->mapView(symbol, {order->context(), orderFields}, {6, 0, 6}, FIELD_TIME, 9);
    const auto mappedImbalance = auction->mapView(symbol, {auction->context(), imbalanceFields}, {6, 0, 6}, FIELD_TIME);
    REQUIRE(mappedOrder);
    REQUIRE(mappedImbalance);
    const auto smallTotal = order->mapView(symbol, {order->context(), orderFields}, {6, 0, 5});
    const auto smallSymbol = auction->mapView(symbol, {auction->context(), imbalanceFields}, {5, 0, 6});
    const auto wrongOrderContext = order->mapView(symbol, {auction->context(), orderFields}, VALUES);
    const auto wrongImbalanceContext = auction->mapView(symbol, {order->context(), imbalanceFields}, VALUES);
    REQUIRE_FALSE(smallTotal);
    REQUIRE_FALSE(smallSymbol);
    REQUIRE_FALSE(wrongOrderContext);
    REQUIRE_FALSE(wrongImbalanceContext);
    CHECK(smallTotal.error() == MappingError::RESOURCE_LIMIT);
    CHECK(smallSymbol.error() == MappingError::RESOURCE_LIMIT);
    CHECK(wrongOrderContext.error() == MappingError::WRONG_CONTEXT);
    CHECK(wrongImbalanceContext.error() == MappingError::WRONG_CONTEXT);
    symbol.clear();
    orderFields.clear();
    imbalanceFields.clear();
    CHECK(mappedOrder->order.symbol == u"AAPL&Q");
    CHECK(mappedOrder->order.eventFlags == 9);
    CHECK(mappedOrder->order.eventTime == FIELD_TIME);
    CHECK(mappedOrder->order.getTime() == 0);
    CHECK(mappedImbalance->symbol == u"AAPL&Q");
    CHECK(mappedImbalance->eventTime == FIELD_TIME);
    CHECK(mappedImbalance->getTime() == 0);
}

TEST_CASE("typed mapping preparation accounts for all exact property type alternatives") {
    const auto otc = q::events::defaultOtcMarketsOrderSchema(SCHEMA);
    const auto imbalance = q::events::defaultOrderImbalanceSchema(SCHEMA);
    REQUIRE(otc);
    REQUIRE(imbalance);
    auto otcBound = MAPPING;
    constexpr std::size_t OTC_TYPE_ALTERNATIVES = 126;
    otcBound.typeAlternatives = OTC_TYPE_ALTERNATIVES;
    CHECK(PreparedOtcMarketsOrder::prepare(*otc, 0, otcBound));
    --otcBound.typeAlternatives;
    const auto otcRefused = PreparedOtcMarketsOrder::prepare(*otc, 0, otcBound);
    REQUIRE_FALSE(otcRefused);
    CHECK(otcRefused.error() == MappingError::RESOURCE_LIMIT);

    auto imbalanceBound = MAPPING;
    constexpr std::size_t IMBALANCE_TYPE_ALTERNATIVES = 63;
    imbalanceBound.typeAlternatives = IMBALANCE_TYPE_ALTERNATIVES;
    CHECK(PreparedOrderImbalance::prepare(*imbalance, 0, imbalanceBound));
    --imbalanceBound.typeAlternatives;
    const auto imbalanceRefused = PreparedOrderImbalance::prepare(*imbalance, 0, imbalanceBound);
    REQUIRE_FALSE(imbalanceRefused);
    CHECK(imbalanceRefused.error() == MappingError::RESOURCE_LIMIT);
}
