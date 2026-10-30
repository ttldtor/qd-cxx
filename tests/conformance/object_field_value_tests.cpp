// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/encoding/compact.hpp"
#include "qd/protocol/tape_reader.hpp"
#include "qd/protocol/tape_writer.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <doctest/doctest.h>
#include <limits>
#include <new>

namespace {
std::ptrdiff_t allocationFailAfter = -1; ///< Single-threaded ordinary-allocation injection; negative disables it.
}

/** Inject allocation failure only in this test process.
 * @param size Requested allocation bytes; zero still produces deletable storage.
 * @return Owned storage or bad_alloc at the selected checkpoint.
 */
void *operator new(std::size_t size) {
    if (allocationFailAfter == 0) {
        throw std::bad_alloc();
    }

    if (allocationFailAfter > 0) {
        --allocationFailAfter;
    }

    if (auto *value = std::malloc(size == 0 ? 1 : size)) {
        return value;
    }

    throw std::bad_alloc();
}

/** Allocate an array using the same injection point.
 * @param size Requested bytes.
 * @return Owned storage or bad_alloc.
 */
void *operator new[](std::size_t size) {
    return ::operator new(size);
}

/** Release ordinary test storage.
 * @param value Previously allocated storage, possibly null.
 */
void operator delete(void *value) noexcept {
    std::free(value);
}

/** Release array test storage.
 * @param value Previously allocated storage, possibly null.
 */
void operator delete[](void *value) noexcept {
    std::free(value);
}

/** Release sized ordinary storage.
 * @param value Previously allocated storage.
 * @param size Compiler size, deliberately unused.
 */
void operator delete(void *value, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(value);
}

/** Release sized array storage.
 * @param value Previously allocated storage.
 * @param size Compiler size, deliberately unused.
 */
void operator delete[](void *value, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(value);
}

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace p = org::ttldtor::qdcxx::protocol;
using Bytes = std::vector<std::byte>; ///< Owned test input/output bytes.

/** Construct finite witness budgets independent of decoder output.
 * @return Explicit budgets for old strings and every new payload family.
 */
p::StreamDataLimits limits() {
    p::StreamDataLimits result{65536, 64, 256, 64, 256, 1024, {65536, 128, 128}, {65536, 8192}, 65536, 8192, 8192};
    result.fieldPayloadEncodedBytes = 65536;
    result.fieldPayloadBytes = 65536;
    result.payloadEncodedBytes = 65536;
    result.payloadWorkBytes = 65536;
    result.ownedPayloadBytes = 65536;

    return result;
}

/** Preserve exact visible wire bytes in a test witness.
 * @param values Octets in wire order.
 * @return Owned byte vector.
 */
Bytes bytes(std::initializer_list<unsigned int> values) {
    Bytes result;

    for (const auto value : values) {
        result.push_back(static_cast<std::byte>(value));
    }

    return result;
}

/** Append canonical compact64 bytes.
 * @param output Destination witness.
 * @param value Exact signed value.
 */
void compact(Bytes &output, std::int64_t value) {
    std::array<std::byte, 9> storage{};
    const auto encoded = e::writeCompactLong(value, storage);
    REQUIRE(encoded.status == e::Status::OK);
    output.insert(output.end(), storage.begin(), storage.begin() + encoded.written);
}

/** Build one physical field without conflating its length grammar with its tag.
 * @param count Encoded compact value; may intentionally exceed the valid domain.
 * @param body Exact subsequent bytes.
 * @return Owned field witness.
 */
Bytes field(std::int64_t count, Bytes body = {}) {
    Bytes result;
    compact(result, count);
    result.insert(result.end(), body.begin(), body.end());

    return result;
}

/** Add a known symbol/id prefix and optional independent scalar sentinel.
 * @param body Fields in descriptor order.
 * @param sentinel Whether to append compact77.
 * @return Complete data payload.
 */
Bytes payload(const Bytes &body, bool sentinel = true) {
    auto result = bytes({0x24, 0x4d, 0});
    result.insert(result.end(), body.begin(), body.end());

    if (sentinel) {
        compact(result, 77);
    }

    return result;
}

/** Build a projection whose local lifetime ends before the returned context is used.
 * @param type Exact value tag.
 * @param fields Ordered source fields; empty selects Value then Sentinel.
 * @param known Whether the source record resolves locally.
 * @return Owned immutable decoder context.
 */
p::StreamDataContext context(std::int32_t type, std::vector<p::WireFieldDescription> fields = {}, bool known = true) {
    const c::SchemaLimits schemaLimits{8, 64, 128, 128, 8192};
    const std::vector<c::LocalRecord> records{{0, u"Objects", false, {{type}, {8}}, {{u"Value", 0}, {u"Sentinel", 1}}}};
    auto local = c::LocalSchema::create(records, schemaLimits, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);

    if (fields.empty()) {
        fields = {{u"Value", type}, {u"Sentinel", 8}};
    }

    const std::vector<p::WireRecordDescription> descriptions{{0, known ? u"Objects" : u"Unknown", std::move(fields)}};
    auto result = p::StreamDataContext::create(*local, descriptions, {schemaLimits, 8, {256, 128, 8192}});
    REQUIRE(result);

    return std::move(*result);
}

/** Decode either supported family through its public complete-payload entry.
 * @param input Complete payload.
 * @param plan Owned immutable context.
 * @param budget Explicit remaining limits.
 * @param raw Whether to select RAW instead of STREAM.
 * @return Owned completed prefix and diagnostics.
 */
p::StreamDataResult decode(const Bytes &input, const p::StreamDataContext &plan, p::StreamDataLimits budget = limits(),
                           bool raw = false) {
    return raw ? p::decodeRawData(input, plan, budget) : p::decodeStreamData(input, plan, budget);
}
} // namespace

TEST_CASE("payload exact tags and old aggregates preserve distinct null empty and invalid mixes") {
    for (const auto type : {9, 137, 233, 249}) {
        CHECK(c::supportsSerialType(type, c::LocalProfile::BINARY_FIELDS_V1));

        for (const auto profile :
             {c::LocalProfile::COMPACT_INT_V1, c::LocalProfile::QUOTE_FIELDS_V1, c::LocalProfile::EXTENDED_FIELDS_V1}) {
            CHECK_FALSE(c::supportsSerialType(type, profile));
        }

        const auto tag = static_cast<c::SerialFieldType>(type);
        c::FieldValue null{tag, 0};
        c::FieldValue empty{tag, 0};

        if (type == 137) {
            empty.units.emplace();
        } else {
            empty.bytes.emplace();
        }

        CHECK(c::validFieldValue(null));
        CHECK(c::validFieldValue(empty));
        CHECK(null != empty);
        auto invalid = empty;
        invalid.raw = 1;
        CHECK_FALSE(c::validFieldValue(invalid));
        invalid = empty;

        if (type == 137) {
            invalid.bytes.emplace();
        } else {
            invalid.units.emplace();
        }

        CHECK_FALSE(c::validFieldValue(invalid));
    }

    const c::FieldValue oldScalar{c::SerialFieldType::COMPACT_INT, 12};
    const c::FieldValue oldText{c::SerialFieldType::UTF_CHAR_ARRAY, 0, u"text"};
    CHECK(c::validFieldValue(oldScalar));
    CHECK(c::validFieldValue(oldText));

    for (auto invalid : {oldScalar, oldText}) {
        invalid.bytes.emplace();
        CHECK_FALSE(c::validFieldValue(invalid));
    }

    for (const auto type : {-1, 25, 168, 265, 393, 489, 505}) {
        CHECK_FALSE(c::supportsSerialType(type, c::LocalProfile::BINARY_FIELDS_V1));
        CHECK_FALSE(c::validFieldValue({static_cast<c::SerialFieldType>(type), 0}));
    }
}

TEST_CASE("all payload families retain owned null empty content and shared RAW STREAM results") {
    for (const auto type : {9, 137, 233, 249}) {
        const auto plan = context(type);

        for (const auto count : {-1, 0, 3}) {
            const auto body = count > 0 ? bytes({0x41, 0, 0x42}) : Bytes{};
            auto input = payload(field(count, body));
            auto result = decode(input, plan);
            const auto raw = decode(input, plan, limits(), true);
            REQUIRE(result.status == p::StreamDataStatus::OK);
            CHECK(raw.status == result.status);
            REQUIRE(result.records.size() == 1);
            CHECK(raw.records[0].typedValues == result.records[0].typedValues);
            CHECK(result.payloadEncodedBytes == 1 + body.size());
            CHECK(result.payloadWorkBytes == body.size());
            CHECK(result.ownedPayloadBytes == body.size() * (type == 137 ? 2 : 1));
            CHECK(result.fieldBytes == 0);
            CHECK(result.fieldUnits == (type == 137 ? body.size() : 0));
            CHECK(result.records[0].typedValues[1].raw == 77);
            const auto value = result.records[0].typedValues[0];
            std::fill(input.begin(), input.end(), std::byte{0xff});
            result.records.clear();
            CHECK(c::validFieldValue(value));
            CHECK(value.raw == 0);

            if (type == 137) {
                CHECK_FALSE(value.bytes);
                CHECK(value.units.has_value() == (count >= 0));

                if (count > 0) {
                    CHECK(*value.units == std::u16string{u'A', 0, u'B'});
                }
            } else {
                CHECK_FALSE(value.units);
                CHECK(value.bytes.has_value() == (count >= 0));

                if (count > 0) {
                    CHECK(*value.bytes == body);
                }
            }
        }

        const auto defaults = decode(payload({}, true), context(type, {{u"Sentinel", 8}}));
        REQUIRE(defaults.status == p::StreamDataStatus::OK);
        CHECK(defaults.records[0].typedValues[0] == c::FieldValue{static_cast<c::SerialFieldType>(type), 0});
    }
}

TEST_CASE("projected serial-object length stays distinct from validated LONG even when discarded") {
    for (const auto type : {9, 233, 249}) {
        for (const auto count : {std::int64_t{0x100000000}, std::int64_t{0xffffffff}, std::int64_t{0x100000001}}) {
            for (const bool known : {false, true}) {
                const auto body = count == 0x100000001 ? bytes({0x41}) : Bytes{};
                const auto result = decode(payload(field(count, body)), context(type, {}, known));
                CHECK(result.status == (type == 249 ? p::StreamDataStatus::OK : p::StreamDataStatus::INVALID_FIELD));
                CHECK(result.payloadEncodedBytes == (type == 249 ? 5 + body.size() : 5));

                if (type == 249 && known) {
                    REQUIRE(result.records.size() == 1);
                    const auto &value = result.records[0].typedValues[0];
                    CHECK(value.bytes.has_value() == (count != 0xffffffff));
                }
            }
        }

        for (const bool known : {false, true}) {
            const auto overlong = decode(payload(bytes({0x80, 1, 0x41})), context(type, {}, known));
            CHECK(overlong.status == p::StreamDataStatus::OK);
            CHECK(overlong.payloadEncodedBytes == 3);
            CHECK(overlong.payloadWorkBytes == 1);
            const auto invalid = decode(payload(field(-2)), context(type, {}, known));
            CHECK(invalid.status == p::StreamDataStatus::INVALID_FIELD);
            CHECK(invalid.payloadEncodedBytes == 1);
            CHECK(invalid.payloadWorkBytes == 0);
        }
    }
}

TEST_CASE("UTFString preserves exact Unicode and validates skipped UTF with actual tail examination") {
    const std::vector<std::pair<Bytes, std::u16string>> valid{
        {bytes({1, 0}), std::u16string{0}},
        {bytes({4, 0xf0, 0x9f, 0x98, 0x80}), u"\U0001f600"},
        {bytes({6, 0xed, 0xa0, 0xbd, 0xed, 0xb8, 0x80}), u"\U0001f600"},
        {bytes({3, 0xed, 0xa0, 0x80}), std::u16string{0xd800}},
        {bytes({2, 0xc0, 0x80}), std::u16string{0}},
        {bytes({3, 0xe0, 0x80, 0x80}), std::u16string{0}},
        {bytes({4, 0xf0, 0x80, 0x80, 0x80}), std::u16string{0}}};

    for (const auto &[body, units] : valid) {
        const auto result = decode(payload(body), context(137));
        REQUIRE(result.status == p::StreamDataStatus::OK);
        CHECK(result.records[0].typedValues[0].units == units);
        CHECK(result.fieldUnits == body.size() - 1);
        CHECK(result.ownedFieldUnits == units.size());
        CHECK(result.ownedPayloadBytes == units.size() * sizeof(char16_t));
        CHECK(result.records[0].typedValues[1].raw == 77);
    }

    for (const auto &body : {bytes({2, 0xc2, 0x41}), bytes({1, 0x80}), bytes({4, 0xf4, 0x90, 0x80, 0x80}),
                             bytes({1, 0xc2, 0x80}), bytes({1, 0xf0, 0x90, 0x80, 0x80})}) {
        for (const bool known : {false, true}) {
            const auto result = decode(payload(body), context(137, {}, known));
            CHECK(result.status == p::StreamDataStatus::INVALID_FIELD);
            CHECK(result.stage == p::StreamDataStage::FIELD);
            CHECK(result.errorOffset == 3);
            CHECK(result.consumed == 3 + body.size());
            CHECK(result.payloadEncodedBytes == body.size());
            CHECK(result.payloadWorkBytes == std::to_integer<unsigned int>(body[0]));
            CHECK(result.records.empty());
        }
    }

    const auto split = payload(bytes({1, 0xc2, 0x80}));

    for (const auto bound : {std::size_t{0}, std::size_t{2}, std::size_t{3}, std::size_t{4}}) {
        auto budget = limits();
        budget.fieldPayloadEncodedBytes = bound;
        const auto result = decode(split, context(137), budget);
        CHECK(result.status == (bound < 3 ? p::StreamDataStatus::RESOURCE_LIMIT : p::StreamDataStatus::INVALID_FIELD));
        CHECK(result.payloadEncodedBytes == std::min(bound, std::size_t{3}));
        CHECK(result.consumed == 3 + result.payloadEncodedBytes);
    }

    CHECK(decode(payload(bytes({2, 0xc2}), false), context(137)).status == p::StreamDataStatus::INVALID_FIELD);
    CHECK(decode(payload(bytes({1, 0xc2}), false), context(137)).status == p::StreamDataStatus::TRUNCATED_PAYLOAD);
    CHECK(decode(payload(bytes({0xf0}), false), context(137)).status == p::StreamDataStatus::TRUNCATED_PAYLOAD);
}

TEST_CASE("every new byte budget is finite at zero and exact edges including skipped work") {
    const auto input = payload(field(3, bytes({0x41, 0x42, 0x43})));

    for (const auto type : {9, 137, 233, 249}) {
        for (const bool known : {false, true}) {
            const auto plan = context(type, {}, known);

            for (const auto member :
                 {&p::StreamDataLimits::fieldPayloadEncodedBytes, &p::StreamDataLimits::payloadEncodedBytes,
                  &p::StreamDataLimits::fieldPayloadBytes, &p::StreamDataLimits::payloadWorkBytes,
                  &p::StreamDataLimits::ownedPayloadBytes}) {
                const bool encoded = member == &p::StreamDataLimits::fieldPayloadEncodedBytes ||
                                     member == &p::StreamDataLimits::payloadEncodedBytes;
                const auto required = encoded                                                            ? 4
                                      : member == &p::StreamDataLimits::ownedPayloadBytes && type == 137 ? 6
                                                                                                         : 3;

                for (const auto allowance : {0, required - 1, required, required + 1}) {
                    auto budget = limits();
                    budget.*member = static_cast<std::size_t>(allowance);
                    const auto result = decode(input, plan, budget);
                    CHECK(result.status ==
                          (allowance < required ? p::StreamDataStatus::RESOURCE_LIMIT : p::StreamDataStatus::OK));
                    CHECK(result.payloadEncodedBytes <= budget.fieldPayloadEncodedBytes);
                    CHECK(result.payloadEncodedBytes <= budget.payloadEncodedBytes);
                }
            }
        }
    }

    for (const auto count : {-1, 0}) {
        auto budget = limits();
        budget.fieldPayloadBytes = budget.payloadWorkBytes = budget.ownedPayloadBytes = 0;
        const auto result = decode(payload(field(count)), context(9), budget);
        CHECK(result.status == p::StreamDataStatus::OK);
        CHECK(result.payloadEncodedBytes == 1);
        CHECK(result.payloadWorkBytes == 0);
    }
}

TEST_CASE("replacements skips failures and completed outputs never refund admitted work or hide peak ownership") {
    const auto plan = context(9, {{u"Value", 9}, {u"skip", 9}, {u"Value", 9}, {u"Sentinel", 8}});
    auto body = field(3, bytes({1, 2, 3}));
    const auto next = field(3, bytes({4, 5, 6}));
    body.insert(body.end(), next.begin(), next.end());
    body.insert(body.end(), next.begin(), next.end());
    const auto input = payload(body);

    for (const auto allowance : {std::size_t{0}, std::size_t{5}, std::size_t{6}, std::size_t{7}}) {
        auto budget = limits();
        budget.ownedPayloadBytes = allowance;
        const auto result = decode(input, plan, budget);
        CHECK(result.status == (allowance >= 6 ? p::StreamDataStatus::OK : p::StreamDataStatus::RESOURCE_LIMIT));
        CHECK(result.payloadWorkBytes == (allowance >= 6 ? 9 : allowance >= 3 ? 6 : 3));
        CHECK(result.ownedPayloadBytes == (allowance >= 6 ? 3 : 0));
    }

    auto budget = limits();
    budget.payloadWorkBytes = 8;
    const auto denied = decode(input, plan, budget);
    CHECK(denied.status == p::StreamDataStatus::RESOURCE_LIMIT);
    CHECK(denied.payloadWorkBytes == 6);
    CHECK(denied.payloadEncodedBytes == 9);
    auto two = payload(field(3, bytes({1, 2, 3})));
    const auto second = two;
    two.insert(two.end(), second.begin(), second.end());
    budget = limits();
    budget.ownedPayloadBytes = 5;
    const auto completed = decode(two, context(9), budget);
    CHECK(completed.status == p::StreamDataStatus::RESOURCE_LIMIT);
    REQUIRE(completed.records.size() == 1);
    CHECK(completed.payloadWorkBytes == 6);
    CHECK(completed.ownedPayloadBytes == 3);
}

TEST_CASE("mixed old character arrays and UTFString share string work without charging old encoded bytes twice") {
    const std::vector<c::LocalRecord> records{{0, u"Mixed", false, {{10}, {137}}, {{u"Old", 0}, {u"New", 1}}}};
    const c::SchemaLimits schemaLimits{8, 64, 128, 128, 8192};
    auto local = c::LocalSchema::create(records, schemaLimits, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    auto plan = p::StreamDataContext::create(
        *local, std::vector<p::WireRecordDescription>{{0, u"Mixed", {{u"Old", 10}, {u"New", 137}}}},
        {schemaLimits, 8, {256, 128, 8192}});
    REQUIRE(plan);
    const auto input = payload(bytes({2, 0x41, 0x42, 2, 0xc0, 0x80}), false);
    const auto result = decode(input, *plan);
    REQUIRE(result.status == p::StreamDataStatus::OK);
    CHECK(result.fieldBytes == 3);
    CHECK(result.fieldUnits == 4);
    CHECK(result.ownedFieldUnits == 3);
    CHECK(result.payloadEncodedBytes == 3);
    CHECK(result.payloadWorkBytes == 2);
    CHECK(result.ownedPayloadBytes == 2);

    for (const auto allowance : {std::size_t{0}, std::size_t{3}, std::size_t{4}, std::size_t{5}}) {
        auto budget = limits();
        budget.fieldUnits = allowance;
        CHECK(decode(input, *plan, budget).status ==
              (allowance >= 4 ? p::StreamDataStatus::OK : p::StreamDataStatus::RESOURCE_LIMIT));
        budget = limits();
        budget.ownedFieldUnits = allowance;
        CHECK(decode(input, *plan, budget).status ==
              (allowance >= 4 ? p::StreamDataStatus::OK : p::StreamDataStatus::RESOURCE_LIMIT));
    }
}

TEST_CASE("completed current and temporary payload storage must fit together at the exact nine-byte peak") {
    const auto plan = context(9, {{u"Value", 9}, {u"skip", 9}, {u"Value", 9}, {u"Sentinel", 8}});
    auto body = field(3, bytes({1, 2, 3}));
    const auto next = field(3, bytes({4, 5, 6}));
    body.insert(body.end(), next.begin(), next.end());
    body.insert(body.end(), next.begin(), next.end());
    const auto first = payload(body);
    auto two = first;
    two.insert(two.end(), first.begin(), first.end());

    for (const auto allowance : {std::size_t{8}, std::size_t{9}, std::size_t{10}}) {
        auto budget = limits();
        budget.ownedPayloadBytes = allowance;
        const auto result = decode(two, plan, budget);
        CHECK(result.status == (allowance >= 9 ? p::StreamDataStatus::OK : p::StreamDataStatus::RESOURCE_LIMIT));
        CHECK(result.records.size() == (allowance >= 9 ? 2 : 1));
        CHECK(result.ownedPayloadBytes == (allowance >= 9 ? 6 : 3));
        CHECK(result.payloadWorkBytes == (allowance >= 9 ? 18 : 15));
        CHECK(result.payloadEncodedBytes == (allowance >= 9 ? 24 : 17));

        if (allowance == 8) {
            CHECK(result.stage == p::StreamDataStage::FIELD);
            CHECK(result.errorOffset == first.size() + 7);
            CHECK(result.consumed == first.size() + 8);
            CHECK(result.records[0].typedValues[0].bytes == bytes({4, 5, 6}));
        }
    }
}

TEST_CASE("all four new payload identities coexist with old strings and exact scalar sentinel") {
    const c::SchemaLimits schemaLimits{8, 64, 128, 128, 8192};
    const std::vector<c::LocalRecord> records{
        {0,
         u"Mixed",
         false,
         {{9}, {137}, {233}, {249}, {10}, {8}},
         {{u"Bytes", 0}, {u"Text", 1}, {u"Custom", 2}, {u"Serial", 3}, {u"Old", 4}, {u"Sentinel", 5}}}};
    auto local = c::LocalSchema::create(records, schemaLimits, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    const std::vector<p::WireRecordDescription> descriptions{
        {0,
         u"Mixed",
         {{u"Bytes", 9}, {u"Text", 137}, {u"Custom", 233}, {u"Serial", 249}, {u"Old", 10}, {u"Sentinel", 8}}}};
    auto plan = p::StreamDataContext::create(*local, descriptions, {schemaLimits, 8, {256, 128, 8192}});
    REQUIRE(plan);
    const auto body = bytes({2, 0, 0xff, 4, 0xf0, 0x9f, 0x98, 0x80, 1, 0xab, 0xf1, 0, 0, 0, 1, 0xcd, 1, 0x41});
    const auto result = decode(payload(body), *plan);
    REQUIRE(result.status == p::StreamDataStatus::OK);
    REQUIRE(result.records.size() == 1);
    const auto &values = result.records[0].typedValues;
    CHECK(values[0].bytes == bytes({0, 0xff}));
    CHECK(values[1].units == u"\U0001f600");
    CHECK(values[2].bytes == bytes({0xab}));
    CHECK(values[3].bytes == bytes({0xcd}));
    CHECK(values[4].units == u"A");
    CHECK(values[5].raw == 77);
    CHECK(result.payloadEncodedBytes == 16);
    CHECK(result.payloadWorkBytes == 8);
    CHECK(result.ownedPayloadBytes == 8);
    CHECK(result.fieldBytes == 2);
    CHECK(result.fieldUnits == 5);
    CHECK(result.ownedFieldUnits == 3);
}

TEST_CASE("allocation failures translate locally while body work and completed prefix survive") {
    const auto input = payload(field(4096, Bytes(4096, std::byte{0x41})));

    for (const auto type : {9, 137, 249}) {
        const auto plan = context(type);
        bool payloadFailure = false, outputFailure = false, success = false;

        for (std::ptrdiff_t checkpoint = 0; checkpoint < 12; ++checkpoint) {
            allocationFailAfter = checkpoint;
            auto result = decode(input, plan);
            allocationFailAfter = -1;

            if (result.status == p::StreamDataStatus::ALLOCATION_FAILURE) {
                CHECK(result.records.empty());

                if (result.payloadWorkBytes == 4096) {
                    payloadFailure = true;
                    CHECK((result.stage == p::StreamDataStage::FIELD || result.stage == p::StreamDataStage::OUTPUT));
                    CHECK(result.ownedPayloadBytes == 0);

                    if (result.stage == p::StreamDataStage::OUTPUT)
                        outputFailure = true;
                }
            } else {
                CHECK(result.status == p::StreamDataStatus::OK);
                success = true;
            }
        }

        CHECK(payloadFailure);
        CHECK(outputFailure);
        CHECK(success);
        auto two = input;
        two.insert(two.end(), input.begin(), input.end());
        bool laterFailure = false;

        for (std::ptrdiff_t checkpoint = 0; checkpoint < 20; ++checkpoint) {
            allocationFailAfter = checkpoint;
            auto result = decode(two, plan);
            allocationFailAfter = -1;

            if (result.status == p::StreamDataStatus::ALLOCATION_FAILURE && !result.records.empty()) {
                laterFailure = true;
                CHECK(result.records.size() == 1);
                CHECK(result.ownedPayloadBytes == 4096 * (type == 137 ? 2 : 1));
                CHECK(result.payloadWorkBytes >= 4096);
                CHECK(c::validFieldValue(result.records[0].typedValues[0]));
            }
        }

        CHECK(laterFailure);
    }

    p::StreamDataRecord saved;
    {
        auto temporary = decode(input, context(9));
        REQUIRE(temporary.status == p::StreamDataStatus::OK);
        saved = temporary.records[0];
        temporary.records[0].typedValues[0].bytes->front() = std::byte{0};
    }
    REQUIRE(saved.typedValues[0].bytes);
    CHECK(saved.typedValues[0].bytes->front() == std::byte{0x41});
    CHECK(saved.typedValues[0].bytes->size() == 4096);
}

TEST_CASE("new read-only profile refuses every outgoing layout before allocating including empty and old-only") {
    for (const auto type : {-1, 8, 9, 137, 233, 249}) {
        std::vector<c::LocalRecord> records;

        if (type >= 0)
            records.push_back({0, u"Objects", false, {{type}}, {{u"Value", 0}}});

        auto local = c::LocalSchema::create(records, {8, 64, 128, 128, 8192}, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(local);
        allocationFailAfter = 0;
        const auto result = p::OutgoingLayouts::create(*local, {}, {});
        allocationFailAfter = -1;
        REQUIRE_FALSE(result);
        CHECK(result.error().error == p::WriterError::UNSUPPORTED);
        CHECK(result.error().stage == "profile");
    }
}

TEST_CASE("huge full lengths reject before narrowing or admitting proportional payload work") {
    for (const auto type : {9, 137, 233}) {
        auto budget = limits();
        budget.fieldPayloadEncodedBytes = budget.fieldPayloadBytes = budget.payloadEncodedBytes =
            budget.payloadWorkBytes = budget.ownedPayloadBytes = std::numeric_limits<std::size_t>::max();

        for (const auto count :
             {std::numeric_limits<std::int64_t>::max(), std::int64_t{0x80000000}, std::int64_t{0x200000000}}) {
            const auto result = decode(payload(field(count)), context(type), budget);
            CHECK(result.status == p::StreamDataStatus::INVALID_FIELD);
            CHECK(result.payloadWorkBytes == 0);
            CHECK(result.ownedPayloadBytes == 0);
        }
    }
}
