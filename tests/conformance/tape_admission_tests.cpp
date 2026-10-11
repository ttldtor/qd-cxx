// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#define DOCTEST_CONFIG_IMPLEMENT
#include "qd/protocol/tape_admission.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <string>
#include <type_traits>

namespace allocation_test {
thread_local bool enabled = false;      ///< Only the actual admission call arms allocation failures.
thread_local std::size_t remaining = 0; ///< Number of successful ordinary allocations before failure.

/** Allocate ordinary storage through a call-scoped fault boundary.
 * @param size Requested ordinary bytes.
 * @return Owned memory or throw bad_alloc.
 */
void *allocate(std::size_t size) {
    if (enabled) {
        if (remaining == 0) {
            throw std::bad_alloc();
        }

        --remaining;
    }

    if (auto *value = std::malloc(size == 0 ? 1 : size)) {
        return value;
    }

    throw std::bad_alloc();
}
} // namespace allocation_test

/** Replace ordinary allocation only for narrowly armed library calls.
 * @param size Requested bytes.
 * @return Owned ordinary storage.
 */
void *operator new(std::size_t size) {
    return allocation_test::allocate(size);
}

/** Replace array allocation through the same narrowly armed boundary.
 * @param size Requested bytes.
 * @return Owned array storage.
 */
void *operator new[](std::size_t size) {
    return allocation_test::allocate(size);
}

/** Release test ordinary storage.
 * @param value Allocated memory or null.
 */
void operator delete(void *value) noexcept {
    std::free(value);
}

/** Release test array storage.
 * @param value Allocated memory or null.
 */
void operator delete[](void *value) noexcept {
    std::free(value);
}

/** Release sized ordinary storage.
 * @param value Allocated memory or null.
 * @param size Supplied allocation size.
 */
void operator delete(void *value, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(value);
}

/** Release sized array storage.
 * @param value Allocated memory or null.
 * @param size Supplied allocation size.
 */
void operator delete[](void *value, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(value);
}

namespace {
namespace p = org::ttldtor::qdcxx::protocol;
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
std::filesystem::path fixtures;      ///< Explicit immutable contract packet location.
std::filesystem::path emitDirectory; ///< Optional synthetic description-heavy output directory.

/** Supply finite generous native test bounds.
 * @return Complete independent preflight configuration.
 */
p::AdmissionLimits limits() {
    c::SchemaLimits schema{256, 4096, 8192, 1024, 1048576};
    p::TapeLimits reader{{1048576, 1048585},
                         16777216,
                         65536,
                         {1024, 32, {1048576, 1024}, 1048576},
                         {schema, 1024, {16384, 1024, 1048576}},
                         67108864,
                         {1048576,
                          1048576,
                          8388608,
                          1048576,
                          8388608,
                          4194304,
                          {1048576, 16384, 16384},
                          {1048576, 16384},
                          16777216,
                          4194304,
                          4194304},
                         65536};

    return {16777216, reader, {schema, 1024, 16384, 1024, 1048576}, 67108864, 16777216, 4194304};
}

/** Construct the exact contract schema without a production JSON dependency.
 * @return Owned validated Example/Empty declarations and explicit aliases.
 */
c::LocalSchema schema() {
    auto value = c::LocalSchema::create(
        std::vector<c::LocalRecord>{
            {0, u"Example", false, {{8}, {10}}, {{u"Count", 0}, {u"OutgoingCount", 0}, {u"Text", 1}}},
            {1, u"Empty", false, {}, {}}},
        limits().layouts.schema, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(value);

    return std::move(*value);
}

/** Read a bounded immutable binary fixture.
 * @param path Explicit path.
 * @return Owned exact bytes.
 */
std::vector<std::byte> read(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.good());
    std::vector<char> chars((std::istreambuf_iterator<char>(file)), {});
    REQUIRE(chars.size() <= limits().inputBytes);
    const auto bytes = std::as_bytes(std::span(chars));

    return {bytes.begin(), bytes.end()};
}

/** Append canonical compact-long bytes, with fixed private capacity.
 * @param bytes Owning destination.
 * @param value Signed primitive.
 */
void number(std::vector<std::byte> &bytes, std::int64_t value) {
    std::array<std::byte, 9> buffer{};
    auto result = e::writeCompactLong(value, buffer);
    REQUIRE(result.status == e::Status::OK);
    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Append exact nullable UTFString using the allocation-free production primitive.
 * @param bytes Owning destination.
 * @param value Nullable exact units.
 */
void text(std::vector<std::byte> &bytes, std::optional<std::u16string_view> value) {
    std::array<std::byte, 4096> buffer{};
    auto result = e::writeUtfString(value, buffer, {4096, 1024});
    REQUIRE(result.status == e::Status::OK);
    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Frame a manually constructed synthetic payload.
 * @param type Explicit supported or intentionally invalid message identity.
 * @param payload Exact owning synthetic payload.
 * @return Complete canonical envelope.
 */
std::vector<std::byte> frame(std::int32_t type, const std::vector<std::byte> &payload) {
    std::vector<std::byte> body;
    number(body, type);
    body.insert(body.end(), payload.begin(), payload.end());
    std::vector<std::byte> result;
    number(result, static_cast<std::int64_t>(body.size()));
    result.insert(result.end(), body.begin(), body.end());

    return result;
}

/** Create a synthetic exact or intentionally invalid descriptor.
 * @param time Exact timing spelling.
 * @param version Nullable optional version.
 * @param options Nullable value; absent unless optionsPresent is true.
 * @param optionsPresent Whether to include the option property, including intentional null.
 * @param send Whether to include explicit RAW send.
 * @param unknown Whether to append an intentionally unsupported property.
 * @return Whole initial descriptor envelope.
 */
std::vector<std::byte> header(std::u16string_view time = u"message", std::optional<std::u16string_view> version = {},
                              std::optional<std::u16string_view> options = u"", bool optionsPresent = true,
                              bool send = true, bool unknown = false) {
    std::vector<std::byte> payload{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    number(payload, 2 + (version ? 1 : 0) + (optionsPresent ? 1 : 0) + (unknown ? 1 : 0));
    text(payload, u"type");
    text(payload, u"tape");
    text(payload, u"time");
    text(payload, time);

    if (version) {
        text(payload, u"version");
        text(payload, version);
    }

    if (optionsPresent) {
        text(payload, u"opt");
        text(payload, options);
    }

    if (unknown) {
        text(payload, u"future");
        text(payload, u"value");
    }

    number(payload, send ? 1 : 0);

    if (send) {
        number(payload, 5);
        text(payload, u"RAW_DATA");
        number(payload, 0);
    }

    number(payload, 0);

    return frame(1, payload);
}

/** Serialize raw declarations without strict outgoing validation, solely for negative witnesses.
 * @param descriptions Exact synthetic wire declarations.
 * @return Complete type-2 envelope.
 */
std::vector<std::byte> descriptions(std::span<const p::WireRecordDescription> descriptions) {
    std::vector<std::byte> payload;

    for (const auto &record : descriptions) {
        number(payload, record.senderId);
        text(payload, record.name);
        number(payload, static_cast<std::int64_t>(record.fields.size()));

        for (const auto &field : record.fields) {
            text(payload, field.name);
            number(payload, field.serialTypeId);
        }
    }

    return frame(2, payload);
}

/** Append a whole envelope.
 * @param bytes Complete source under construction.
 * @param suffix Whole canonical or intentional malformed bytes.
 */
void append(std::vector<std::byte> &bytes, const std::vector<std::byte> &suffix) {
    bytes.insert(bytes.end(), suffix.begin(), suffix.end());
}

/** Standard Example description in preserved wire permutation.
 * @return Owning declaration.
 */
p::WireRecordDescription example() {
    return {37, u"Example", {{u"Text", 10}, {u"OutgoingCount", 8}}};
}

/** Construct a synthetic description-only source, independently labelled from FileWriter observations.
 * @param records Exact wire descriptions.
 * @param initial Descriptor envelope.
 * @return Complete source with initial header and the supplied description group.
 */
std::vector<std::byte> source(std::span<const p::WireRecordDescription> records,
                              std::vector<std::byte> initial = header()) {
    append(initial, descriptions(records));

    return initial;
}

/** Require a stable strict refusal and verify no result token escaped.
 * @param bytes Complete synthetic input.
 * @param code Stable admission reason.
 */
void refused(const std::vector<std::byte> &bytes, p::AdmissionCode code) {
    auto local = schema();
    auto result = p::admitTape(local, bytes, limits());
    REQUIRE_FALSE(result);
    INFO(result.error().stage);

    if (result.error().reader) {
        INFO(result.error().reader->stage);
    }

    CHECK(result.error().code == code);
}

/** Baseline native writer fixture; retains all metadata/data/append placements.
 * @return Exact reviewed public interoperability bytes.
 */
std::vector<std::byte> contract() {
    return read(fixtures / "../../../../conformance/protocol/tape-write/writer-evidence/inputs/contract/tape.qtp");
}
} // namespace

TEST_CASE("strict admission preserves complete native writer contract and owns its token") {
    static_assert(!std::is_default_constructible_v<p::AdmittedTape>);
    static_assert(!std::is_copy_constructible_v<p::AdmittedTape>);
    static_assert(!std::is_move_assignable_v<p::AdmittedTape>);
    auto bytes = contract();
    auto original = bytes;
    auto local = schema();
    auto result = p::admitTape(local, bytes, limits());
    REQUIRE(result);
    CHECK(result->valid());
    CHECK(result->layouts().records().size() == 2);
    CHECK(result->layouts().records()[0].fields[0].localFieldId == 1);
    CHECK(result->layouts().records()[0].fields[1].wireName == u"OutgoingCount");
    CHECK(result->descriptor().time == u"message");
    CHECK(result->descriptor().options == u"");
    CHECK_FALSE(result->descriptor().version);
    bytes.clear();
    CHECK(std::ranges::equal(result->bytes(), original));
    p::AdmittedTape moved(std::move(*result));
    CHECK(moved.valid());
    CHECK_FALSE(result->valid());
    CHECK(std::ranges::equal(moved.bytes(), original));
}

TEST_CASE("all strict admission packet cases have explicit synthetic witnesses") {
    auto packet = read(fixtures / "admission-cases.json");
    const std::string textPacket(reinterpret_cast<const char *>(packet.data()), packet.size());

    for (unsigned i = 1; i <= 23; ++i) {
        const auto id = std::string("A") + (i < 10 ? "0" : "") + std::to_string(i);
        CAPTURE(id);
        CHECK(textPacket.find("\"id\": \"" + id + "\"") != std::string::npos);
    }

    auto base = example();
    std::array<p::WireRecordDescription, 1> records{base};
    auto local = schema();
    CHECK(p::admitTape(local, source(records), limits())); // A01 exact permutation.
    auto bytes = source(records);
    append(bytes, descriptions(std::array<p::WireRecordDescription, 1>{{99, u"Unknown", {}}}));
    refused(bytes, p::AdmissionCode::UNKNOWN_RECORD); // A02 unused zero-field unknown.
    records[0].fields.push_back({u"Unknown", 8});
    refused(source(records), p::AdmissionCode::SKIPPED_FIELD); // A03.
    records[0] = base;
    records[0].fields[1] = {u"Text", 10};
    refused(source(records), p::AdmissionCode::REPEATED_TARGET); // A04.
    records[0] = base;
    records[0].fields.pop_back();
    refused(source(records), p::AdmissionCode::MISSING_TARGET); // A05.
    records[0] = base;
    bytes = source(records);
    append(bytes, descriptions(records));
    auto result = p::admitTape(local, bytes, limits());
    REQUIRE_FALSE(result);
    CHECK(result.error().reader->error == p::TapeError::MALFORMED); // A06.
    records[0].senderId = 99;
    bytes = source(std::array{base});
    append(bytes, descriptions(records));
    refused(bytes, p::AdmissionCode::DUPLICATE_LAYOUT_IDENTITY); // A07.
    records[0] = base;

    for (auto type : {24, 137}) {
        records[0] = base;
        records[0].fields[type == 24 ? 0 : 1].serialTypeId = type;
        auto result1 = p::admitTape(local, source(records), limits());
        REQUIRE_FALSE(result1);
        CHECK(result1.error().error == p::AdmissionError::UNSUPPORTED);
    } // A08/A09.

    auto result2 = p::admitTape(local, header(u"message", {}, u"", true, true, true), limits());
    REQUIRE_FALSE(result2);
    CHECK(result2.error().error == p::AdmissionError::UNSUPPORTED); // A10.
    auto result3 = p::admitTape(local, header(u"message", {}, {}, true), limits());
    REQUIRE_FALSE(result3);
    CHECK(result3.error().error == p::AdmissionError::UNSUPPORTED); // A11.
    auto result4 = p::admitTape(local, header(u"MESSAGE"), limits());
    REQUIRE(result4);
    CHECK(result4->descriptor().time == u"MESSAGE"); // A12.
    auto result5 = p::admitTape(local, header(u"message", {}, {}, false, false), limits());
    REQUIRE(result5);
    CHECK_FALSE(result5->descriptor().version);
    CHECK_FALSE(result5->descriptor().options);
    CHECK_FALSE(result5->descriptor().explicitRawSend); // A13.
    auto result6 = p::admitTape(local, header(u"message", u"", u"hs"), limits());
    REQUIRE(result6);
    CHECK(result6->descriptor().version == u"");
    CHECK(result6->descriptor().options == u"hs"); // A14.
    // NEW_FLAGS F9 precedes compact flags, then the actual empty CESU symbol FD00.
    bytes = source(std::array{base});
    append(bytes, frame(5, {std::byte{0xf9}, std::byte{1}, std::byte{0xfd}, std::byte{0}}));
    auto result7 = p::admitTape(local, bytes, limits());
    REQUIRE_FALSE(result7);
    CHECK(result7.error().error == p::AdmissionError::UNSUPPORTED); // A15.
    bytes = header();
    append(bytes, frame(0, {std::byte{2}, std::byte{0}}));
    auto result8 = p::admitTape(local, bytes, limits());
    REQUIRE_FALSE(result8);
    CHECK(result8.error().error == p::AdmissionError::UNSUPPORTED); // A16.
    bytes = contract();
    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0x80});
    auto result9 = p::admitTape(local, bytes, limits());
    REQUIRE_FALSE(result9);
    CHECK(result9.error().error == p::AdmissionError::MALFORMED); // A17.
    auto result10 = p::admitTape(local, header(u"FIELD"), limits());
    REQUIRE_FALSE(result10);
    CHECK(result10.error().error == p::AdmissionError::UNSUPPORTED); // A18.
    bytes = header();
    append(bytes, frame(15, {}));
    auto result11 = p::admitTape(local, bytes, limits());
    REQUIRE_FALSE(result11);
    CHECK(result11.error().error == p::AdmissionError::UNSUPPORTED); // A19.
    // Copy a real completed RAW frame before any description, rather than inventing symbol grammar.
    auto fixture = contract();
    p::FrameReader framer(limits().reader.frame);
    std::size_t offset = 0;
    bytes = header();

    while (offset < fixture.size()) {
        auto event = framer.feed(std::span<const std::byte>(fixture).subspan(offset));
        offset += event.accepted;

        if (event.frame && event.frame->type == 5) {
            append(bytes, frame(5, event.frame->payload));
            break;
        }
    }

    auto result12 = p::admitTape(local, bytes, limits());
    REQUIRE_FALSE(result12);
    CHECK(result12.error().error == p::AdmissionError::MALFORMED); // A20.
    records[0] = base;
    records[0].name = std::u16string(1, char16_t{0xd800});
    auto result13 = p::admitTape(local, source(records), limits());
    REQUIRE_FALSE(result13);
    CHECK(result13.error().error == p::AdmissionError::MALFORMED); // A21.
    bytes = source(std::array{base});
    auto tail = descriptions(std::array<p::WireRecordDescription, 1>{{127, u"Empty", {}}});
    tail.push_back(std::byte{0x80});
    // Malformed suffix is inside the type-2 envelope, not a subsequent envelope.
    p::FrameReader tailReader(limits().reader.frame);
    auto tailEvent = tailReader.feed(tail);
    REQUIRE(tailEvent.frame);
    auto payload = tailEvent.frame->payload;
    payload.push_back(std::byte{0x80});
    append(bytes, frame(2, payload));
    auto result14 = p::admitTape(local, bytes, limits());
    REQUIRE_FALSE(result14);
    CHECK(result14.error().error == p::AdmissionError::MALFORMED); // A22.
    records[0] = base;
    records[0].senderId = INT32_MAX;
    auto result15 = p::admitTape(local, source(records), limits());
    REQUIRE_FALSE(result15);
    REQUIRE(result15.error().writer);
    CHECK(result15.error().writer->error == p::WriterError::INVALID_LAYOUT);
    CHECK(result15.error().writer->stage == "record-id"); // A23.
}

TEST_CASE("extra admission budgets admit equality and reject zero or one below actual charge") {
    auto bytes = contract();
    auto local = schema();
    auto baseline = p::admitTape(local, bytes, limits());
    REQUIRE(baseline);
    const auto measured = baseline->counters();

    for (unsigned member = 0; member < 4; ++member) {
        auto bounds = limits();
        auto *limit = member == 0   ? &bounds.inputBytes
                      : member == 1 ? &bounds.workUnits
                      : member == 2 ? &bounds.scratchBytes
                                    : &bounds.scratchUnits;
        const auto exact = member == 0   ? bytes.size()
                           : member == 1 ? measured.workUnits
                           : member == 2 ? measured.peakScratchBytes
                                         : measured.peakScratchUnits;
        REQUIRE(exact > 0);

        for (auto value : {std::size_t{0}, exact - 1, exact, exact + 1}) {
            *limit = value;
            auto outcome = p::admitTape(local, bytes, bounds);
            CAPTURE(member);
            CAPTURE(value);
            CAPTURE(exact);
            CHECK(outcome.has_value() == (value >= exact));

            if (!outcome) {
                CHECK(outcome.error().error == p::AdmissionError::RESOURCE_LIMIT);
            }
        }
    }
}

TEST_CASE("description heavy actual extra text ownership controls admission before allocation") {
    std::vector<c::LocalRecord> localRecords;
    std::vector<p::WireRecordDescription> wire;

    for (std::int32_t i = 0; i < 128; ++i) {
        auto digits = std::to_string(i);
        digits.insert(digits.begin(), 3 - digits.size(), '0');
        std::u16string name = u"Example";

        for (auto digit : digits) {
            name.push_back(static_cast<char16_t>(digit));
        }

        localRecords.push_back({i, name, false, {{8}}, {{u"Count", 0}, {u"OutgoingCount", 0}}});
        wire.push_back({37 + i, name, {{u"OutgoingCount", 8}}});
    }

    auto local = c::LocalSchema::create(localRecords, limits().layouts.schema, c::LocalProfile::COMPACT_INT_V1);
    REQUIRE(local);
    auto bytes = source(wire);
    std::vector<p::OutgoingRecord> outgoingRecords;
    std::vector<std::int32_t> senderIds;
    std::vector<p::OutgoingRecordView> data;
    const std::array<c::FieldValue, 1> zero{{c::SerialFieldType::COMPACT_INT, 0, {}}};

    for (const auto &record : wire) {
        outgoingRecords.push_back({record.senderId, record.senderId - 37, record.name, {{u"OutgoingCount", 8, 0}}});
        senderIds.push_back(record.senderId);
        data.push_back({record.senderId, u"IBM", zero, 0});
    }

    auto outgoing = p::OutgoingLayouts::create(*local, outgoingRecords, limits().layouts);
    REQUIRE(outgoing);
    const p::WriterLimits writerLimits{1048576,          16777216, 65536,   1048576, 8388608, {1048576, 1024}, 1048576,
                                       {1048576, 16384}, 16777216, 4194304, 16384,   4194304, 1048585,         32768,
                                       67108864};
    auto writer = p::TapeWriter::create(*outgoing, {{}, u"", u"message", true}, writerLimits);
    REQUIRE(writer);
    REQUIRE(writer->begin());
    REQUIRE(writer->describe(senderIds));
    auto dataChunk = writer->data(data);
    REQUIRE(dataChunk);
    append(bytes, dataChunk->bytes);
    REQUIRE(writer->finish());
    auto baseline = p::admitTape(*local, bytes, limits());
    REQUIRE(baseline);
    const auto exact = baseline->counters().peakScratchUnits;
    // Simultaneous outgoing candidate + copied outgoing inventory + copied schema + exact descriptor.
    CHECK(exact == 2944 + 2944 + 3584 + 7);

    for (auto value : {std::size_t{0}, exact - 1, exact, exact + 1}) {
        auto bounds = limits();
        bounds.scratchUnits = value;
        auto result = p::admitTape(*local, bytes, bounds);
        CAPTURE(value);
        CAPTURE(exact);
        CHECK(result.has_value() == (value >= exact));

        if (!result) {
            CHECK(result.error().stage == "scratch-units");
        }
    }

    if (!emitDirectory.empty()) {
        std::filesystem::create_directories(emitDirectory);
        std::ofstream file(emitDirectory / "description-heavy.qtp", std::ios::binary);
        REQUIRE(file.good());
        file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        file.close();
        REQUIRE(!file.fail());
        std::ofstream schemaFile(emitDirectory / "description-heavy.json", std::ios::binary);
        REQUIRE(schemaFile.good());
        schemaFile << "{\"formatVersion\":1,\"profile\":{\"id\":\"qdcxx-native\",\"version\":1},\"records\":[";

        for (std::size_t i = 0; i < localRecords.size(); ++i) {
            if (i != 0) {
                schemaFile << ',';
            }

            std::string name;

            for (auto unit : localRecords[i].name) {
                name.push_back(static_cast<char>(unit));
            }

            schemaFile << "{\"id\":" << i << ",\"name\":\"" << name
                       << "\",\"hasTime\":false,\"fields\":[{\"serialTypeId\":8}],\"lookups\":[{\"name\":\"Count\","
                          "\"fieldId\":0},{\"name\":\"OutgoingCount\",\"fieldId\":0}]}";
        }

        schemaFile << "]}\n";
        schemaFile.close();
        REQUIRE(!schemaFile.fail());
        std::cout << "description-heavy-peak-units\t" << exact << '\n';
    }
}

TEST_CASE("actual owning allocation failures never publish an admitted token") {
    auto bytes = contract();
    auto local = schema();
    bool reachedSuccess = false;

    for (std::size_t allowance = 0; allowance < 512; ++allowance) {
        allocation_test::remaining = allowance;
        allocation_test::enabled = true;
        auto result = p::admitTape(local, bytes, limits());
        allocation_test::enabled = false;

        if (result) {
            reachedSuccess = true;
            break;
        }

        CAPTURE(allowance);
        INFO(result.error().stage);

        if (result.error().reader) {
            INFO(result.error().reader->stage);
        }

        CHECK(result.error().error == p::AdmissionError::ALLOCATION_FAILURE);
    }

    CHECK(reachedSuccess);
}

TEST_CASE("descriptor-only admission independently charges every unused schema element copy") {
    auto bytes = header();

    for (std::size_t fields : {std::size_t{0}, std::size_t{1}, std::size_t{3}}) {
        c::LocalRecord record{0, u"Example", false, {}, {}};

        for (std::size_t i = 0; i < fields; ++i) {
            record.fields.push_back({8});
            record.lookups.push_back({std::u16string(1, static_cast<char16_t>(u'A' + i)), i});
        }

        auto local = c::LocalSchema::create(std::array{record}, limits().layouts.schema);
        REQUIRE(local);
        auto outcome = p::admitTape(*local, bytes, limits());
        REQUIRE(outcome);
        // Source copy + descriptor text + schema census (record/lookups) + copied record/fields/lookups/text.
        const auto expected = bytes.size() + 7 + (1 + fields) + (1 + fields + fields) + (7 + fields);
        CHECK(outcome->counters().workUnits == expected);
        auto bounds = limits();
        bounds.workUnits = expected - 1;
        auto refused = p::admitTape(*local, bytes, bounds);
        REQUIRE_FALSE(refused);
        CHECK(refused.error().stage == "admission-work");
        bounds.workUnits = expected;
        CHECK(p::admitTape(*local, bytes, bounds));
    }
}

/** Run deterministic conformance tests with an explicit immutable fixture packet.
 * @param argc Argument count.
 * @param argv Fixture directory and optional emitted synthetic corpus path.
 * @return Doctest exit status or invocation failure.
 */
int main(int argc, char **argv) {
    if (argc != 2 && argc != 3) {
        return 2;
    }

    fixtures = argv[1];

    if (argc == 3) {
        emitDirectory = argv[2];
    }

    doctest::Context context;

    return context.run();
}
