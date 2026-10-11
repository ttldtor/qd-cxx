// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "qd/encoding/char_array.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace encoding = org::ttldtor::qdcxx::encoding;
std::filesystem::path fixtures;     ///< Contract fixture directory selected by the test caller.
constexpr std::byte SENTINEL{0x5a}; ///< Detect partial writes and changed excess capacity.

/** Convert fixture hex to a bounded integer.
 * @param text Borrowed fixture hexadecimal text.
 * @return Parsed value; malformed fixture input fails the current test.
 */
unsigned int parseHex(std::string_view text) {
    unsigned int value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    REQUIRE(result.ec == std::errc{});
    REQUIRE(result.ptr == text.data() + text.size());

    return value;
}

/** Read exact nullable UTF-16 fixture units without Unicode normalization.
 * @param text Null, empty or hexadecimal UTF-16 units from the contract fixture.
 * @return Owned nullable units.
 */
std::optional<std::u16string> parseUnits(std::string_view text) {
    if (text == "null") {
        return std::nullopt;
    }

    std::u16string result;

    if (text != "empty") {
        REQUIRE(text.size() % 4 == 0);

        for (std::size_t index = 0; index < text.size(); index += 4) {
            result.push_back(static_cast<char16_t>(parseHex(text.substr(index, 4))));
        }
    }

    return result;
}

/** Decode reviewed fixture bytes; this helper is not a primitive encoder.
 * @param text Hexadecimal expected bytes.
 * @return Exact expected octets.
 */
std::vector<std::byte> parseBytes(std::string_view text) {
    REQUIRE(text.size() % 2 == 0);
    std::vector<std::byte> result;

    for (std::size_t index = 0; index < text.size(); index += 2) {
        result.push_back(static_cast<std::byte>(parseHex(text.substr(index, 2))));
    }

    return result;
}

TEST_CASE("char-array writer consumes reviewed primitive contract rows") {
    std::ifstream input(fixtures / "primitives.tsv");
    REQUIRE(input.good());
    std::string line;
    std::getline(input, line);
    std::size_t rows = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream stream(line);
        std::vector<std::string> columns;
        std::string column;

        while (std::getline(stream, column, '\t')) {
            columns.push_back(column);
        }

        REQUIRE(columns.size() == 8);

        if (columns[1] != "char-array") {
            continue;
        }

        CAPTURE(columns[0]);
        ++rows;
        const auto units = parseUnits(columns[2]);
        const auto capacity = static_cast<std::size_t>(std::stoull(columns[3]));
        const encoding::CharArrayLimits limits{static_cast<std::size_t>(std::stoull(columns[4])),
                                               static_cast<std::size_t>(std::stoull(columns[5]))};
        std::vector<std::byte> output(capacity + 3, SENTINEL);
        const auto original = output;
        const std::optional<std::u16string_view> value =
            units ? std::optional<std::u16string_view>(*units) : std::nullopt;
        const auto result = encoding::writeCharArray(value, std::span(output).first(capacity), limits);

        if (columns[6] == "OK") {
            const auto expected = parseBytes(columns[7]);
            CHECK(result.status == encoding::Status::OK);
            CHECK(result.written == expected.size());
            CHECK(std::equal(expected.begin(), expected.end(), output.begin()));
            CHECK(std::all_of(output.begin() + static_cast<std::ptrdiff_t>(result.written), output.end(),
                              [](std::byte byte) {
                                  return byte == SENTINEL;
                              }));

            const auto decoded = encoding::readCharArray(std::span(output).first(result.written), limits);
            CHECK(decoded.status == encoding::Status::OK);
            CHECK(decoded.units == units);
        } else {
            const auto expected = columns[6] == "RESOURCE_LIMIT" ? encoding::Status::RESOURCE_LIMIT
                                                                 : encoding::Status::INSUFFICIENT_OUTPUT;
            CHECK(result.status == expected);
            CHECK(result.written == 0);
            CHECK(output == original);
        }
    }

    CHECK(rows == 9);
}

TEST_CASE("char-array writer preflights whole output and exact limits") {
    const auto maximum = std::numeric_limits<std::size_t>::max();
    const std::u16string units{u'\0',
                               static_cast<char16_t>(0x7f),
                               static_cast<char16_t>(0x80),
                               static_cast<char16_t>(0x7ff),
                               static_cast<char16_t>(0x800),
                               static_cast<char16_t>(0xd800),
                               static_cast<char16_t>(0xdc00),
                               static_cast<char16_t>(0xffff)};
    const auto expected = parseBytes("08007fc280dfbfe0a080eda080edb080efbfbf");
    std::vector<std::byte> output(expected.size() + 4, SENTINEL);
    const auto original = output;

    for (std::size_t capacity = 0; capacity < expected.size(); ++capacity) {
        const auto result = encoding::writeCharArray(units, std::span(output).first(capacity), {maximum, maximum});
        CHECK(result.status == encoding::Status::INSUFFICIENT_OUTPUT);
        CHECK(result.written == 0);
        CHECK(output == original);
    }

    for (const auto limits :
         {encoding::CharArrayLimits{expected.size() - 1, units.size()},
          encoding::CharArrayLimits{expected.size(), units.size() - 1}, encoding::CharArrayLimits{0, 0}}) {
        const auto result = encoding::writeCharArray(units, std::span(output).first(0), limits);
        CHECK(result.status == encoding::Status::RESOURCE_LIMIT);
        CHECK(result.written == 0);
        CHECK(output == original);
    }

    const auto result = encoding::writeCharArray(units, output, {expected.size(), units.size()});
    REQUIRE(result.status == encoding::Status::OK);
    CHECK(result.written == expected.size());
    CHECK(std::equal(expected.begin(), expected.end(), output.begin()));
    CHECK(std::all_of(output.begin() + static_cast<std::ptrdiff_t>(result.written), output.end(), [](std::byte byte) {
        return byte == SENTINEL;
    }));

    for (const auto value : {std::optional<std::u16string_view>{}, std::optional<std::u16string_view>{u""}}) {
        output = original;
        const auto rejected = encoding::writeCharArray(value, output, {0, 0});
        CHECK(rejected.status == encoding::Status::RESOURCE_LIMIT);
        CHECK(rejected.written == 0);
        CHECK(output == original);
        CHECK(encoding::writeCharArray(value, output, {1, 0}).status == encoding::Status::OK);
        CHECK(output[0] == (value ? std::byte{0} : std::byte{0x7f}));
    }
}

TEST_CASE("char-array writer canonical count transitions remain unit counts") {
    const std::array<std::pair<std::size_t, std::string_view>, 6> transitions{
        {{63, "3f"}, {64, "8040"}, {8191, "9fff"}, {8192, "c02000"}, {1048575, "cfffff"}, {1048576, "e0100000"}}};

    for (const auto &[count, prefixHex] : transitions) {
        CAPTURE(count);
        const std::u16string units(count, u'A');
        std::vector<std::byte> output(count + 5, SENTINEL);
        const auto result = encoding::writeCharArray(units, output, {output.size(), count});
        REQUIRE(result.status == encoding::Status::OK);
        const auto expectedPrefix = parseBytes(prefixHex);
        CHECK(std::equal(expectedPrefix.begin(), expectedPrefix.end(), output.begin()));
        const auto prefix = encoding::readCompactInt(output);
        REQUIRE(prefix.status == encoding::Status::OK);
        CHECK(prefix.value == static_cast<std::int32_t>(count));
        CHECK(result.written == prefix.consumed + count);
        CHECK(std::all_of(output.begin() + static_cast<std::ptrdiff_t>(prefix.consumed),
                          output.begin() + static_cast<std::ptrdiff_t>(result.written), [](std::byte byte) {
                              return byte == std::byte{0x41};
                          }));
    }
}

TEST_CASE("char-array writer preserves every UTF-16 unit independently") {
    constexpr std::size_t BMP_UNITS = 0x10000; ///< Entire exact UTF-16 unit domain.
    std::u16string units;
    units.reserve(BMP_UNITS);

    for (std::size_t unit = 0; unit < BMP_UNITS; ++unit) {
        units.push_back(static_cast<char16_t>(unit));
    }

    std::vector<std::byte> output(BMP_UNITS * 3 + 5, SENTINEL);
    const encoding::CharArrayLimits limits{output.size(), units.size()};
    const auto written = encoding::writeCharArray(units, output, limits);
    REQUIRE(written.status == encoding::Status::OK);
    const auto read = encoding::readCharArray(std::span(output).first(written.written), limits);
    REQUIRE(read.status == encoding::Status::OK);
    CHECK(read.units == std::optional<std::u16string>(units));
    CHECK(read.consumed == written.written);
}
} // namespace

/** Run primitive fixture and atomic writer checks.
 * @param argc Argument count including the fixture directory and optional doctest options.
 * @param argv Borrowed arguments; argv[1] names the contract fixture directory.
 * @return Zero on passing tests; nonzero on missing arguments or conformance failures.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: char_array_writer_tests <contract fixture directory> [doctest options]\n";

        return 1;
    }

    fixtures = argv[1];
    doctest::Context context(argc - 1, argv + 1);
    context.setOption("no-colors", true);
    context.setOption("no-breaks", true);

    return context.run();
}
