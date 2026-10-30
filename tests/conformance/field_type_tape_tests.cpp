// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "qd/encoding/compact.hpp"
#include "qd/protocol/tape_reader.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace p = org::ttldtor::qdcxx::protocol;
std::filesystem::path fixtures; ///< Immutable Java artifact directory.

/** Construct finite allowances independent of observed output.
 * @return Bounded tape configuration with explicit field budgets.
 */
p::TapeLimits limits() {
    return {{8192, 8201},
            65536,
            128,
            {16, 8, {1024, 256}, 2048},
            {{8, 64, 128, 128, 8192}, 32, {256, 128, 8192}},
            100000,
            {8192, 128, 1024, 128, 2048, 4096, {8192, 128, 128}, {1024, 256}, 4096, 1024, 1024},
            128};
}

/** Construct an explicit caller projection, retaining all logical fields.
 * @param profile Select unchanged standard Profile instead of the synthetic witness.
 * @return Owned schema, without an event adapter or inferred wire schema.
 */
c::LocalSchema schema(bool profile = false) {
    c::LocalRecord record{0, profile ? u"Profile" : u"FieldTypes", false, {}, {}};
    const std::vector<std::pair<std::u16string, std::int32_t>> fields =
        profile
            ? std::vector<std::pair<std::u16string, std::int32_t>>{{u"Beta", 120},          {u"Eps", 120},
                                                                   {u"DivFreq", 120},       {u"ExdDivAmount", 120},
                                                                   {u"ExdDivDate", 88},     {u"HighPrice52", 120},
                                                                   {u"LowPrice52", 120},    {u"Shares", 120},
                                                                   {u"FreeFloat", 120},     {u"HighLimitPrice", 120},
                                                                   {u"LowLimitPrice", 120}, {u"HaltStartTime", 56},
                                                                   {u"HaltEndTime", 56},    {u"Flags", 8},
                                                                   {u"Description", 10},    {u"StatusReason", 10}}
            : std::vector<std::pair<std::u16string, std::int32_t>>{{u"Text", 10},    {u"Short", 40}, {u"Sequence", 72},
                                                                   {u"Date", 88},    {u"Count", 8},  {u"Char", 2},
                                                                   {u"Seconds", 56}, {u"Wide", 120}, {u"Tail", 10}};

    for (const auto &[name, type] : fields) {
        record.lookups.push_back({name, record.fields.size()});
        record.fields.push_back({type});
    }

    auto result =
        c::LocalSchema::create(std::vector{record}, limits().context.schema, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Read original binary evidence without transformation.
 * @param name Fixture stem.
 * @return Owned exact bytes.
 */
std::vector<std::byte> input(std::string_view name) {
    std::ifstream file(fixtures / (std::string(name) + ".qtp"), std::ios::binary);
    REQUIRE(file.good());
    const std::vector<char> chars((std::istreambuf_iterator<char>(file)), {});
    const auto bytes = std::as_bytes(std::span(chars));

    return {bytes.begin(), bytes.end()};
}

/** Parse a complete integral cell.
 * @tparam T Integral output type.
 * @param text Complete cell.
 * @param base Radix.
 * @return Exact value, or throw for malformed fixture text.
 */
template <typename T> T number(std::string_view text, int base = 10) {
    T value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, base);

    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        throw std::runtime_error("invalid integer");
    }

    return value;
}

/** Split TSV preserving empty columns.
 * @param line Complete row.
 * @return Ordered cells.
 */
std::vector<std::string> cells(const std::string &line) {
    std::vector<std::string> out;
    std::size_t start = 0;

    for (;;) {
        const auto end = line.find('\t', start);
        out.push_back(line.substr(start, end - start));

        if (end == std::string::npos) {
            return out;
        }

        start = end + 1;
    }
}

/** Decode exact hexadecimal UTF-16 fixture units.
 * @param text Four lowercase digits per unit or the empty sentinel.
 * @return Owned units, or throw for malformed evidence.
 */
std::u16string units(std::string_view text) {
    if (text == "-") {
        return {};
    }

    if (text.empty() || text.size() % 4 != 0 || text.find_first_not_of("0123456789abcdef") != text.npos) {
        throw std::runtime_error("invalid UTF-16");
    }

    std::u16string result;

    for (std::size_t i = 0; i < text.size(); i += 4) {
        result.push_back(static_cast<char16_t>(number<unsigned>(text.substr(i, 4), 16)));
    }

    return result;
}

/** One callback projected from immutable Java evidence. */
struct ExpectedRecord {
    std::int32_t message;              ///< Java callback kind, including RAW fanout.
    std::u16string symbol;             ///< Exact symbol units.
    std::int64_t time;                 ///< Java cursor sequence.
    std::vector<c::FieldValue> values; ///< All logical fields.
};

/** Strictly read complete Java callback evidence.
 * @param name Fixture stem and case identity.
 * @return STREAM callback projection in fixture order.
 */
std::vector<ExpectedRecord> expected(std::string_view name) {
    std::ifstream file(fixtures / (std::string(name) + ".tsv"));
    REQUIRE(file.good());
    std::vector<ExpectedRecord> all;
    std::string line;
    bool started = false;
    bool ended = false;
    bool complete = false;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const auto row = cells(line);
        REQUIRE_FALSE(complete);

        if (row[0] == "case") {
            REQUIRE(row.size() == 2);
            REQUIRE_FALSE(started);
            REQUIRE(row[1] == name);
            started = true;
        } else if (row[0] == "record") {
            REQUIRE(started);
            REQUIRE_FALSE(ended);
            REQUIRE(row.size() == 8);
            REQUIRE(number<std::size_t>(row[1]) == all.size());
            REQUIRE(number<unsigned>(row[6]) == 0);
            all.push_back({number<std::int32_t>(row[2]), units(row[5]), number<std::int64_t>(row[7]), {}});
        } else if (row[0] == "field") {
            REQUIRE_FALSE(ended);
            REQUIRE(row.size() == 7);
            REQUIRE_FALSE(all.empty());
            REQUIRE(number<std::size_t>(row[1]) == all.size() - 1);
            auto &values = all.back().values;
            REQUIRE(number<std::size_t>(row[2]) == values.size());
            c::FieldValue value{static_cast<c::SerialFieldType>(number<std::int32_t>(row[4])),
                                number<std::int64_t>(row[5])};

            if (row[4] == "10") {
                if (row[6] != "null") {
                    value.units = units(row[6]);
                }
            } else {
                REQUIRE(row[6] == "-");
            }

            REQUIRE(c::validFieldValue(value));
            values.push_back(std::move(value));
        } else if (row[0] == "end") {
            REQUIRE(row.size() == 3);
            REQUIRE_FALSE(ended);
            REQUIRE(number<std::size_t>(row[1]) == all.size());
            REQUIRE(row[2] == "0");
            ended = true;
        } else if (row[0] == "complete") {
            REQUIRE(row.size() == 2);
            REQUIRE(ended);
            REQUIRE(row[1] == name);
            complete = true;
        } else {
            FAIL("unexpected fixture row");
        }
    }

    REQUIRE(complete);
    std::vector<ExpectedRecord> projection;

    for (auto &record : all) {
        if (record.message == 15) {
            projection.push_back(std::move(record));
        }
    }

    return projection;
}

/** Owned tape observation surviving reader, schema and input destruction. */
struct Observation {
    std::vector<p::StreamDataRecord> records;         ///< Completed records.
    std::vector<std::optional<p::TapeTiming>> timing; ///< One copied timing per record.
    std::vector<std::size_t> recordFrames;            ///< Actual associated frame indices.
    std::vector<p::Frame> frames;                     ///< Owned envelopes and exact offsets.
    p::TapeStatus status{};                           ///< Terminal result.
    p::TapeDiagnostic error;                          ///< First error.
    std::size_t bytes{};                              ///< Charged field bytes.
    std::size_t units{};                              ///< Charged field units.
    std::size_t owned{};                              ///< Completed field units.
    std::size_t processed{};                          ///< Record attempts.
    std::size_t fields{};                             ///< Field attempts.
};

/** Feed a finite schedule and verify terminal behavior.
 * @param bytes Borrowed original or deliberately mutated tape.
 * @param profile Standard Profile projection selector.
 * @param raw RAW/MESSAGE policy selector.
 * @param chunks Repeated positive chunk-size schedule.
 * @param budget Explicit bounds.
 * @return Fully owned output observation.
 */
Observation run(std::span<const std::byte> bytes, bool profile, bool raw, std::span<const std::size_t> chunks,
                p::TapeLimits budget = limits()) {
    p::TapeReader reader(schema(profile), budget, raw ? p::TapeMode::RAW_MESSAGE_V1 : p::TapeMode::LEGACY);
    Observation result;
    std::size_t step = 0;

    while (!bytes.empty()) {
        const auto size = std::min(bytes.size(), chunks[step++ % chunks.size()]);
        auto event = reader.feed(bytes.first(size));
        CHECK(event.accepted <= size);
        bytes = bytes.subspan(event.accepted);

        if (event.frame) {
            CHECK(event.frame->startOffset == (result.frames.empty() ? 0 : result.frames.back().endOffset));
            CHECK(event.frame->endOffset == reader.acceptedBytes());

            for (auto &record : event.records) {
                result.records.push_back(std::move(record));
                result.timing.push_back(event.timing);
                result.recordFrames.push_back(result.frames.size());
            }

            result.frames.push_back(std::move(*event.frame));
        }

        if (event.status == p::TapeStatus::FAILED) {
            break;
        }

        REQUIRE(event.accepted > 0);
        CHECK(reader.feed({}).status == p::TapeStatus::NEED_INPUT);
    }

    result.status = reader.finish();
    result.error = reader.diagnostic();
    result.bytes = reader.fieldBytes();
    result.units = reader.fieldUnits();
    result.owned = reader.ownedFieldUnits();
    result.processed = reader.processedRecords();
    result.fields = reader.wireFields();
    CHECK(reader.finish() == result.status);
    CHECK(reader.feed(bytes).accepted == 0);
    CHECK(reader.feed(bytes).status == result.status);

    return result;
}

/** Construct an owned small-context test independent of retained caller schemas.
 * @param fields Ordered incoming string-field names, including skips/repeated targets.
 * @param unknown Make the incoming record unknown locally.
 * @return Validated owned immutable context.
 */
p::StreamDataContext context(std::vector<p::WireFieldDescription> fields, bool unknown = false) {
    const std::vector<c::LocalRecord> records{{0, u"R", false, {{10}, {10}}, {{u"x", 0}, {u"absent", 1}}}};
    auto local = c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(local);
    const std::vector<p::WireRecordDescription> descriptions{{0, unknown ? u"Unknown" : u"R", std::move(fields)}};
    auto result = p::StreamDataContext::create(*local, descriptions, limits().context);
    REQUIRE(result);

    return std::move(*result);
}

/** Materialize explicit test bytes without a production field writer.
 * @param values Exact manually reviewed byte values.
 * @return Owned bytes.
 */
std::vector<std::byte> bytes(std::initializer_list<unsigned> values) {
    std::vector<std::byte> result;

    for (const auto value : values) {
        result.push_back(static_cast<std::byte>(value));
    }

    return result;
}

/** Decode a fixture's hexadecimal wire bytes without using an encoder under test.
 * @param text Exact lowercase octets from the production oracle.
 * @return Owned fixture bytes.
 */
std::vector<std::byte> bytes(std::string_view text) {
    REQUIRE(text.size() % 2 == 0);
    REQUIRE(text.find_first_not_of("0123456789abcdef") == text.npos);
    std::vector<std::byte> result;

    for (std::size_t index = 0; index < text.size(); index += 2) {
        result.push_back(static_cast<std::byte>(number<unsigned>(text.substr(index, 2), 16)));
    }

    return result;
}

/** Construct an ordinary read-only binary projection with exact wire identities.
 * @param type Local Value identity, distinct from the production Java consumer storage type.
 * @param fields Explicit source declarations, including skips and repeated assignments.
 * @return Owned binding/default snapshot independent of the temporary schema and declarations.
 */
p::StreamDataContext scalarContext(std::int32_t type, std::vector<p::WireFieldDescription> fields) {
    const std::vector<c::LocalRecord> records{{0, u"Scalar", false, {{type}, {8}}, {{u"Value", 0}, {u"Sentinel", 1}}}};
    auto local = c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    const std::vector<p::WireRecordDescription> descriptions{{0, u"Scalar", std::move(fields)}};
    auto context = p::StreamDataContext::create(*local, descriptions, limits().context);
    REQUIRE(context);

    return std::move(*context);
}

/** Read one original scalar producer artifact.
 * @param directory Explicit immutable pin-specific fixture directory.
 * @param stem Fixture name excluding .qtp.
 * @return Owned unchanged bytes.
 */
std::vector<std::byte> scalarTapeInput(const std::filesystem::path &directory, const std::string &stem) {
    std::ifstream file(directory / (stem + ".qtp"), std::ios::binary);
    REQUIRE(file.good());
    const std::vector<char> chars((std::istreambuf_iterator<char>(file)), {});
    const auto source = std::as_bytes(std::span(chars));

    return {source.begin(), source.end()};
}
} // namespace

TEST_CASE("new explicit scalar schema reads original FileWriter and requested composer containers") {
    for (const auto &directory : {fixtures / "scalars", fixtures.parent_path() / "current-scalar-expected"}) {
        for (const auto type : {0, 1, 3, 4, 24, 104, 152}) {
            for (const bool raw : {false, true}) {
                CAPTURE(directory);
                CAPTURE(type);
                CAPTURE(raw);
                const std::string suffix = raw ? "-raw" : "";
                auto wire =
                    scalarTapeInput(directory, "file-" + std::to_string(type == 1 || type == 3 ? 4 : type) + suffix);

                if (type == 1 || type == 3) {
                    // BYTE/SHORT have no default fields: use an unchanged production header plus
                    // the actual requested-wire composer frames, not a claimed FileWriter capture.
                    const auto length = e::readCompactInt(wire);
                    REQUIRE(length.status == e::Status::OK);
                    REQUIRE(length.value > 0);
                    wire.resize(length.consumed + static_cast<std::size_t>(length.value));
                    const auto composed = scalarTapeInput(directory, "composer-" + std::to_string(type) + suffix);
                    wire.insert(wire.end(), composed.begin(), composed.end());
                }

                for (const auto chunk : {std::size_t{1}, std::size_t{7}, std::size_t{65536}}) {
                    std::vector<p::StreamDataRecord> records;
                    {
                        const std::vector<c::LocalRecord> input{
                            {0, u"Scalar", false, {{type}, {8}}, {{u"Value", 0}, {u"Sentinel", 1}}}};
                        auto schema =
                            c::LocalSchema::create(input, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
                        REQUIRE(schema);
                        p::TapeReader reader(*schema, limits(),
                                             raw ? p::TapeMode::RAW_MESSAGE_V1 : p::TapeMode::LEGACY);
                        auto remaining = std::span<const std::byte>(wire);

                        while (!remaining.empty()) {
                            const auto event = reader.feed(remaining.first(std::min(chunk, remaining.size())));
                            REQUIRE(event.status != p::TapeStatus::FAILED);
                            REQUIRE(event.accepted > 0);
                            remaining = remaining.subspan(event.accepted);
                            records.insert(records.end(), event.records.begin(), event.records.end());
                        }

                        CHECK(reader.finish() == p::TapeStatus::FINISHED);
                        CHECK(reader.wireFields() == (type == 0 ? 1 : 2));
                    }

                    REQUIRE(records.size() == 1);
                    const auto expectedRaw = type == 0                    ? 0
                                             : type == 104 || type == 152 ? std::numeric_limits<std::int64_t>::min()
                                                                          : -1;
                    CHECK(records[0].typedValues ==
                          std::vector<c::FieldValue>{{static_cast<c::SerialFieldType>(type), expectedRaw},
                                                     {c::SerialFieldType::COMPACT_INT, 913}});
                    CHECK(records[0].context.valid());
                }
            }
        }
    }
}

TEST_CASE("both pinned production scalar parsers match exact raw values truncations and examined positions") {
    for (const auto &directory : {fixtures / "scalars", fixtures.parent_path() / "current-scalar-expected"}) {
        std::ifstream input(directory / "samples.tsv");
        REQUIRE(input.good());
        std::string line;
        REQUIRE(std::getline(input, line));
        REQUIRE(line == "case\tscalar-samples");
        std::size_t samples = 0;
        bool complete = false;

        while (std::getline(input, line)) {
            REQUIRE_FALSE(complete);
            const auto row = cells(line);

            if (row[0] == "complete") {
                REQUIRE(row == std::vector<std::string>{"complete", "scalar-samples"});
                complete = true;

                continue;
            }

            if (row[0] != "sample") {
                continue;
            }

            REQUIRE(row.size() == 10);
            CAPTURE(directory);
            CAPTURE(row[1]);
            CAPTURE(row[2]);
            CAPTURE(row[3]);
            const auto type = number<std::int32_t>(row[2]);
            const auto context = scalarContext(type, {{u"Value", type}, {u"Sentinel", 8}});
            auto payload = bytes(row[4]);
            const bool raw = row[3] == "5";
            const auto result = raw ? p::decodeRawData(payload, context, limits().data)
                                    : p::decodeStreamData(payload, context, limits().data);
            CHECK(result.consumed == 3 + number<std::size_t>(row[9]));
            CHECK(result.fieldBytes == 0);
            CHECK(result.fieldUnits == 0);
            CHECK(result.ownedFieldUnits == 0);
            CHECK(result.processedRecords == 1);

            if (row[6] == "0") {
                // RAW's three Java callback families project onto one source wire record.
                CHECK(row[5] == (raw ? "3" : "1"));
                REQUIRE(result.status == p::StreamDataStatus::OK);
                REQUIRE(result.records.size() == 1);
                CHECK(result.consumed == payload.size());
                CHECK(result.wireFields == 2);
                std::fill(payload.begin(), payload.end(), std::byte{0xff});
                const auto &record = result.records[0];
                CHECK(record.values.empty());
                CHECK(record.typedValues ==
                      std::vector<c::FieldValue>{{static_cast<c::SerialFieldType>(type), number<std::int64_t>(row[7])},
                                                 {c::SerialFieldType::COMPACT_INT, number<std::int64_t>(row[8])}});
                CHECK(record.symbol.units == u"IBM");
                CHECK(record.senderId == 0);
                CHECK(record.context.valid());
            } else {
                CHECK(row[5] == "0");
                CHECK(row[6] == "1");
                CHECK(result.status == p::StreamDataStatus::TRUNCATED_PAYLOAD);
                CHECK(result.stage == p::StreamDataStage::FIELD);
                CHECK(result.records.empty());
                const bool sentinel = row[1].find("-sentinel-prefix-") != std::string::npos;
                const auto sentinelBytes = row[1].ends_with("-1") ? 1 : 0;
                CHECK(result.errorOffset == (sentinel ? payload.size() - sentinelBytes : 3));
                CHECK(result.wireFields == (sentinel ? 2 : 1));
            }

            ++samples;
        }

        CHECK(complete);
        CHECK(samples == 1106);
    }
}

TEST_CASE("zero-width VOID keeps explicit source bindings distinct from dense local defaults") {
    const auto declared = scalarContext(0, {{u"Sentinel", 8}, {u"Value", 0}, {u"skip", 0}, {u"Value", 0}});
    const auto absent = scalarContext(0, {{u"Sentinel", 8}});
    REQUIRE(declared.find(0));
    REQUIRE(absent.find(0));
    CHECK(declared.find(0)->binding.fields.size() == 4);
    CHECK(absent.find(0)->binding.fields.size() == 1);
    CHECK(declared.find(0)->binding.fields[1].targetFieldId == 0);
    CHECK_FALSE(declared.find(0)->binding.fields[2].targetFieldId);
    const auto payload = bytes({0x24, 0x4d, 0, 0x83, 0x91, 0x24, 0x4d, 0, 0x83, 0x91});

    for (const bool raw : {false, true}) {
        for (const auto allowance : {std::size_t{0}, std::size_t{3}, std::size_t{4}, std::size_t{5}}) {
            auto budget = limits().data;
            budget.wireFields = allowance;
            const auto first = std::span(payload).first(5);
            const auto result =
                raw ? p::decodeRawData(first, declared, budget) : p::decodeStreamData(first, declared, budget);
            CHECK(result.status == (allowance >= 4 ? p::StreamDataStatus::OK : p::StreamDataStatus::RESOURCE_LIMIT));
            CHECK(result.wireFields == std::min(allowance, std::size_t{4}));
            CHECK(result.consumed == (allowance == 0 ? 3 : 5));
        }

        for (const auto *context : {&declared, &absent}) {
            const auto decoded = raw ? p::decodeRawData(payload, *context, limits().data)
                                     : p::decodeStreamData(payload, *context, limits().data);
            REQUIRE(decoded.status == p::StreamDataStatus::OK);
            REQUIRE(decoded.records.size() == 2);
            CHECK(decoded.wireFields == (context == &declared ? 8 : 2));

            for (const auto &record : decoded.records) {
                CHECK(record.typedValues == std::vector<c::FieldValue>{{c::SerialFieldType::VOID_FIELD, 0},
                                                                       {c::SerialFieldType::COMPACT_INT, 913}});
            }
        }
    }
}

TEST_CASE("binary scalar skips replacements failure prefixes and finite budgets share STREAM RAW charging") {
    const auto context = scalarContext(1, {{u"Value", 1}, {u"skip", 3}, {u"Value", 1}, {u"Sentinel", 8}});
    const auto payload = bytes({0x24, 0x4d, 0, 0x80, 0x80, 0x00, 0x7f, 0x83, 0x91});

    for (const bool raw : {false, true}) {
        for (const auto allowance : {std::size_t{0}, payload.size() - 1, payload.size(), payload.size() + 1}) {
            auto budget = limits().data;
            budget.payloadBytes = allowance;
            const auto result =
                raw ? p::decodeRawData(payload, context, budget) : p::decodeStreamData(payload, context, budget);
            CHECK(result.status ==
                  (allowance >= payload.size() ? p::StreamDataStatus::OK : p::StreamDataStatus::RESOURCE_LIMIT));
            CHECK(result.processedRecords == (allowance >= payload.size() ? 1 : 0));
        }

        for (const auto allowance : {std::size_t{0}, std::size_t{1}, std::size_t{2}}) {
            for (const bool recordOutput : {false, true}) {
                auto budget = limits().data;

                if (recordOutput) {
                    budget.emittedRecords = allowance;
                } else {
                    budget.processedRecords = allowance;
                }

                const auto result =
                    raw ? p::decodeRawData(payload, context, budget) : p::decodeStreamData(payload, context, budget);
                CHECK(result.status ==
                      (allowance >= 1 ? p::StreamDataStatus::OK : p::StreamDataStatus::RESOURCE_LIMIT));
                CHECK(result.wireFields == (allowance >= 1 ? 4 : 0));
            }
        }

        for (const auto allowance : {std::size_t{0}, std::size_t{2}, std::size_t{3}, std::size_t{4}}) {
            auto budget = limits().data;
            budget.ownedSymbolUnits = allowance;
            const auto result =
                raw ? p::decodeRawData(payload, context, budget) : p::decodeStreamData(payload, context, budget);
            CHECK(result.status == (allowance >= 3 ? p::StreamDataStatus::OK : p::StreamDataStatus::RESOURCE_LIMIT));
            CHECK(result.wireFields == (allowance >= 3 ? 4 : 0));
        }

        for (const auto allowance : {std::size_t{0}, std::size_t{3}, std::size_t{4}, std::size_t{5}}) {
            auto budget = limits().data;
            budget.wireFields = allowance;
            const auto result =
                raw ? p::decodeRawData(payload, context, budget) : p::decodeStreamData(payload, context, budget);
            CHECK(result.wireFields == std::min(allowance, std::size_t{4}));

            if (allowance >= 4) {
                REQUIRE(result.status == p::StreamDataStatus::OK);
                CHECK(result.records[0].typedValues[0].raw == 127);
            } else {
                CHECK(result.status == p::StreamDataStatus::RESOURCE_LIMIT);
                CHECK(result.records.empty());
            }
        }

        for (const auto allowance : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{3}}) {
            auto budget = limits().data;
            budget.emittedValues = allowance;
            const auto result =
                raw ? p::decodeRawData(payload, context, budget) : p::decodeStreamData(payload, context, budget);
            CHECK(result.status == (allowance >= 2 ? p::StreamDataStatus::OK : p::StreamDataStatus::RESOURCE_LIMIT));
            CHECK(result.wireFields == (allowance >= 2 ? 4 : 0));
        }

        auto two = payload;
        two.insert(two.end(), payload.begin(), payload.begin() + 5);
        const auto failed =
            raw ? p::decodeRawData(two, context, limits().data) : p::decodeStreamData(two, context, limits().data);
        CHECK(failed.status == p::StreamDataStatus::TRUNCATED_PAYLOAD);
        CHECK(failed.records.size() == 1);
        CHECK(failed.processedRecords == 2);
        CHECK(failed.wireFields == 6);
        CHECK(failed.errorOffset == payload.size() + 4);
        CHECK(failed.consumed == two.size());
        const auto malformedSkip = bytes({0x24, 0x4d, 0, 0x7f, 0x80});
        const auto skipped = raw ? p::decodeRawData(malformedSkip, context, limits().data)
                                 : p::decodeStreamData(malformedSkip, context, limits().data);
        CHECK(skipped.status == p::StreamDataStatus::TRUNCATED_PAYLOAD);
        CHECK(skipped.wireFields == 2);
        CHECK(skipped.errorOffset == 4);
        CHECK(skipped.consumed == 5);
    }
}

TEST_CASE("original Java tapes retain all logical fields and raw timing under every chunk schedule") {
    for (const bool profile : {false, true}) {
        for (const bool raw : {false, true}) {
            const std::string name = std::string(profile ? "profile" : "mixed") + (raw ? "-raw" : "-stream");
            CAPTURE(name);
            const auto original = input(name);
            const auto oracle = expected(name);
            REQUIRE(oracle.size() == 3);

            for (const auto &schedule : std::vector<std::vector<std::size_t>>{{65536}, {1}, {7, 2, 19, 1, 5}}) {
                auto borrowed = original;
                const auto result = run(borrowed, profile, raw, schedule);
                std::fill(borrowed.begin(), borrowed.end(), std::byte{0xff});
                REQUIRE(result.status == p::TapeStatus::FINISHED);
                REQUIRE(result.records.size() == oracle.size());
                CHECK(result.processed == 3);
                CHECK(result.fields == (profile ? 48 : 27));
                CHECK(result.units == 10);
                CHECK(result.owned == 10);
                CHECK(result.bytes == 22);
                std::vector<std::size_t> dataFrames;

                for (std::size_t i = 0; i < result.frames.size(); ++i) {
                    if (result.frames[i].type == (raw ? 5 : 15)) {
                        dataFrames.push_back(i);
                    }
                }

                CHECK(dataFrames == (raw ? std::vector<std::size_t>{3, 5, 6} : std::vector<std::size_t>{2, 3, 4}));
                CHECK(result.recordFrames == dataFrames);

                for (std::size_t i = 0; i < oracle.size(); ++i) {
                    CHECK(result.records[i].localId == 0);
                    CHECK(result.records[i].values.empty());
                    CHECK(result.records[i].typedValues == oracle[i].values);
                    CHECK(result.records[i].symbol.units == oracle[i].symbol);
                    CHECK(result.records[i].symbol.eventFlags == 0);

                    if (raw) {
                        REQUIRE(result.timing[i]);
                        CHECK(result.timing[i]->eventTimeSequence == oracle[i].time);
                        CHECK(result.timing[i]->timestampMs == 1600000000123LL);

                        if (!profile) {
                            CHECK(result.records[i].typedValues[2].raw != result.timing[i]->eventTimeSequence);
                        }
                    } else {
                        CHECK_FALSE(result.timing[i]);
                    }
                }
            }
        }
    }
}

TEST_CASE("ordered replacements and skips reserve work and simultaneous owned units") {
    const auto bindings = context({{u"x", 10}, {u"skip", 10}, {u"x", 10}});
    const auto payload = bytes({0x24, 0x4d, 0, 2, 'a', 'b', 3, 's', 'k', 'p', 1, 'z'});

    for (const bool raw : {false, true}) {
        auto budget = limits().data;
        budget.ownedFieldUnits = 5;
        const auto result =
            raw ? p::decodeRawData(payload, bindings, budget) : p::decodeStreamData(payload, bindings, budget);
        REQUIRE(result.status == p::StreamDataStatus::OK);
        REQUIRE(result.records.size() == 1);
        CHECK(result.records[0].typedValues[0].units == u"z");
        CHECK_FALSE(result.records[0].typedValues[1].units);
        CHECK(result.fieldBytes == 9);
        CHECK(result.fieldUnits == 6);
        CHECK(result.ownedFieldUnits == 1);
        CHECK(result.wireFields == 3);
        budget.ownedFieldUnits = 4;
        const auto failed = p::decodeStreamData(payload, bindings, budget);
        CHECK(failed.status == p::StreamDataStatus::RESOURCE_LIMIT);
        CHECK(failed.errorOffset == 6);
        CHECK(failed.consumed == 7);
        CHECK(failed.fieldBytes == 4);
        CHECK(failed.fieldUnits == 2);
        CHECK(failed.ownedFieldUnits == 0);
        CHECK(failed.records.empty());
    }

    auto budget = limits().data;
    budget.ownedFieldUnits = 2;
    const auto replaced =
        p::decodeStreamData(bytes({0x24, 0x4d, 0, 2, 'a', 'b', 1, 'z'}), context({{u"x", 10}, {u"x", 10}}), budget);
    CHECK(replaced.status == p::StreamDataStatus::RESOURCE_LIMIT);
    CHECK(replaced.consumed == 7);
    CHECK(replaced.fieldUnits == 2);
    const auto unknown =
        p::decodeRawData(payload, context({{u"x", 10}, {u"skip", 10}, {u"x", 10}}, true), limits().data);
    CHECK(unknown.status == p::StreamDataStatus::OK);
    CHECK(unknown.records.empty());
    CHECK(unknown.fieldBytes == 9);
    CHECK(unknown.fieldUnits == 6);
    CHECK(unknown.ownedFieldUnits == 0);
}

TEST_CASE("failed current strings preserve complete prefixes and charge admitted work") {
    const auto bindings = context({{u"x", 10}});
    const auto payload = bytes({0x24, 0x4d, 0, 2, 'a', 'b', 0x24, 0x4d, 0, 2, 'x', 0xff});
    const auto result = p::decodeRawData(payload, bindings, limits().data);
    CHECK(result.status == p::StreamDataStatus::INVALID_FIELD);
    CHECK(result.stage == p::StreamDataStage::FIELD);
    CHECK(result.errorOffset == 9);
    CHECK(result.consumed == 12);
    CHECK(result.fieldBytes == 6);
    CHECK(result.fieldUnits == 4);
    CHECK(result.ownedFieldUnits == 2);
    REQUIRE(result.records.size() == 1);
    CHECK(result.records[0].typedValues[0].units == u"ab");
    const auto skipped =
        p::decodeStreamData(bytes({0x24, 0x4d, 0, 2, 'x', 0xff}), context({{u"skip", 10}}), limits().data);
    CHECK(skipped.status == p::StreamDataStatus::INVALID_FIELD);
    CHECK(skipped.fieldUnits == 2);
    CHECK(skipped.fieldBytes == 3);
    CHECK(skipped.records.empty());
}

TEST_CASE("per-value and aggregate byte and unit bounds preserve primitive-start positions") {
    const auto bindings = context({{u"x", 10}});
    const auto payload = bytes({0x24, 0x4d, 0, 2, 'a', 'b'});

    for (const bool aggregate : {false, true}) {
        auto budget = limits().data;
        (aggregate ? budget.fieldBytes : budget.fieldString.encodedBytes) = 2;
        auto result = p::decodeStreamData(payload, bindings, budget);
        CHECK(result.status == p::StreamDataStatus::RESOURCE_LIMIT);
        CHECK(result.errorOffset == 3);
        CHECK(result.consumed == 5);
        CHECK(result.fieldBytes == 2);
        CHECK(result.fieldUnits == 2);
        budget = limits().data;
        (aggregate ? budget.fieldUnits : budget.fieldString.codeUnits) = 1;
        result = p::decodeStreamData(payload, bindings, budget);
        CHECK(result.status == p::StreamDataStatus::RESOURCE_LIMIT);
        CHECK(result.consumed == 4);
        CHECK(result.fieldBytes == 1);
        CHECK(result.fieldUnits == 0);
    }

    auto budget = limits().data;
    budget.fieldString = {1, 0};
    budget.fieldUnits = 0;
    budget.ownedFieldUnits = 0;

    for (const auto prefix : {0U, 0x7fU}) {
        const auto result = p::decodeStreamData(bytes({0x24, 0x4d, 0, prefix}), bindings, budget);
        CHECK(result.status == p::StreamDataStatus::OK);
        CHECK(result.fieldBytes == 1);
        CHECK(result.fieldUnits == 0);
    }

    budget.fieldBytes = 0;
    const auto rejected = p::decodeStreamData(bytes({0x24, 0x4d, 0, 0x7f}), bindings, budget);
    CHECK(rejected.status == p::StreamDataStatus::RESOURCE_LIMIT);
    CHECK(rejected.consumed == 3);
    CHECK(rejected.fieldBytes == 0);
}

TEST_CASE("tape string work and output budgets remain cumulative across frames") {
    for (const bool raw : {false, true}) {
        const auto original = input(raw ? "mixed-raw" : "mixed-stream");

        for (const unsigned kind : {0U, 1U, 2U}) {
            auto budget = limits();

            if (kind == 0) {
                budget.data.fieldBytes = 8;
            } else if (kind == 1) {
                budget.data.fieldUnits = 9;
            } else {
                budget.data.ownedFieldUnits = 9;
            }

            const auto result = run(original, false, raw, std::array<std::size_t, 3>{1, 7, 3}, budget);
            CHECK(result.status == p::TapeStatus::FAILED);
            CHECK(result.error.error == p::TapeError::RESOURCE_LIMIT);
            CHECK(result.error.stage == "data");
            CHECK(result.records.size() == 2);
            CHECK(result.owned == 4);
            CHECK(result.units == 4);
            CHECK(result.bytes == (kind == 0 ? 8 : 9));
            CHECK(result.error.errorOffset == 21);
            CHECK(result.error.consumed == (kind == 0 ? 21 : 22));
        }
    }
}

TEST_CASE("malformed field suffix retains complete tape prefix and exact failing frame positions") {
    for (const bool raw : {false, true}) {
        auto original = input(raw ? "mixed-raw" : "mixed-stream");
        const auto baseline = run(original, false, raw, std::array<std::size_t, 1>{65536});
        REQUIRE(baseline.status == p::TapeStatus::FINISHED);
        const auto &frame = baseline.frames[raw ? 6 : 4];
        REQUIRE(frame.payload.size() == 35);
        // The final string unit is newline; corrupt it without changing the following empty Tail.
        original[static_cast<std::size_t>(frame.endOffset) - 2] = std::byte{0xff};

        for (const auto &schedule : std::vector<std::vector<std::size_t>>{{65536}, {1}, {2, 11, 3}}) {
            const auto result = run(original, false, raw, schedule);
            CHECK(result.status == p::TapeStatus::FAILED);
            CHECK(result.error.error == p::TapeError::MALFORMED);
            CHECK(result.error.stage == "data");
            CHECK(result.error.frameStart == frame.startOffset);
            CHECK(result.error.payloadStart == frame.endOffset - frame.payload.size());
            CHECK(result.error.errorOffset == 21);
            CHECK(result.error.consumed == 34);
            CHECK(result.processed == 3);
            CHECK(result.fields == 26);
            CHECK(result.bytes == 21);
            CHECK(result.units == 10);
            CHECK(result.owned == 4);
            REQUIRE(result.records.size() == 2);
            CHECK(result.records[0].typedValues == baseline.records[0].typedValues);
            CHECK(result.records[1].typedValues == baseline.records[1].typedValues);

            if (raw) {
                REQUIRE(result.timing.back());
                CHECK(result.timing.back()->timestampMs == 1600000000123LL);
            }
        }
    }
}

TEST_CASE("failed appended description leaves previous context and returned strings owned") {
    auto original = input("mixed-stream");
    p::TapeReader reader(schema(), limits());
    std::span<const std::byte> remaining(original);
    std::vector<p::StreamDataRecord> records;

    while (!remaining.empty()) {
        auto event = reader.feed(remaining);
        REQUIRE(event.status != p::TapeStatus::FAILED);
        REQUIRE(event.accepted > 0);
        remaining = remaining.subspan(event.accepted);

        for (auto &record : event.records) {
            records.push_back(std::move(record));
        }
    }

    const auto *snapshot = reader.context();
    REQUIRE(snapshot);
    REQUIRE(snapshot->find(0));
    // A fresh sender ID is structurally complete but its full type 11 is outside the selected union.
    const auto rejectedDescription = bytes({8, 2, 1, 1, 'R', 1, 1, 'x', 11});
    const auto rejected = reader.feed(rejectedDescription);
    CHECK(rejected.status == p::TapeStatus::FAILED);
    CHECK(reader.diagnostic().error == p::TapeError::UNSUPPORTED);
    CHECK(reader.context() == snapshot);
    CHECK(reader.context()->find(0) != nullptr);
    CHECK(reader.context()->find(1) == nullptr);
    CHECK(reader.fieldUnits() == 10);
    CHECK(reader.ownedFieldUnits() == 10);
    CHECK(reader.finish() == p::TapeStatus::FAILED);
    CHECK(reader.feed(original).accepted == 0);
    std::fill(original.begin(), original.end(), std::byte{0xff});
    REQUIRE(records.size() == 3);
    REQUIRE(records[2].typedValues[0].units);
    CHECK(*records[2].typedValues[0].units == std::u16string{u'A', 0, 0xd83d, 0xde00, 0xd800, u'\n'});
}

TEST_CASE("production parser fixtures preserve skips defaults repeated targets and wider scalar projections") {
    std::vector<std::string> names{"missing",       "mixed-order",   "repeat-0",       "repeat-1",
                                   "repeat-2",      "unknown-field", "unknown-record", "string-wire-0",
                                   "string-wire-1", "string-wire-2", "string-wire-3",  "string-wire-4",
                                   "string-wire-5", "string-wire-8", "string-wire-10"};

    for (const auto type : {40, 72, 88}) {
        for (const auto value : {"-9223372036854775808", "2147483648", "4294967295", "9223372036854775807"}) {
            names.push_back("wide-" + std::to_string(type) + "-" + value);
        }
    }

    for (const auto &name : names) {
        CAPTURE(name);
        const auto oracle = expected(name);
        const auto result = run(input(name), false, false, std::array<std::size_t, 3>{5, 1, 17});
        REQUIRE(result.status == p::TapeStatus::FINISHED);
        REQUIRE(result.records.size() == oracle.size());

        for (std::size_t i = 0; i < oracle.size(); ++i) {
            CHECK(result.records[i].symbol.units == oracle[i].symbol);
            CHECK(result.records[i].typedValues == oracle[i].values);
        }

        REQUIRE_FALSE(result.records.empty());
        CHECK(result.records.back().symbol.units == u"NEXT");
        CHECK(result.records.back().typedValues[4].raw == 913);
    }
}

TEST_CASE("malformed skipped strings are terminal although Java can recover at later frames") {
    for (const auto name : {"malformed-skipped", "malformed-unknown-record", "oversized-skipped", "string-wire-6",
                            "string-wire-7", "string-wire-9"}) {
        CAPTURE(name);
        const auto result = run(input(name), false, false, std::array<std::size_t, 3>{1, 13, 2});
        CHECK(result.status == p::TapeStatus::FAILED);
        CHECK(result.error.error == p::TapeError::MALFORMED);
        CHECK(result.error.stage == "data");
        CHECK(result.records.empty());
        CHECK(result.bytes > 0);
        CHECK(result.owned == 0);
    }

    for (const auto name : {"mismatch-int", "mismatch-object"}) {
        const auto result = run(input(name), false, false, std::array<std::size_t, 1>{65536});
        CHECK(result.status == p::TapeStatus::FAILED);
        CHECK(result.error.error == p::TapeError::UNSUPPORTED);
        CHECK(result.error.stage == "binding");
        CHECK(result.records.empty());
        CHECK(result.bytes == 0);
    }
}

TEST_CASE("owned direct results survive input and context destruction and readers isolate timing") {
    p::StreamDataResult output;
    {
        auto payload = bytes({0x24, 0x4d, 0, 2, 'a', 'b'});
        const auto bindings = context({{u"x", 10}});
        output = p::decodeStreamData(payload, bindings, limits().data);
        std::fill(payload.begin(), payload.end(), std::byte{0xff});
    }
    REQUIRE(output.status == p::StreamDataStatus::OK);
    REQUIRE(output.records.size() == 1);
    CHECK(output.records[0].typedValues[0].units == u"ab");
    const auto raw = run(input("mixed-raw"), false, true, std::array<std::size_t, 1>{65536});
    const auto untimed = run(input("mixed-stream"), false, false, std::array<std::size_t, 1>{1});
    REQUIRE(raw.records.size() == 3);
    REQUIRE(untimed.records.size() == 3);
    REQUIRE(raw.timing.back());
    CHECK_FALSE(untimed.timing.front());
    CHECK_FALSE(untimed.records[0].typedValues[0].units);
    CHECK(raw.records[2].typedValues[0].units == untimed.records[2].typedValues[0].units);
}

/** Run focused tests with an explicit immutable fixture directory.
 * @param argc Argument count.
 * @param argv Executable, fixture directory and optional doctest arguments.
 * @return Test failure count or usage error 2.
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
