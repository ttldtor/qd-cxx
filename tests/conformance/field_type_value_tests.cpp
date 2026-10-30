// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/core/local_schema.hpp"
#include "qd/encoding/char_array.hpp"
#include "qd/events/quote.hpp"

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace q = org::ttldtor::qdcxx::events;

/// Immutable Java primitive observations selected by the test invocation.
std::filesystem::path fixturePath;

/** Parse a whole integral cell without accepting trailing bytes.
 * @tparam T Integral destination type.
 * @param cell Exact numeric spelling.
 * @param base Numeric radix.
 * @return Parsed number; malformed cells fail the test.
 */
template <typename T> T number(const std::string &cell, int base = 10) {
    T value{};
    const auto parsed = std::from_chars(cell.data(), cell.data() + cell.size(), value, base);
    REQUIRE(parsed.ec == std::errc{});
    REQUIRE(parsed.ptr == cell.data() + cell.size());

    if (base == 10) {
        REQUIRE(cell == std::to_string(value));
    }

    return value;
}

/** Split an observation row without dropping empty cells.
 * @param line Row excluding newline.
 * @return Exact ordered cells.
 */
std::vector<std::string> cells(const std::string &line) {
    std::vector<std::string> result;
    std::size_t start = 0;

    for (;;) {
        const auto end = line.find('\t', start);
        result.push_back(line.substr(start, end - start));

        if (end == std::string::npos) {
            return result;
        }

        start = end + 1;
    }
}

/** Decode lowercase hexadecimal octets; dash represents empty bytes.
 * @param cell Observation cell.
 * @return Owned encoded bytes.
 */
std::vector<std::byte> bytes(const std::string &cell) {
    std::vector<std::byte> result;

    if (cell == "-") {
        return result;
    }

    REQUIRE_FALSE(cell.empty());
    REQUIRE(cell.size() % 2 == 0);
    REQUIRE(cell.find_first_not_of("0123456789abcdef") == std::string::npos);

    for (std::size_t index = 0; index < cell.size(); index += 2) {
        result.push_back(static_cast<std::byte>(number<unsigned int>(cell.substr(index, 2), 16)));
    }

    return result;
}

/** Decode the Java nullable exact UTF-16 observation grammar.
 * @param cell Null, dash for empty, or four-digit hexadecimal code units.
 * @return Owned nullable units.
 */
std::optional<std::u16string> units(const std::string &cell) {
    if (cell == "null") {
        return std::nullopt;
    }

    std::u16string result;

    if (cell == "-") {
        return result;
    }

    REQUIRE_FALSE(cell.empty());
    REQUIRE(cell.size() % 4 == 0);
    REQUIRE(cell.find_first_not_of("0123456789abcdef") == std::string::npos);

    for (std::size_t index = 0; index < cell.size(); index += 4) {
        result.push_back(static_cast<char16_t>(number<unsigned int>(cell.substr(index, 4), 16)));
    }

    return result;
}

TEST_CASE("locked Java char-array and exact integer field primitive observations") {
    std::ifstream input(fixturePath, std::ios::binary);
    REQUIRE(input.is_open());
    std::string line;
    REQUIRE(static_cast<bool>(std::getline(input, line)));
    REQUIRE(line == "case\tprimitives");
    std::set<std::string> identities;
    std::size_t reads = 0;
    std::size_t writes = 0;
    std::size_t integers = 0;
    bool complete = false;

    while (std::getline(input, line)) {
        REQUIRE_FALSE(complete);
        const auto row = cells(line);
        REQUIRE_FALSE(row.empty());
        CAPTURE(line);

        if (row[0] == "complete") {
            REQUIRE(row.size() == 2);
            REQUIRE(row[1] == "primitives");
            complete = true;
        } else if (row[0] == "string-write") {
            REQUIRE(row.size() == 4);
            REQUIRE(identities.insert(row[0] + "\t" + row[1]).second);
            const auto encoded = bytes(row[3]);
            const auto result = e::readCharArray(encoded, {4096, 1024});
            REQUIRE(result.status == e::Status::OK);
            CHECK(result.units == units(row[2]));
            CHECK(result.consumed == encoded.size());
            CHECK(result.chargedUnits == (result.units ? result.units->size() : 0));
            ++writes;
        } else if (row[0] == "string-read") {
            REQUIRE(row.size() == 7);
            REQUIRE((row[2] == "buffered" || row[2] == "data-input" || row[2] == "message-part"));
            REQUIRE(identities.insert(row[0] + "\t" + row[1] + "\t" + row[2]).second);
            const auto encoded = bytes(row[3]);
            const auto boundary =
                row[2] == "message-part" ? e::CharArrayBoundary::MESSAGE : e::CharArrayBoundary::PRIMITIVE;
            const auto result = e::readCharArray(encoded, {4096, 1024}, boundary);
            CHECK(result.consumed == number<std::size_t>(row[6]));

            if (row[5] == "ok") {
                REQUIRE(result.status == e::Status::OK);
                CHECK(result.units == units(row[4]));
            } else {
                REQUIRE(row[4] == "-");
                REQUIRE((row[5] == "EOFException" || row[5] == "UTFDataFormatException" || row[5] == "IOException"));
                const auto expected = row[5] == "EOFException"  ? e::Status::INSUFFICIENT_INPUT
                                      : row[5] == "IOException" ? e::Status::INVALID_LENGTH
                                                                : e::Status::INVALID_ENCODING;
                CHECK(result.status == expected);
                CHECK_FALSE(result.units.has_value());
            }

            ++reads;
        } else if (row[0] == "int-read") {
            REQUIRE(row.size() == 7);
            const auto type = number<std::int32_t>(row[1]);
            REQUIRE((type == 40 || type == 72 || type == 88));
            REQUIRE(identities.insert(row[0] + "\t" + row[1] + "\t" + row[2]).second);
            const auto encoded = bytes(row[3]);
            const auto result = e::readCompactInt(encoded);
            CHECK(result.consumed == number<std::size_t>(row[6]));

            if (row[5] == "ok") {
                REQUIRE(result.status == e::Status::OK);
                CHECK(result.value == number<std::int32_t>(row[4]));
                CHECK(c::validFieldValue({static_cast<c::SerialFieldType>(type), result.value}));
            } else {
                REQUIRE(row[5] == "EOFException");
                REQUIRE(row[4] == "-");
                CHECK(result.status == e::Status::INSUFFICIENT_INPUT);
            }

            ++integers;
        } else {
            FAIL("Unknown primitive observation row");
        }
    }

    CHECK(input.eof());
    CHECK(complete);
    CHECK(writes == 9);
    CHECK(reads == 219);
    CHECK(integers == 324);
}

TEST_CASE("full field identities expose serialization and representation without losing unknown bits") {
    using T = c::SerialFieldType;
    using S = c::SerialType;
    using R = c::FieldRepresentation;
    // Independent protocol literals must not be derived from the implementation constants.
    constexpr std::array<T, 8> TYPES{T::UTF_CHAR,     T::COMPACT_INT, T::UTF_CHAR_ARRAY, T::SHORT_STRING,
                                     T::TIME_SECONDS, T::SEQUENCE,    T::DATE,           T::WIDE_DECIMAL};
    constexpr std::array<std::int32_t, 8> IDS{2, 8, 10, 40, 56, 72, 88, 120};
    constexpr std::array<std::int32_t, 8> SERIAL{2, 8, 10, 8, 8, 8, 8, 8};
    constexpr std::array<std::int32_t, 8> REPRESENTATIONS{0, 0, 0, 0x20, 0x30, 0x40, 0x50, 0x70};
    static_assert(c::composeSerialTypeId(S::COMPACT_INT, R::WIDE_DECIMAL) == 120);
    static_assert(c::serialTypeBits(0x178) == 0x108);
    static_assert(c::representationBits(0x178) == 0x70);

    CHECK(c::REPRESENTATION_MASK == 0xf0);
    CHECK(c::SERIAL_TYPE_MASK == ~std::int32_t{0xf0});

    // Pin selectors directly: an extra base bit can disappear when composing the full identity.
    CHECK(static_cast<std::int32_t>(R::PLAIN) == 0x00);
    CHECK(static_cast<std::int32_t>(R::SHORT_STRING) == 0x20);
    CHECK(static_cast<std::int32_t>(R::TIME_SECONDS) == 0x30);
    CHECK(static_cast<std::int32_t>(R::SEQUENCE) == 0x40);
    CHECK(static_cast<std::int32_t>(R::DATE) == 0x50);
    CHECK(static_cast<std::int32_t>(R::WIDE_DECIMAL) == 0x70);

    for (std::size_t index = 0; index < TYPES.size(); ++index) {
        CAPTURE(index);
        CHECK(static_cast<std::int32_t>(TYPES[index]) == IDS[index]);
        CHECK(c::serialTypeBits(IDS[index]) == SERIAL[index]);
        CHECK(c::representationBits(IDS[index]) == REPRESENTATIONS[index]);
    }

    for (std::int32_t id = 0; id <= 0xff; ++id) {
        CAPTURE(id);
        CHECK(c::serialTypeBits(id) == id % 16);
        CHECK(c::representationBits(id) == (id / 16) * 16);
        CHECK((c::serialTypeBits(id) | c::representationBits(id)) == id);
    }

    // The serialization mask is NOT 0x0f: unknown high bits must survive decomposition.
    for (const auto id :
         {0x108, 0x178, -1, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
        CAPTURE(id);
        CHECK(c::serialTypeBits(id) == (id & ~std::int32_t{0xf0}));
        CHECK((c::serialTypeBits(id) | c::representationBits(id)) == id);

        for (const auto profile :
             {c::LocalProfile::COMPACT_INT_V1, c::LocalProfile::QUOTE_FIELDS_V1, c::LocalProfile::EXTENDED_FIELDS_V1}) {
            CHECK_FALSE(c::supportsSerialType(id, profile));
        }
    }

    // Bit composition is not a promise that a type/representation pair is supported.
    const auto unsupported = c::composeSerialTypeId(S::UTF_CHAR, R::WIDE_DECIMAL);
    CHECK(unsupported == 114);
    CHECK_FALSE(c::supportsSerialType(unsupported, c::LocalProfile::EXTENDED_FIELDS_V1));
}

TEST_CASE("extended identities preserve both prior profile gates and reject serialization siblings") {
    constexpr std::array<std::int32_t, 8> EXTENDED{2, 8, 10, 40, 56, 72, 88, 120};
    constexpr std::array<std::int32_t, 4> QUOTE{2, 8, 56, 120};

    for (std::int32_t type = -1; type <= 256; ++type) {
        CAPTURE(type);
        const bool extended = std::find(EXTENDED.begin(), EXTENDED.end(), type) != EXTENDED.end();
        const bool quote = std::find(QUOTE.begin(), QUOTE.end(), type) != QUOTE.end();
        CHECK(c::supportsSerialType(type, c::LocalProfile::EXTENDED_FIELDS_V1) == extended);
        CHECK(c::supportsSerialType(type, c::LocalProfile::QUOTE_FIELDS_V1) == quote);
        CHECK(c::supportsSerialType(type, c::LocalProfile::COMPACT_INT_V1) == (type == 8));
        CHECK_FALSE(c::supportsSerialType(type, static_cast<c::LocalProfile>(-1)));
        const c::LocalRecord record{0, u"Fields", false, {{type}}, {{u"Value", 0}}};
        const auto schema =
            c::LocalSchema::create(std::span(&record, 1), {1, 1, 1, 6, 11}, c::LocalProfile::EXTENDED_FIELDS_V1);
        CHECK(schema.has_value() == extended);
    }
}

TEST_CASE("owned FieldValue equality and canonical representations reject malformed manual values") {
    using T = c::SerialFieldType;
    const c::FieldValue nullValue{T::UTF_CHAR_ARRAY, 0};
    const c::FieldValue emptyValue{T::UTF_CHAR_ARRAY, 0, std::u16string{}};
    const c::FieldValue content{T::UTF_CHAR_ARRAY, 0, std::u16string{0, 0xd800, 0xdc00, 0xffff}};
    CHECK(nullValue != emptyValue);
    CHECK(emptyValue != content);
    CHECK(c::validFieldValue(nullValue));
    CHECK(c::validFieldValue(emptyValue));
    CHECK(c::validFieldValue(content));
    CHECK_FALSE(c::validFieldValue({T::UTF_CHAR_ARRAY, 1}));

    for (const auto type : {T::COMPACT_INT, T::TIME_SECONDS, T::SHORT_STRING, T::SEQUENCE, T::DATE}) {
        for (const auto raw : {std::numeric_limits<std::int32_t>::min(), 0, std::numeric_limits<std::int32_t>::max()}) {
            CHECK(c::validFieldValue({type, raw}));
            CHECK_FALSE(c::validFieldValue({type, raw, u""}));
        }

        CHECK_FALSE(c::validFieldValue({type, std::int64_t{1} << 31}));
        CHECK_FALSE(c::validFieldValue({type, -1 - (std::int64_t{1} << 31)}));
    }

    CHECK(c::validFieldValue({T::UTF_CHAR, 0x10ffff}));
    CHECK_FALSE(c::validFieldValue({T::UTF_CHAR, -1}));
    CHECK_FALSE(c::validFieldValue({T::UTF_CHAR, 0x110000}));
    CHECK_FALSE(c::validFieldValue({T::UTF_CHAR, 0, u""}));
    CHECK(c::validFieldValue({T::WIDE_DECIMAL, std::numeric_limits<std::int64_t>::min()}));
    CHECK(c::validFieldValue({T::WIDE_DECIMAL, std::numeric_limits<std::int64_t>::max()}));
    CHECK_FALSE(c::validFieldValue({T::WIDE_DECIMAL, 0, u""}));
    CHECK_FALSE(c::validFieldValue({static_cast<T>(25), 0}));
}

TEST_CASE("binary scalar profile preserves exact old policies and canonical signed raw ranges") {
    using T = c::SerialFieldType;
    constexpr std::array<std::int32_t, 15> BINARY{0, 1, 2, 3, 4, 8, 10, 24, 40, 56, 72, 88, 104, 120, 152};
    constexpr std::array<T, 7> NEW_TYPES{T::VOID_FIELD, T::BYTE, T::SHORT, T::INT, T::DECIMAL, T::LONG, T::TIME_MILLIS};
    constexpr std::array<std::int32_t, 7> NEW_IDS{0, 1, 3, 4, 24, 104, 152};

    for (std::size_t index = 0; index < NEW_TYPES.size(); ++index) {
        CHECK(static_cast<std::int32_t>(NEW_TYPES[index]) == NEW_IDS[index]);

        for (const auto profile :
             {c::LocalProfile::COMPACT_INT_V1, c::LocalProfile::QUOTE_FIELDS_V1, c::LocalProfile::EXTENDED_FIELDS_V1}) {
            CHECK_FALSE(c::supportsSerialType(NEW_IDS[index], profile));
        }
    }

    for (std::int32_t type = -1; type <= 512; ++type) {
        CAPTURE(type);
        const bool accepted = std::find(BINARY.begin(), BINARY.end(), type) != BINARY.end();
        CHECK(c::supportsSerialType(type, c::LocalProfile::BINARY_FIELDS_V1) == accepted);
        const c::LocalRecord record{0, u"R", false, {{type}}, {{u"Value", 0}}};
        const auto local =
            c::LocalSchema::create(std::span(&record, 1), {1, 1, 1, 5, 6}, c::LocalProfile::BINARY_FIELDS_V1);
        CHECK(local.has_value() == accepted);
    }

    CHECK(c::validFieldValue({T::VOID_FIELD, 0}));
    CHECK_FALSE(c::validFieldValue({T::VOID_FIELD, 1}));
    CHECK_FALSE(c::validFieldValue({T::VOID_FIELD, -1}));

    for (const auto &[type, maximum] : std::array<std::pair<T, std::int64_t>, 4>{
             {{T::BYTE, 127}, {T::SHORT, 32767}, {T::INT, 2147483647}, {T::DECIMAL, 2147483647}}}) {
        CHECK(c::validFieldValue({type, -maximum - 1}));
        CHECK(c::validFieldValue({type, maximum}));
        CHECK_FALSE(c::validFieldValue({type, -maximum - 2}));
        CHECK_FALSE(c::validFieldValue({type, maximum + 1}));
    }

    for (const auto type : {T::LONG, T::TIME_MILLIS}) {
        CHECK(c::validFieldValue({type, std::numeric_limits<std::int64_t>::min()}));
        CHECK(c::validFieldValue({type, std::numeric_limits<std::int64_t>::max()}));
    }

    for (const auto type : NEW_TYPES) {
        CHECK_FALSE(c::validFieldValue({type, 0, u""}));
    }

    CHECK(static_cast<std::int32_t>(c::FieldRepresentation::DECIMAL) == 0x10);
    CHECK(static_cast<std::int32_t>(c::FieldRepresentation::LONG) == 0x60);
    CHECK(static_cast<std::int32_t>(c::FieldRepresentation::TIME_MILLIS) == 0x90);
}

TEST_CASE("char-array finite bounds and primitive versus message precedence") {
    const auto encoded = bytes("0241c2807f");
    const auto exact = e::readCharArray(encoded, {4, 2});
    REQUIRE(exact.status == e::Status::OK);
    CHECK(exact.units == std::optional<std::u16string>{u"A\u0080"});
    CHECK(exact.consumed == 4);
    CHECK(exact.chargedUnits == 2);
    const auto bytesShort = e::readCharArray(encoded, {3, 2});
    CHECK(bytesShort.status == e::Status::RESOURCE_LIMIT);
    CHECK(bytesShort.consumed == 3);
    CHECK(bytesShort.chargedUnits == 2);
    CHECK_FALSE(bytesShort.units);
    const auto unitsShort = e::readCharArray(encoded, {4, 1});
    CHECK(unitsShort.status == e::Status::RESOURCE_LIMIT);
    CHECK(unitsShort.consumed == 1);
    CHECK(unitsShort.chargedUnits == 0);

    for (const auto &empty : {bytes("00"), bytes("7f")}) {
        CHECK(e::readCharArray(empty, {0, 0}).status == e::Status::RESOURCE_LIMIT);
        CHECK(e::readCharArray(empty, {1, 0}).status == e::Status::OK);
    }

    const auto prefix = bytes("0341");
    const auto primitive = e::readCharArray(prefix, {8, 3});
    CHECK(primitive.status == e::Status::INSUFFICIENT_INPUT);
    CHECK(primitive.consumed == 2);
    CHECK(primitive.chargedUnits == 3);
    const auto message = e::readCharArray(prefix, {8, 0}, e::CharArrayBoundary::MESSAGE);
    CHECK(message.status == e::Status::INVALID_LENGTH);
    CHECK(message.consumed == 1);
    CHECK(message.chargedUnits == 0);
    const auto clipped = e::readCharArray(bytes("024142"), {1, 2}, e::CharArrayBoundary::MESSAGE);
    CHECK(clipped.status == e::Status::RESOURCE_LIMIT);
    CHECK(clipped.consumed == 1);
    CHECK(clipped.chargedUnits == 2);
    const auto oversized = e::readCharArray(bytes("f07fffffff"), {9, 2});
    CHECK(oversized.status == e::Status::RESOURCE_LIMIT);
    CHECK(oversized.consumed == 5);
    CHECK(oversized.chargedUnits == 0);
    const auto invalid = e::readCharArray(bytes("7e"), {1, 0});
    CHECK(invalid.status == e::Status::INVALID_LENGTH);
    CHECK(invalid.chargedUnits == 0);
    const auto longPrefix = bytes("800141");
    const auto truncatedPrefix = e::readCharArray(longPrefix, {1, 1});
    CHECK(truncatedPrefix.status == e::Status::RESOURCE_LIMIT);
    CHECK(truncatedPrefix.consumed == 1);
    CHECK(truncatedPrefix.chargedUnits == 0);
    const auto actualPrefix = e::readCharArray(std::span(longPrefix).first(1), {1, 1});
    CHECK(actualPrefix.status == e::Status::INSUFFICIENT_INPUT);
    CHECK(actualPrefix.consumed == 1);
    CHECK(actualPrefix.chargedUnits == 0);
}

TEST_CASE("char-array owns exact units and charges malformed suffix without exposing partial value") {
    auto encoded = bytes("044100eda080edb0807f");
    auto decoded = e::readCharArray(encoded, {10, 4});
    REQUIRE(decoded.status == e::Status::OK);
    CHECK(decoded.consumed == 9);
    std::fill(encoded.begin(), encoded.end(), std::byte{0});
    encoded.clear();
    REQUIRE(decoded.units);
    CHECK(*decoded.units == std::u16string{u'A', 0, 0xd800, 0xdc00});

    for (const auto &bad : {bytes("0241e00080"), bytes("0241f0908080"), bytes("0241f00080")}) {
        const auto result = e::readCharArray(bad, {8, 2});
        CHECK(result.status != e::Status::OK);
        CHECK(result.consumed == bad.size());
        CHECK(result.chargedUnits == 2);
        CHECK_FALSE(result.units);
    }
}

TEST_CASE("Quote rejects each new tag and every present scalar payload in every slot") {
    std::vector<c::FieldValue> baseline;

    for (const auto type : q::QUOTE_FIELD_TYPES) {
        baseline.push_back({type, 0});
    }

    REQUIRE(q::mapQuote(u"", baseline, 0));

    for (std::size_t index = 0; index < baseline.size(); ++index) {
        CAPTURE(index);

        for (const auto type : {c::SerialFieldType::UTF_CHAR_ARRAY, c::SerialFieldType::SHORT_STRING,
                                c::SerialFieldType::SEQUENCE, c::SerialFieldType::DATE}) {
            auto fields = baseline;
            fields[index].type = type;
            const auto result = q::mapQuote(u"", fields, 0);
            REQUIRE_FALSE(result);
            CHECK(result.error() == q::QuoteError::INVALID_SHAPE);
        }

        for (const auto &payload : {std::u16string{}, std::u16string{u'x'}}) {
            auto fields = baseline;
            fields[index].units = payload;
            const auto result = q::mapQuote(u"", fields, 0);
            REQUIRE_FALSE(result);
            CHECK(result.error() == q::QuoteError::INVALID_SHAPE);
        }
    }
}
} // namespace

/** Run immutable primitive observations and focused owned-value checks.
 * @param argc Argument count including fixture path and doctest options.
 * @param argv Borrowed arguments; first argument is the immutable primitive TSV.
 * @return Zero on success, nonzero on usage or failed checks.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: qdcxx_field_type_value_tests <primitives.tsv> [doctest options]\n";

        return 1;
    }

    fixturePath = argv[1];
    doctest::Context context(argc - 1, argv + 1);
    context.setOption("no-colors", true);
    context.setOption("no-breaks", true);

    return context.run();
}
