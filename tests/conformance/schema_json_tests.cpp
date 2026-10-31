// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/schema/json_schema.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <simdjson.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
bool allocationFault = false;       ///< Test-only ordinary-allocation fault injection, serialized with the loader.
std::size_t allowedAllocations = 0; ///< Requests allowed before the selected operation fails.
std::size_t requestedBytes = 0;     ///< Successful ordinary request payloads in the selected observation interval.
std::size_t liveRequestedBytes = 0; ///< Ordinary request payloads still owned by this test executable.
std::size_t peakRequestedBytes = 0; ///< Maximum simultaneous ordinary payload bytes in the observation interval.

/** Aligned bookkeeping for an ordinary test allocation, excluded from observed request payloads. */
struct alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) AllocationHeader {
    std::size_t bytes; ///< Original ordinary operator-new request.
};
} // namespace

/** Inject/observe ordinary allocation requests without changing production allocators.
 * @param bytes Requested ordinary payload size.
 * @return Aligned owned storage, or bad_alloc before ownership when injection is armed.
 */
void *operator new(std::size_t bytes) {
    if (allocationFault) {
        if (allowedAllocations == 0) {
            throw std::bad_alloc{};
        }

        --allowedAllocations;
    }

    if (bytes > std::numeric_limits<std::size_t>::max() - sizeof(AllocationHeader)) {
        throw std::bad_alloc{};
    }

    auto *header = static_cast<AllocationHeader *>(std::malloc(sizeof(AllocationHeader) + bytes));

    if (!header) {
        throw std::bad_alloc{};
    }

    header->bytes = bytes;
    requestedBytes += bytes;
    liveRequestedBytes += bytes;
    peakRequestedBytes = std::max(peakRequestedBytes, liveRequestedBytes);

    return header + 1;
}

/** Allocate one ordinary test array through the same observed request boundary.
 * @param bytes Requested payload size.
 * @return Owned storage, or bad_alloc.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** Observe nonthrowing ordinary allocations, including pinned parser failure paths.
 * @param bytes Requested payload size.
 * @param tag Standard nonthrowing selection, unused.
 * @return Owned storage, or null on failure.
 */
void *operator new(std::size_t bytes, const std::nothrow_t &tag) noexcept {
    (void)tag;

    try {
        return ::operator new(bytes);
    } catch (...) {
        return nullptr;
    }
}

/** Observe nonthrowing ordinary arrays.
 * @param bytes Requested payload size.
 * @param tag Standard nonthrowing selection.
 * @return Owned storage, or null on failure.
 */
void *operator new[](std::size_t bytes, const std::nothrow_t &tag) noexcept {
    return ::operator new(bytes, tag);
}

/** Release one observed ordinary test allocation.
 * @param pointer Previously returned storage, or null.
 */
void operator delete(void *pointer) noexcept {
    if (pointer) {
        auto *header = static_cast<AllocationHeader *>(pointer) - 1;
        liveRequestedBytes -= header->bytes;
        std::free(header);
    }
}

/** Release an observed ordinary array.
 * @param pointer Previously allocated array, or null.
 */
void operator delete[](void *pointer) noexcept {
    ::operator delete(pointer);
}

/** Release sized ordinary storage without relying on its implementation-supplied size.
 * @param pointer Previously allocated storage, or null.
 * @param bytes Implementation-supplied size, unused.
 */
void operator delete(void *pointer, std::size_t bytes) noexcept {
    (void)bytes;
    ::operator delete(pointer);
}

/** Release sized array storage.
 * @param pointer Previously allocated array, or null.
 * @param bytes Implementation-supplied size, unused.
 */
void operator delete[](void *pointer, std::size_t bytes) noexcept {
    (void)bytes;
    ::operator delete(pointer);
}

/** Release ordinary storage after a nonthrowing constructor failure.
 * @param pointer Allocated storage, or null.
 * @param tag Standard nonthrowing selection, unused.
 */
void operator delete(void *pointer, const std::nothrow_t &tag) noexcept {
    (void)tag;
    ::operator delete(pointer);
}

/** Release array storage after a nonthrowing constructor failure.
 * @param pointer Allocated storage, or null.
 * @param tag Standard nonthrowing selection, unused.
 */
void operator delete[](void *pointer, const std::nothrow_t &tag) noexcept {
    (void)tag;
    ::operator delete(pointer);
}

namespace org::ttldtor::qdcxx::schema {
namespace {

/** Wrap caller records in the required native identity.
 * @param records Complete JSON array spelling.
 * @return Owned document for loader tests.
 */
std::string document(std::string_view records = "[]") {
    return R"({"formatVersion":1,"profile":{"id":"qdcxx-native","version":1},"records":)" + std::string(records) + "}";
}

/** Create one minimal ordinary record, exposing slots for adversarial member spellings.
 * @param name Complete JSON name spelling.
 * @param fields Complete fields array spelling.
 * @param lookups Complete lookup array spelling.
 * @return One-record native document.
 */
std::string oneRecord(std::string_view name = R"("R")", std::string_view fields = R"([{"serialTypeId":8}])",
                      std::string_view lookups = R"([{"name":"F","fieldId":0}])") {
    return document(R"([{"id":0,"name":)" + std::string(name) + R"(,"hasTime":false,"fields":)" + std::string(fields) +
                    R"(,"lookups":)" + std::string(lookups) + "}]");
}

/** Replace one fixture fragment without silently accepting a misspelled test precondition.
 * @param input Owned source fixture.
 * @param from Existing fragment, required to occur.
 * @param to Replacement fragment.
 * @return Mutated document.
 */
std::string replace(std::string input, std::string_view from, std::string_view to) {
    const auto position = input.find(from);
    REQUIRE(position != std::string::npos);
    input.replace(position, from.size(), to);

    return input;
}

/** Supply an explicitly selected binary document without changing legacy fixture helpers.
 * @param records Complete record-array JSON.
 * @return Owned binary-profile document.
 */
std::string binaryDocument(std::string_view records = "[]") {
    return R"({"formatVersion":1,"profile":{"id":"qdcxx-native-binary","version":1},"records":)" +
           std::string(records) + "}";
}

/** Construct complete logical field/lookup metadata with an explicit time layout.
 * @param types Full field identities.
 * @param layout Complete layout JSON, or null for an ordinary record.
 * @return One-record binary document with complete explicit keys.
 */
std::string binaryRecord(std::span<const int> types,
                         std::string_view layout = R"({"kind":"pair-int32","highFieldId":0,"lowFieldId":1})") {
    std::string fields = "[";
    std::string lookups = "[";

    for (std::size_t i = 0; i < types.size(); ++i) {
        if (i != 0) {
            fields += ',';
            lookups += ',';
        }

        fields += "{\"serialTypeId\":" + std::to_string(types[i]) + "}";
        lookups += "{\"name\":\"F" + std::to_string(i) + "\",\"fieldId\":" + std::to_string(i) + "}";
    }

    return binaryDocument("[{\"id\":0,\"name\":\"R\",\"hasTime\":" + std::string(layout == "null" ? "false" : "true") +
                          ",\"fields\":" + fields + "],\"lookups\":" + lookups +
                          "],\"indexedTimeLayout\":" + std::string(layout) + "}]");
}

/** Check one complete binary-profile failure without retaining its input in the result.
 * @param input Complete document.
 * @param error Required stable category.
 * @param limits Explicit independent loading budgets.
 */
void binaryError(std::string_view input, JsonSchemaError error, BinaryJsonSchemaLimits limits = {}) {
    const auto loaded = loadBinaryJsonSchema(input, limits);
    CAPTURE(input);
    REQUIRE_FALSE(loaded);
    CHECK(loaded.error() == error);
}

/** Assert the complete load fails with the selected stable category.
 * @param input Complete candidate document.
 * @param expected Required typed failure.
 * @param limits Explicit finite bounds.
 */
void checkError(std::string_view input, JsonSchemaError expected, JsonSchemaLimits limits = {}) {
    const auto result = loadJsonSchema(input, limits);
    CAPTURE(input);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == expected);
}

} // namespace

TEST_CASE("native JSON owns exact names, aliases and independent schema contexts") {
    auto input = oneRecord(R"("R\u0000\uD83D\uDE00")", R"([{"serialTypeId":10}])",
                           R"([{"name":"F","fieldId":0},{"name":"Alias","fieldId":0}])");
    auto first = loadJsonSchema(input);
    auto second = loadJsonSchema(input);
    REQUIRE(first);
    REQUIRE(second);
    input.assign(input.size(), 'x');
    input.clear();
    input.shrink_to_fit();
    CHECK(first->profile() == core::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(first->records().size() == 1);
    CHECK(first->records()[0].name == std::u16string({u'R', u'\0', 0xd83d, 0xde00}));
    CHECK(first->records()[0].fields[0].serialTypeId == 10);
    REQUIRE(first->records()[0].lookups.size() == 2);
    CHECK(first->records()[0].lookups[1].name == u"Alias");
    CHECK(first->records()[0].lookups[1].fieldId == 0);
    CHECK_FALSE(first->recordContext(0).matches(second->recordContext(0)));
    CHECK(first->recordContext(0).matches(first->recordContext(0)));
    CHECK(loadJsonSchema(document()));
    CHECK(loadJsonSchema(oneRecord(R"("R")", "[]", "[]")));
    CHECK(loadJsonSchema(replace(oneRecord(), "\"id\":0", "\"id\":-0")));
    CHECK(loadJsonSchema(" \r\n\t" + document() + " \r\n\t"));
    CHECK(loadJsonSchema(R"({"records":[],"profile":{"version":1,"id":"qdcxx-native"},"formatVersion":1})"));
}

TEST_CASE("native JSON accepts only the eight full type identities") {
    for (const auto type : {2, 8, 10, 40, 56, 72, 88, 120}) {
        const auto loaded = loadJsonSchema(oneRecord(R"("R")", "[{\"serialTypeId\":" + std::to_string(type) + "}]"));
        REQUIRE(loaded);
        CHECK(loaded->records()[0].fields[0].serialTypeId == type);
    }

    for (const auto type : {0, 1, 9, 24, 137, 264, 2147483647}) {
        checkError(oneRecord(R"("R")", "[{\"serialTypeId\":" + std::to_string(type) + "}]"),
                   JsonSchemaError::UNSUPPORTED_TYPE);
    }
}

TEST_CASE("native JSON rejects malformed syntax and Unicode") {
    for (const auto input : {"", " \t\n", "{", "{}{}", "/*x*/{}", "{\"a\":1,}", "[1,]", "{\"x\":01}", "{\"x\":+1}",
                             "{\"x\":1.}", "{\"x\":1e}", "{\"x\":NaN}", "{\"x\":Infinity}"}) {
        checkError(input, JsonSchemaError::MALFORMED_JSON);
    }

    for (const auto name : {R"("\uD800")", R"("\uDC00")", R"("\uD800x")", R"("\uD800\uD800")", R"("\x41")"}) {
        checkError(oneRecord(name), JsonSchemaError::MALFORMED_JSON);
    }

    checkError(std::string("\xef\xbb\xbf") + document(), JsonSchemaError::MALFORMED_JSON);
    checkError(oneRecord(std::string("\"") + "\xc0\xaf" + "\""), JsonSchemaError::MALFORMED_JSON);
    checkError(oneRecord(std::string("\"") + "\xed\xa0\x80" + "\""), JsonSchemaError::MALFORMED_JSON);
    checkError(oneRecord(std::string("\"") + "\xf4\x90\x80\x80" + "\""), JsonSchemaError::MALFORMED_JSON);
    checkError(oneRecord(std::string("\"a\0b\"", 5)), JsonSchemaError::MALFORMED_JSON);
}

TEST_CASE("native JSON distinguishes numeric schema failures from numeric syntax") {
    for (const auto value : {"true", "null", "\"1\"", "1.0", "1e0", "-1", "2147483648", "9223372036854775807",
                             "18446744073709551615", "1e-9999"}) {
        checkError(replace(document(), "\"formatVersion\":1", "\"formatVersion\":" + std::string(value)),
                   JsonSchemaError::INVALID_SCHEMA);
    }

    for (const auto value : {"99999999999999999999999x", "18446744073709551616x", "1e9999x", "99999999999999999999999.",
                             "99999999999999999999999e+", "01", "--1", "18446744073709551616",
                             "999999999999999999999999999999", "-9223372036854775809", "1e9999"}) {
        checkError(replace(document(), "\"formatVersion\":1", "\"formatVersion\":" + std::string(value)),
                   JsonSchemaError::MALFORMED_JSON);
    }

    for (const auto member : {"\"id\":0", "\"serialTypeId\":8", "\"fieldId\":0"}) {
        const std::string prefix = std::string(member).substr(0, std::string(member).find(':') + 1);
        checkError(replace(oneRecord(), member, prefix + "1.0"), JsonSchemaError::INVALID_SCHEMA);
        checkError(replace(oneRecord(), member, prefix + "2147483648"), JsonSchemaError::INVALID_SCHEMA);
    }
}

TEST_CASE("native JSON rejects unknown, missing, duplicate and wrong-kind members at each object level") {
    for (const auto input : {"null", "[]", "true", "1", "\"schema\"", "{}"}) {
        checkError(input, JsonSchemaError::INVALID_SCHEMA);
    }

    const auto source = oneRecord();
    const std::array<std::pair<std::string_view, std::string_view>, 5> members{
        {{"\"formatVersion\":1", "\"formatVersion\":1,\"formatVersion\":1"},
         {"\"version\":1", "\"version\":1,\"versi\\u006fn\":1"},
         {"\"name\":\"R\"", "\"name\":\"R\",\"na\\u006de\":\"R\""},
         {"\"serialTypeId\":8", "\"serialTypeId\":8,\"serialTypeId\":8"},
         {"\"fieldId\":0", "\"fieldId\":0,\"fieldId\":0"}}};

    for (const auto &[from, duplicate] : members) {
        checkError(replace(source, from, duplicate), JsonSchemaError::INVALID_SCHEMA);
        checkError(replace(source, from, "\"unknown\":null," + std::string(from)), JsonSchemaError::INVALID_SCHEMA);
        checkError(replace(source, from, "\"unknown\":null"), JsonSchemaError::INVALID_SCHEMA);
        const auto colon = from.find(':');
        checkError(replace(source, from, std::string(from.substr(0, colon + 1)) + "null"),
                   JsonSchemaError::INVALID_SCHEMA);
    }

    for (const auto removed :
         {"\"formatVersion\":1,", "\"profile\":{\"id\":\"qdcxx-native\",\"version\":1},", "\"id\":\"qdcxx-native\",",
          ",\"version\":1", "\"id\":0,", "\"name\":\"R\",", "\"hasTime\":false,", "\"fields\":[{\"serialTypeId\":8}],",
          "\"serialTypeId\":8", "\"name\":\"F\",", ",\"fieldId\":0"}) {
        checkError(replace(source, removed, ""), JsonSchemaError::INVALID_SCHEMA);
    }

    checkError(replace(source, ",\"lookups\":[{\"name\":\"F\",\"fieldId\":0}]", ""), JsonSchemaError::INVALID_SCHEMA);
    checkError(replace(document(), ",\"records\":[]", ""), JsonSchemaError::INVALID_SCHEMA);
    checkError(replace(source, "\"profile\":{\"id\":\"qdcxx-native\",\"version\":1}", "\"profile\":null"),
               JsonSchemaError::INVALID_SCHEMA);
    checkError(replace(source, "\"id\":\"qdcxx-native\"", "\"id\":1"), JsonSchemaError::INVALID_SCHEMA);
    checkError(replace(source, "\"name\":\"R\"", "\"name\\u0000\":\"R\""), JsonSchemaError::INVALID_SCHEMA);

    for (const auto member :
         {"\"hasTime\":false", "\"fields\":[{\"serialTypeId\":8}]", "\"lookups\":[{\"name\":\"F\",\"fieldId\":0}]"}) {
        checkError(replace(source, member, std::string(member).substr(0, std::string(member).find(':') + 1) + "null"),
                   JsonSchemaError::INVALID_SCHEMA);
    }

    checkError(replace(document(), "[]", "{}"), JsonSchemaError::INVALID_SCHEMA);
    checkError(document("[null]"), JsonSchemaError::INVALID_SCHEMA);
    checkError(oneRecord(R"("R")", "[null]"), JsonSchemaError::INVALID_SCHEMA);
    checkError(oneRecord(R"("R")", R"([{"serialTypeId":8}])", "[null]"), JsonSchemaError::INVALID_SCHEMA);
}

TEST_CASE("native JSON rejects unsupported identities before record interpretation") {
    checkError(replace(document(), "\"formatVersion\":1", "\"formatVersion\":2"), JsonSchemaError::UNSUPPORTED_VERSION);
    checkError(replace(document(), "\"version\":1", "\"version\":2"), JsonSchemaError::UNSUPPORTED_VERSION);
    checkError(replace(document(), "qdcxx-native", "unknown"), JsonSchemaError::UNSUPPORTED_PROFILE);
    checkError(
        R"({"formatVersion":1,"profile":{"id":"qd-3.355-xml-captured","version":1},"provenance":{},"records":null})",
        JsonSchemaError::UNSUPPORTED_PROFILE);
    checkError(replace(oneRecord(), "\"hasTime\":false", "\"hasTime\":true"), JsonSchemaError::UNSUPPORTED_INDEXED);
    checkError(replace(oneRecord(), "\"hasTime\":false", "\"hasTime\":1"), JsonSchemaError::INVALID_SCHEMA);
    checkError(replace(document("[null]"), "qdcxx-native", "unknown"), JsonSchemaError::UNSUPPORTED_PROFILE);
}

TEST_CASE("native JSON delegates exact local invariants to core") {
    checkError(replace(oneRecord(), "\"id\":0", "\"id\":1"), JsonSchemaError::INVALID_SCHEMA);
    checkError(oneRecord(R"("")"), JsonSchemaError::INVALID_SCHEMA);
    checkError(oneRecord(R"("R")", R"([{"serialTypeId":8}])", "[]"), JsonSchemaError::INVALID_SCHEMA);
    checkError(oneRecord(R"("R")", "[]"), JsonSchemaError::INVALID_SCHEMA);
    checkError(replace(oneRecord(), "\"fieldId\":0", "\"fieldId\":1"), JsonSchemaError::INVALID_SCHEMA);
    checkError(replace(oneRecord(), "\"name\":\"F\"", "\"name\":\"\""), JsonSchemaError::INVALID_SCHEMA);
    checkError(
        oneRecord(R"("R")", R"([{"serialTypeId":8}])", R"([{"name":"F","fieldId":0},{"name":"\u0046","fieldId":0}])"),
        JsonSchemaError::INVALID_SCHEMA);
    const auto records =
        R"([{"id":0,"name":"R","hasTime":false,"fields":[],"lookups":[]},{"id":1,"name":"\u0052","hasTime":false,"fields":[],"lookups":[]}])";
    checkError(document(records), JsonSchemaError::INVALID_SCHEMA);
    CHECK(loadJsonSchema(replace(document(records), "\\u0052", "Other")));
    CHECK(
        loadJsonSchema(oneRecord(R"(". R .")", R"([{"serialTypeId":8}])",
                                 R"([{"name":"F","fieldId":0},{"name":"f","fieldId":0},{"name":" F ","fieldId":0}])")));
    const auto literal = loadJsonSchema(oneRecord("\"\xc3\xa9\xf0\x9f\x98\x80\""));
    const auto escaped = loadJsonSchema(oneRecord(R"("\u00e9\ud83d\ude00")"));
    REQUIRE(literal);
    REQUIRE(escaped);
    CHECK(literal->records()[0].name == escaped->records()[0].name);
}

TEST_CASE("native JSON enforces exact byte and container depth budgets") {
    auto limits = JsonSchemaLimits{};
    const auto input = oneRecord();
    limits.documentBytes = input.size();
    limits.depth = 5;
    CHECK(loadJsonSchema(input, limits));
    limits.documentBytes--;
    checkError(input, JsonSchemaError::RESOURCE_LIMIT, limits);
    limits.documentBytes = 0;
    checkError(input, JsonSchemaError::RESOURCE_LIMIT, limits);
    limits = {};
    limits.depth = 4;
    checkError(input, JsonSchemaError::RESOURCE_LIMIT, limits);
    limits.depth = 0;
    checkError("{}", JsonSchemaError::RESOURCE_LIMIT, limits);
    limits.depth = 1;
    checkError(document(), JsonSchemaError::RESOURCE_LIMIT, limits);
    limits.depth = 2;
    CHECK(loadJsonSchema(document(), limits));
    limits.depth = 4;
    CHECK(loadJsonSchema(oneRecord(R"("R")", "[]", "[]"), limits));
    checkError(replace(document(), "[]", "[[[[]]]]"), JsonSchemaError::RESOURCE_LIMIT, limits);
    checkError(replace(document(), "\"records\":[]", "\"records\":[],\"unknown\":[[[[]]]]"),
               JsonSchemaError::RESOURCE_LIMIT, limits);
    limits.depth = std::numeric_limits<std::size_t>::max();
    CHECK(loadJsonSchema(input, limits));
    const auto quotedContainers = oneRecord(R"("[{}]\\\"[]")");
    CHECK(loadJsonSchema(quotedContainers, limits));
}

TEST_CASE("native JSON checks zero exact and cumulative schema budgets") {
    const auto input = oneRecord(R"("R\ud83d\ude00")");
    JsonSchemaLimits exact{};
    exact.schema = {1, 1, 1, 3, 4};
    CHECK(loadJsonSchema(input, exact));

    for (const auto member : {&core::SchemaLimits::records, &core::SchemaLimits::fields, &core::SchemaLimits::lookups,
                              &core::SchemaLimits::nameUnits, &core::SchemaLimits::totalNameUnits}) {
        auto limits = exact;
        --(limits.schema.*member);
        checkError(input, JsonSchemaError::RESOURCE_LIMIT, limits);
        limits.schema.*member = 0;
        checkError(input, JsonSchemaError::RESOURCE_LIMIT, limits);
    }

    exact.schema = {0, 0, 0, 0, 0};
    CHECK(loadJsonSchema(document(), exact));
    const auto two = document(
        R"([{"id":0,"name":"A","hasTime":false,"fields":[{"serialTypeId":8}],"lookups":[{"name":"F","fieldId":0}]},{"id":1,"name":"B","hasTime":false,"fields":[{"serialTypeId":10}],"lookups":[{"name":"F","fieldId":0}]}])");
    exact.schema = {2, 2, 2, 1, 4};
    CHECK(loadJsonSchema(two, exact));

    for (const auto member :
         {&core::SchemaLimits::fields, &core::SchemaLimits::lookups, &core::SchemaLimits::totalNameUnits}) {
        auto limits = exact;
        --(limits.schema.*member);
        checkError(two, JsonSchemaError::RESOURCE_LIMIT, limits);
    }

    const auto keyUnits =
        oneRecord(R"("R")", R"([{"serialTypeId":8}])", R"([{"name":"\u00e9\ud83d\ude00","fieldId":0}])");
    exact.schema = {1, 1, 1, 3, 4};
    CHECK(loadJsonSchema(keyUnits, exact));
    exact.schema.nameUnits = 2;
    checkError(keyUnits, JsonSchemaError::RESOURCE_LIMIT, exact);
    exact.schema.nameUnits = 3;
    exact.schema.totalNameUnits = 3;
    checkError(keyUnits, JsonSchemaError::RESOURCE_LIMIT, exact);
}

TEST_CASE("native JSON diagnostic tokens are stable static text") {
    const std::array errors{JsonSchemaError::MALFORMED_JSON,      JsonSchemaError::INVALID_SCHEMA,
                            JsonSchemaError::UNSUPPORTED_PROFILE, JsonSchemaError::UNSUPPORTED_VERSION,
                            JsonSchemaError::UNSUPPORTED_TYPE,    JsonSchemaError::UNSUPPORTED_INDEXED,
                            JsonSchemaError::RESOURCE_LIMIT,      JsonSchemaError::ALLOCATION_FAILURE};
    const std::array<std::string_view, 8> names{"MALFORMED_JSON",      "INVALID_SCHEMA",    "UNSUPPORTED_PROFILE",
                                                "UNSUPPORTED_VERSION", "UNSUPPORTED_TYPE",  "UNSUPPORTED_INDEXED",
                                                "RESOURCE_LIMIT",      "ALLOCATION_FAILURE"};

    for (std::size_t i = 0; i < errors.size(); ++i) {
        CHECK(jsonSchemaErrorName(errors[i]) == names[i]);
    }
}

TEST_CASE("binary JSON selects independent identity and owns all forty-nine pair layouts") {
    for (const auto high : {4, 8, 24, 40, 56, 72, 88}) {
        for (const auto low : {4, 8, 24, 40, 56, 72, 88}) {
            const std::array types{low, 137, high, 233};
            auto input = binaryRecord(types, R"({"kind":"pair-int32","highFieldId":2,"lowFieldId":0})");
            auto loaded = loadBinaryJsonSchema(input);
            auto other = loadBinaryJsonSchema(input);
            REQUIRE(loaded);
            REQUIRE(other);
            input.assign(input.size(), 'x');
            input.clear();
            input.shrink_to_fit();
            CHECK(loaded->profile() == core::LocalProfile::BINARY_FIELDS_V1);
            const auto &record = loaded->records()[0];
            REQUIRE(record.indexedTimeLayout);
            const auto &pair = std::get<core::IndexedTimePairInt32>(*record.indexedTimeLayout);
            CHECK(pair.firstField == 2);
            CHECK(pair.secondField == 0);
            CHECK(record.fields[2].serialTypeId == high);
            CHECK(record.fields[0].serialTypeId == low);
            CHECK_FALSE(loaded->sharesIdentityWith(*other));
            auto copy = *loaded;
            CHECK(copy.sharesIdentityWith(*loaded));
            CHECK(copy.recordContext(0).matches(loaded->recordContext(0)));
        }
    }

    binaryError(document(), JsonSchemaError::UNSUPPORTED_PROFILE);
    checkError(binaryDocument(), JsonSchemaError::UNSUPPORTED_PROFILE);
    checkError(replace(document(), "\"formatVersion\":1", "\"formatVersion\":2"), JsonSchemaError::UNSUPPORTED_VERSION);
    binaryError(replace(binaryDocument(), "\"version\":1", "\"version\":2"), JsonSchemaError::UNSUPPORTED_VERSION);
    binaryError(replace(binaryDocument("[null]"), "qdcxx-native-binary", "unknown"),
                JsonSchemaError::UNSUPPORTED_PROFILE);
}

TEST_CASE("binary JSON single64 and ordinary records retain the complete exact field catalog") {
    const std::array catalog{0, 1, 2, 3, 4, 8, 9, 10, 24, 40, 56, 72, 88, 104, 120, 137, 152, 233, 249};

    for (const auto type : {104, 120, 152}) {
        std::vector<int> types(catalog.begin(), catalog.end());
        types.push_back(type);
        auto loaded = loadBinaryJsonSchema(binaryRecord(types, R"({"kind":"single-int64","fieldId":19})"));
        REQUIRE(loaded);
        REQUIRE(loaded->records()[0].indexedTimeLayout);
        CHECK(std::get<core::IndexedTimeSingleInt64>(*loaded->records()[0].indexedTimeLayout).field == 19);
        CHECK(loaded->records()[0].fields.size() == 20);
    }

    auto ordinary = loadBinaryJsonSchema(binaryRecord(catalog, "null"));
    REQUIRE(ordinary);
    CHECK_FALSE(ordinary->records()[0].hasTime);
    CHECK_FALSE(ordinary->records()[0].indexedTimeLayout);
    CHECK(loadBinaryJsonSchema(binaryDocument()));
    CHECK(loadBinaryJsonSchema(binaryRecord({}, "null")));
}

TEST_CASE("binary JSON rejects partial duplicate mismatched and unsupported indexing atomically") {
    const std::array types{56, 72};
    const auto valid = binaryRecord(types);

    for (const auto layout : {"null", "{}", R"({"kind":"pair-int32","highFieldId":0})",
                              R"({"kind":"pair-int32","highFieldId":0,"lowFieldId":0})",
                              R"({"kind":"pair-int32","highFieldId":2,"lowFieldId":0})",
                              R"({"kind":"pair-int32","highFieldId":0,"lowFieldId":1,"fieldId":0})",
                              R"({"kind":"pair-int32","highFieldId":0,"highField\u0049d":1,"lowFieldId":1})",
                              R"({"kind":"single-int64"})", R"({"kind":"single-int64","fieldId":2})"}) {
        binaryError(replace(valid, R"({"kind":"pair-int32","highFieldId":0,"lowFieldId":1})", layout),
                    JsonSchemaError::INVALID_SCHEMA);
    }

    for (const auto number : {"-1", "1.0", "1e0", "2147483648", "null", "true", "\"0\""}) {
        binaryError(replace(valid, "\"highFieldId\":0", "\"highFieldId\":" + std::string(number)),
                    JsonSchemaError::INVALID_SCHEMA);
    }

    for (const auto type : {0, 1, 2, 3, 9, 10, 104, 120, 137, 152, 233, 249}) {
        const std::array wrong{type, 8};
        binaryError(binaryRecord(wrong), JsonSchemaError::UNSUPPORTED_INDEXED);
    }

    binaryError(binaryRecord(types, R"({"kind":"single-int64","fieldId":0})"), JsonSchemaError::UNSUPPORTED_INDEXED);
    binaryError(binaryRecord(types, R"({"kind":"unknown"})"), JsonSchemaError::UNSUPPORTED_INDEXED);
    binaryError(replace(valid, "\"hasTime\":true", "\"hasTime\":false"), JsonSchemaError::INVALID_SCHEMA);
    binaryError(replace(valid, "\"serialTypeId\":56", "\"serialTypeId\":264"), JsonSchemaError::UNSUPPORTED_TYPE);
    binaryError(replace(valid, "\"fieldId\":1", "\"fieldId\":9"), JsonSchemaError::INVALID_SCHEMA);
    binaryError(replace(valid, "\"name\":\"F1\"", "\"name\":\"F0\""), JsonSchemaError::INVALID_SCHEMA);
    binaryError(replace(valid, "\"id\":0", "\"id\":1"), JsonSchemaError::INVALID_SCHEMA);
}

TEST_CASE("binary JSON independent budgets admit equality and reject one-short without changing old schemas") {
    const std::array types{56, 72};
    const auto valid = binaryRecord(types);
    auto old = loadJsonSchema(oneRecord());
    REQUIRE(old);
    const auto context = old->recordContext(0);

    for (const auto axis : {&BinaryJsonSchemaLimits::parserBytes, &BinaryJsonSchemaLimits::workUnits,
                            &BinaryJsonSchemaLimits::schemaModelBytes}) {
        std::size_t low = 0;
        std::size_t high = BinaryJsonSchemaLimits{}.*axis;

        while (low < high) {
            const auto middle = low + (high - low) / 2;
            BinaryJsonSchemaLimits limits;
            limits.*axis = middle;

            if (loadBinaryJsonSchema(valid, limits)) {
                high = middle;
            } else {
                low = middle + 1;
            }
        }

        REQUIRE(low > 0);
        BinaryJsonSchemaLimits exact;
        exact.*axis = low;
        REQUIRE(loadBinaryJsonSchema(valid, exact));
        --(exact.*axis);
        binaryError(valid, JsonSchemaError::RESOURCE_LIMIT, exact);
        exact.*axis = 0;
        binaryError(valid, JsonSchemaError::RESOURCE_LIMIT, exact);
        CHECK(context.matches(old->recordContext(0)));
    }

    BinaryJsonSchemaLimits huge;
    const auto maximum = std::numeric_limits<std::size_t>::max();
    huge.loading = {maximum, maximum, {maximum, maximum, maximum, maximum, maximum}};
    huge.parserBytes = maximum;
    huge.workUnits = maximum;
    huge.schemaModelBytes = maximum;
    CHECK(loadBinaryJsonSchema(binaryDocument(), huge));
    CHECK(loadBinaryJsonSchema(valid, huge));
    BinaryJsonSchemaLimits depth;
    depth.loading.depth = 4;
    binaryError(valid, JsonSchemaError::RESOURCE_LIMIT, depth);
    depth.loading.depth = 5;
    CHECK(loadBinaryJsonSchema(valid, depth));
    BinaryJsonSchemaLimits logical;
    logical.loading.documentBytes = valid.size();
    logical.loading.schema = {1, 2, 2, 2, 5};
    REQUIRE(loadBinaryJsonSchema(valid, logical));

    for (const auto member : {&core::SchemaLimits::records, &core::SchemaLimits::fields, &core::SchemaLimits::lookups,
                              &core::SchemaLimits::nameUnits, &core::SchemaLimits::totalNameUnits}) {
        auto shortLimit = logical;
        --(shortLimit.loading.schema.*member);
        binaryError(valid, JsonSchemaError::RESOURCE_LIMIT, shortLimit);
    }

    --logical.loading.documentBytes;
    binaryError(valid, JsonSchemaError::RESOURCE_LIMIT, logical);
    auto unicodeInput = replace(valid, "\"name\":\"R\"", R"("name":"R\u0000\uD83D\uDE00")");
    auto unicode = loadBinaryJsonSchema(unicodeInput);
    REQUIRE(unicode);
    unicodeInput.assign(unicodeInput.size(), 'x');
    CHECK(unicode->records()[0].name == std::u16string({u'R', u'\0', 0xd83d, 0xde00}));
}

TEST_CASE("binary JSON failed ordinary allocations release all staged storage and preserve old provenance") {
    const std::array types{104, 10, 137, 9, 233, 249};
    const auto input = binaryRecord(types, R"({"kind":"single-int64","fieldId":0})");
    auto old = loadJsonSchema(oneRecord());
    REQUIRE(old);
    const auto identity = old->recordContext(0);
    bool success = false;
    std::size_t failed = 0;

    for (std::size_t allowed = 0; allowed < 128; ++allowed) {
        const auto before = liveRequestedBytes;
        bool allocationError = false;
        {
            allowedAllocations = allowed;
            allocationFault = true;
            auto result = loadBinaryJsonSchema(input);
            allocationFault = false;
            success = result.has_value();
            allocationError = !result && result.error() == JsonSchemaError::ALLOCATION_FAILURE;
        }

        CHECK(liveRequestedBytes == before);
        CHECK(identity.matches(old->recordContext(0)));

        if (success) {
            break;
        }

        REQUIRE(allocationError);
        ++failed;
    }

    CHECK(success);
    CHECK(failed > 6);
}

TEST_CASE("ordinary allocation instrumentation preserves the default new alignment before typed access") {
    for (const std::size_t bytes : {0, 1, 8, 16, 64}) {
        void *storage = ::operator new(bytes);
        const auto remainder = reinterpret_cast<std::uintptr_t>(storage) % __STDCPP_DEFAULT_NEW_ALIGNMENT__;
        ::operator delete(storage);
        CHECK(remainder == 0);
    }
}

TEST_CASE("binary JSON charges long-prefix record lookup and coverage work before owned construction") {
    constexpr std::size_t COUNT = 192;
    const std::string prefix(192, 'p');
    std::string records = "[";
    std::string fields = "[";
    std::string lookups = "[";

    for (std::size_t i = 0; i < COUNT; ++i) {
        const auto suffix = std::to_string(i);
        const auto separator = i == 0 ? "" : ",";
        records += separator + std::string("{\"id\":") + suffix + ",\"name\":\"" + prefix + suffix +
                   "\",\"hasTime\":false,\"fields\":[],\"lookups\":[],\"indexedTimeLayout\":null}";
        fields += separator + std::string("{\"serialTypeId\":8}");
        lookups += separator + std::string("{\"name\":\"") + prefix + suffix + "\",\"fieldId\":" + suffix + "}";
    }

    records += ']';
    fields += ']';
    lookups += ']';
    const auto manyRecords = binaryDocument(records);
    const auto manyLookups = binaryDocument("[{\"id\":0,\"name\":\"R\",\"hasTime\":false,\"fields\":" + fields +
                                            ",\"lookups\":" + lookups + ",\"indexedTimeLayout\":null}]");

    for (const auto &input : {manyRecords, manyLookups}) {
        CHECK(loadBinaryJsonSchema(input));
        BinaryJsonSchemaLimits byteOnly;
        byteOnly.workUnits = 32 * input.size();
        binaryError(input, JsonSchemaError::RESOURCE_LIMIT, byteOnly);
    }

    binaryError(replace(binaryDocument(), "\"records\":[]", "\"records\":[],\"unknown\":[[[[[[]]]]]]"),
                JsonSchemaError::RESOURCE_LIMIT, {{1048576, 5, {1024, 16384, 32768, 1024, 1048576}}});
}

TEST_CASE("binary JSON pinned parser envelopes cover selected and fallback ordinary requests") {
    const simdjson::implementation *selected = simdjson::get_active_implementation();

    for (const auto *backend : {selected, simdjson::get_available_implementations()["fallback"]}) {
        REQUIRE(backend);
        simdjson::get_active_implementation() = backend;
        const auto before = liveRequestedBytes;
        requestedBytes = 0;
        peakRequestedBytes = before;
        bool invalid = false;
        {
            auto rejected = loadBinaryJsonSchema("null");
            invalid = !rejected && rejected.error() == JsonSchemaError::INVALID_SCHEMA;
        }

        const auto requests = requestedBytes;
        const auto peak = peakRequestedBytes - before;
        const auto after = liveRequestedBytes;
        // C=32, B=4, D=5: independent pinned-source arithmetic for variable requests.
        constexpr std::size_t VARIABLE_REQUESTS = 4 + 64 + 4 * (64 + 9) + 8 * 64 + 128 + 5 * 9;
        const auto fixed =
            sizeof(simdjson::internal::dom_parser_implementation) + 6 * sizeof(void *) + alignof(std::max_align_t);
        CHECK(invalid);
        CHECK(requests <= VARIABLE_REQUESTS + fixed);
        CHECK(peak <= requests);
        CHECK(after == before);
        std::cout << "parser-requests\t" << backend->name() << '\t' << requests << '\t' << peak << '\t'
                  << VARIABLE_REQUESTS + fixed << '\n';
    }

    simdjson::get_active_implementation() = selected;
}

TEST_CASE("binary JSON STL capacities and small-string transitions are qualified observations") {
    for (const auto length : {0, 1, 6, 7, 8, 15, 16, 31, 32}) {
        auto input = binaryRecord(std::array{104}, R"({"kind":"single-int64","fieldId":0})");
        input = replace(std::move(input), "\"name\":\"R\"", "\"name\":\"R" + std::string(length, 'x') + "\"");
        requestedBytes = 0;
        auto loaded = loadBinaryJsonSchema(input);
        const auto requests = requestedBytes;
        REQUIRE(loaded);
        auto copy = *loaded;
        const auto &record = loaded->records()[0];
        std::cout << "schema-capacity\t" << record.name.size() << '\t' << record.name.capacity() << '\t'
                  << record.fields.capacity() << '\t' << record.lookups.capacity() << '\t' << loaded->storageBytes()
                  << '\t' << copy.storageBytes() << '\t' << requests << '\n';
        CHECK(copy.recordContext(0).matches(loaded->recordContext(0)));
    }
}

} // namespace org::ttldtor::qdcxx::schema
