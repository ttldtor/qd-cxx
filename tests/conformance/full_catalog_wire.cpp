// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/protocol/describe_records.hpp"
#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/subscription_writer.hpp"
#include "qd/schema/json_schema.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace p = q::protocol;
using q::core::SerialFieldType;

constexpr auto SENDER_ID = 911;
constexpr auto SYMBOL = u"IBM";
constexpr auto REMOVE_EVENT = 2;
constexpr auto REMOVAL_TIME = std::bit_cast<std::int64_t>(std::uint64_t{0x81234567fedcba98});
constexpr std::size_t FRAME_BYTES = 65536;
constexpr std::size_t FIELDS = 256;
constexpr p::FrameLimits FRAME_LIMITS{FRAME_BYTES, FRAME_BYTES + 9};
constexpr q::core::SchemaLimits SCHEMA_LIMITS{1024, 16384, 32768, 1024, 1048576};
constexpr p::StreamDataLimits DATA_LIMITS{FRAME_BYTES,
                                          1,
                                          FIELDS,
                                          1,
                                          FIELDS,
                                          FIELDS,
                                          {FRAME_BYTES, FIELDS, FIELDS},
                                          {FRAME_BYTES, FIELDS},
                                          FRAME_BYTES,
                                          FRAME_BYTES,
                                          FRAME_BYTES,
                                          FRAME_BYTES,
                                          FRAME_BYTES,
                                          FRAME_BYTES,
                                          FRAME_BYTES,
                                          FRAME_BYTES};
constexpr std::array OPAQUE{std::byte{0xff}, std::byte{0x42}};
// QTP Penta cipher for IBM followed by compact integer 911.
constexpr std::array DATA_PREFIX{std::byte{0x24}, std::byte{0x4d}, std::byte{0x83}, std::byte{0x8f}};
// A fresh flag state sends F9 followed by the compact REMOVE_EVENT value.
constexpr std::array REMOVE_PREFIX{std::byte{0xf9}, std::byte{REMOVE_EVENT}, DATA_PREFIX[0],
                                   DATA_PREFIX[1],  DATA_PREFIX[2],          DATA_PREFIX[3]};

enum class Scenario { DEFAULT, BOUNDARY, EMPTY, CONTENT };

void require(bool condition, const char *diagnostic) {
    if (!condition) {
        throw std::runtime_error(diagnostic);
    }
}

Scenario parseScenario(std::string_view name) {
    if (name == "default") {
        return Scenario::DEFAULT;
    }

    if (name == "boundary") {
        return Scenario::BOUNDARY;
    }

    if (name == "empty") {
        return Scenario::EMPTY;
    }

    require(name == "content", "Unknown witness scenario");

    return Scenario::CONTENT;
}

q::core::FieldValueView witness(SerialFieldType type, Scenario scenario) {
    q::core::FieldValueView value{type, 0, {}, {}};

    if (scenario == Scenario::DEFAULT) {
        return value;
    }

    switch (type) {
    case SerialFieldType::VOID_FIELD:
        break;
    case SerialFieldType::UTF_CHAR:
        value.raw = 0x0416;
        break;
    case SerialFieldType::LONG:
    case SerialFieldType::WIDE_DECIMAL:
    case SerialFieldType::TIME_MILLIS:
        value.raw = (std::numeric_limits<std::int64_t>::min)() + 37;
        break;
    case SerialFieldType::UTF_CHAR_ARRAY:

        if (scenario == Scenario::EMPTY) {
            value.units = u"";
        } else if (scenario == Scenario::CONTENT) {
            value.units = u"\u0416\U0001f680";
        }

        break;
    case SerialFieldType::SERIAL_OBJECT:

        if (scenario == Scenario::EMPTY) {
            value.bytes = std::span<const std::byte>{};
        } else if (scenario == Scenario::CONTENT) {
            value.bytes = OPAQUE;
        }

        break;
    default:
        value.raw = (std::numeric_limits<std::int32_t>::min)() + 37;
        break;
    }

    return value;
}

std::string hexBody(std::span<const std::byte> payload, std::span<const std::byte> prefix) {
    require(payload.size() >= prefix.size() && std::ranges::equal(payload.first(prefix.size()), prefix),
            "Invalid symbol, flags or sender identity");
    const auto body = payload.subspan(prefix.size());

    if (body.empty()) {
        return "_";
    }

    constexpr std::string_view digits = "0123456789abcdef";
    std::string output;

    for (const auto byte : body) {
        const auto octet = std::to_integer<unsigned>(byte);
        output += digits[octet >> 4];
        output += digits[octet & 15];
    }

    return output;
}

void checkValues(std::span<const q::core::FieldValueView> expected, const p::StreamDataResult &decoded,
                 std::int32_t localId) {
    require(decoded.status == p::StreamDataStatus::OK && decoded.records.size() == 1, "DATA decode failed");
    const auto &row = decoded.records.front();
    require(row.localId == localId && row.senderId == SENDER_ID && row.symbol.units == SYMBOL,
            "Decoded record identity differs");
    require(row.typedValues.size() == expected.size(), "Decoded logical field count differs");

    for (std::size_t slot = 0; slot < expected.size(); ++slot) {
        const auto &actual = row.typedValues[slot];
        const auto &value = expected[slot];
        require(actual.type == value.type && actual.raw == value.raw && actual.units == value.units,
                "Decoded field type or value differs");
        require(actual.bytes.has_value() == value.bytes.has_value() &&
                    (!actual.bytes || std::ranges::equal(*actual.bytes, *value.bytes)),
                "Decoded opaque value differs");
    }
}

std::string checkRemoval(p::SubscriptionWriter &writer, p::SubscriptionWriterWork &work,
                         const p::StreamDataContext &context, std::string_view javaBody) {
    const p::LiveDataRecordView row{SENDER_ID, SYMBOL, {}, REMOVE_EVENT, REMOVAL_TIME};
    const auto encoded = writer.composeData(p::LiveDataMessage::HISTORY_DATA, std::span(&row, 1), true, work);
    require(encoded.has_value(), "REMOVE_EVENT encode failed");
    p::FrameReader reader(FRAME_LIMITS);
    const auto result = reader.feed(encoded->bytes());
    require(result.frame && result.frame->type == static_cast<int>(p::LiveDataMessage::HISTORY_DATA),
            "Invalid HISTORY_DATA frame");
    const auto payload = std::span(result.frame->payload);
    const auto nativeBody = hexBody(payload, REMOVE_PREFIX);
    require(nativeBody == javaBody, "REMOVE_EVENT differs from Java");
    const auto decoded =
        p::decodeStreamDataWithEventPolicy(payload, context, DATA_LIMITS, p::DataEventPolicy::BINARY_CATALOG_V1);
    require(decoded.status == p::StreamDataStatus::OK && decoded.records.size() == 1 &&
                decoded.records.front().removalTime == REMOVAL_TIME,
            "Decoded removal time differs");

    return nativeBody;
}

void checkRecord(const q::core::LocalSchema &schema, const q::core::LocalRecord &record, std::string_view fixture,
                 Scenario scenario, std::ostream &native) {
    std::istringstream input{std::string(fixture)};
    std::string name;
    std::string javaBody;
    std::string javaRemoval;
    require(static_cast<bool>(input >> name >> javaBody >> javaRemoval), "Incomplete oracle row");
    require(std::u16string(name.begin(), name.end()) == record.name, "Oracle record name differs");
    require((javaRemoval != "_") == (scenario == Scenario::DEFAULT && record.hasTime),
            "Oracle removal coverage differs");
    p::OutgoingRecord layout{SENDER_ID, record.id, record.name, {}};
    std::vector<q::core::FieldValueView> values;

    for (const auto &field : record.fields) {
        std::string property;
        require(static_cast<bool>(input >> property), "Oracle field name missing");
        const auto slot = static_cast<int>(values.size());
        layout.fields.push_back({std::u16string(property.begin(), property.end()), field.serialTypeId, slot});
        values.push_back(witness(static_cast<SerialFieldType>(field.serialTypeId), scenario));
    }

    std::string extra;
    require(!(input >> extra), "Unexpected oracle field");
    p::SubscriptionWriterWork work{10000, 10000000, 10000000, 10000000};
    auto writer = p::SubscriptionWriter::create(
        schema, std::span(&layout, 1), {SCHEMA_LIMITS, 1024, 16384, 1024, 1048576},
        {FRAME_BYTES, FRAME_BYTES + 9, 1024, FIELDS, {8192, 1024}, 131072, 131072, 8, true}, work);
    require(writer.has_value(), "Outgoing binding failed");
    const auto described = writer->describe(std::span(&layout.senderId, 1), work);
    require(described.has_value(), "DESCRIBE_RECORDS encode failed");
    p::FrameReader descriptionReader(FRAME_LIMITS);
    const auto description = descriptionReader.feed(described->bytes());
    require(description.frame && description.frame->type == 2, "Invalid DESCRIBE_RECORDS frame");
    const auto parsed = p::parseDescribeRecords(description.frame->payload,
                                                {FRAME_BYTES, 1, FIELDS, FIELDS, {8192, 1024}, FRAME_BYTES});
    require(parsed.status == p::DescriptionStatus::OK && parsed.descriptions.size() == 1,
            "DESCRIBE_RECORDS decode failed");
    const auto &advertised = parsed.descriptions.front().fields;
    std::size_t emitted = 0;

    for (const auto &field : layout.fields) {
        if (field.serialTypeId != 0) {
            require(emitted < advertised.size() && advertised[emitted].name == field.wireName &&
                        advertised[emitted].serialTypeId == field.serialTypeId,
                    "Advertised field differs");
            ++emitted;
        }
    }

    require(emitted == advertised.size(), "Unexpected advertised field");
    const auto context =
        p::StreamDataContext::create(schema, parsed.descriptions, {SCHEMA_LIMITS, 1, {FIELDS, 1024, FRAME_BYTES}});
    require(context.has_value(), "Incoming binding failed");

    const p::LiveDataRecordView row{SENDER_ID, SYMBOL, values};
    const auto encoded = writer->composeData(p::LiveDataMessage::STREAM_DATA, std::span(&row, 1), false, work);
    require(encoded.has_value(), "DATA encode failed");
    p::FrameReader dataReader(FRAME_LIMITS);
    const auto data = dataReader.feed(encoded->bytes());
    require(data.frame && data.frame->type == static_cast<int>(p::LiveDataMessage::STREAM_DATA),
            "Invalid STREAM_DATA frame");
    const auto nativeBody = hexBody(data.frame->payload, DATA_PREFIX);
    require(nativeBody == javaBody, "DATA body differs from Java");
    checkValues(values, p::decodeStreamData(data.frame->payload, *context, DATA_LIMITS), record.id);
    const auto removal = javaRemoval == "_" ? "_" : checkRemoval(*writer, work, *context, javaRemoval);
    native << name << '\t' << nativeBody << '\t' << removal << '\n';
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 5) {
        std::cerr << "Usage: full-catalog-wire schema.json oracle.tsv scenario native.tsv\n";

        return 2;
    }

    std::string line;
    try {
        const auto scenario = parseScenario(argv[3]);
        std::ifstream schemaFile(argv[1], std::ios::binary);
        std::ifstream oracle(argv[2]);
        std::ofstream native(argv[4], std::ios::binary);
        require(schemaFile && oracle && native, "Cannot open input or output");
        const std::string document{std::istreambuf_iterator<char>(schemaFile), {}};
        auto schema = q::schema::loadBinaryJsonSchema(document);
        require(schema.has_value(), "Cannot load binary catalog");

        for (const auto &record : schema->records()) {
            require(static_cast<bool>(std::getline(oracle, line)), "Oracle catalog ended early");
            checkRecord(*schema, record, line, scenario, native);
        }

        require(!std::getline(oracle, line), "Oracle catalog has extra rows");
        native.close();
        require(static_cast<bool>(native), "Cannot write native bodies");
        std::cout << "Checked " << schema->records().size() << " records\n";
    } catch (const std::exception &failure) {
        std::cerr << line.substr(0, line.find('\t')) << ": " << failure.what() << '\n';

        return 1;
    }
}
