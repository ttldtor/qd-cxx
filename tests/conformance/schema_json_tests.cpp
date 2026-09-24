// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/schema/json_schema.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

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

} // namespace org::ttldtor::qdcxx::schema
