// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/core/wide_decimal.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
using org::ttldtor::qdcxx::core::DecimalKind;
using org::ttldtor::qdcxx::core::WideDecimal;
using org::ttldtor::qdcxx::encoding::readCompactInt;
using org::ttldtor::qdcxx::encoding::readCompactLong;
using org::ttldtor::qdcxx::encoding::readUtfChar;
using org::ttldtor::qdcxx::encoding::Status;
using org::ttldtor::qdcxx::encoding::writeCompactLong;
using org::ttldtor::qdcxx::encoding::writeUtfChar;

/// Caller-selected immutable Java observation file.
std::filesystem::path fixturePath;

/** Parse an entire numeric cell without locale or trailing text.
 * @tparam T Integral destination type.
 * @param cell Exact ASCII cell.
 * @param base Radix used by the observation grammar.
 * @return Parsed value; malformed cells fail the test.
 */
template <typename T> T number(const std::string &cell, int base = 10) {
    T value{};
    const auto result = std::from_chars(cell.data(), cell.data() + cell.size(), value, base);
    REQUIRE(result.ec == std::errc{});
    REQUIRE(result.ptr == cell.data() + cell.size());

    return value;
}

/** Split a row preserving empty cells.
 * @param line Row without newline.
 * @return Owned cells in order.
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

/** Decode a strict lowercase octet cell, with dash denoting empty bytes.
 * @param cell Hexadecimal cell or dash.
 * @return Owned exact bytes.
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

/** Check an exact decimal observation, classifying NaN and comparing every other binary64 bit.
 * @param raw Preserved compact-long value.
 * @param text Observed Java decimal rendering.
 * @param bits Observed canonical Java binary64 bits.
 */
void checkDecimal(std::int64_t raw, const std::string &text, const std::string &bits) {
    const WideDecimal value{raw};
    CHECK(value.raw == raw);
    CHECK(value.toString() == text);
    CHECK(value.toString().size() <= 32);
    REQUIRE(bits.size() == 16);
    REQUIRE(bits.find_first_not_of("0123456789abcdef") == std::string::npos);
    const auto expected = number<std::uint64_t>(bits, 16);

    if (text == "NaN") {
        CHECK(value.kind() == DecimalKind::NAN_VALUE);
        CHECK(std::isnan(value.toDouble()));
        CHECK(expected == 0x7ff8000000000000ULL);
    } else {
        CHECK(std::bit_cast<std::uint64_t>(value.toDouble()) == expected);
        CHECK(value.kind() == (text == "Infinity"    ? DecimalKind::POSITIVE_INFINITY
                               : text == "-Infinity" ? DecimalKind::NEGATIVE_INFINITY
                                                     : DecimalKind::FINITE));
    }

    CHECK(value.significand() == (raw >> 8));
    CHECK(value.exponent() == 128 - static_cast<int>(static_cast<std::uint64_t>(raw) & 255U));
}

TEST_CASE("Every finite Java primitive row retains its exact value and consumption") {
    std::ifstream input(fixturePath);
    REQUIRE(input.is_open());
    std::array<std::size_t, 5> counts{};
    std::size_t row = 0;
    bool complete = false;
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        CAPTURE(row);
        CAPTURE(line);
        REQUIRE_FALSE(complete);
        const auto field = cells(line);
        REQUIRE_FALSE(field.empty());

        if (row++ == 0) {
            REQUIRE(field == std::vector<std::string>{"case", "primitives"});
        } else if (field[0] == "char-write") {
            REQUIRE(field.size() == 5);
            ++counts[0];
            const auto point = number<std::int32_t>(field[1]);
            const auto expected = bytes(field[2]);
            std::array<std::byte, 5> output{};
            output.fill(std::byte{0x5a});
            const auto result = writeUtfChar(point, output);
            REQUIRE((field[3] == "ok" || field[3] == "UTFDataFormatException"));
            CHECK(result.status == (field[3] == "ok" ? Status::OK : Status::INVALID_ENCODING));
            CHECK(result.written == number<std::size_t>(field[4]));
            REQUIRE(result.written == expected.size());
            CHECK(std::equal(expected.begin(), expected.end(), output.begin()));

            for (auto index = result.written; index < output.size(); ++index) {
                CHECK(output[index] == std::byte{0x5a});
            }

            for (std::size_t capacity = 0; capacity < expected.size(); ++capacity) {
                output.fill(std::byte{0x5a});
                const auto shortResult = writeUtfChar(point, std::span(output).first(capacity));
                CHECK(shortResult.status == Status::INSUFFICIENT_OUTPUT);
                CHECK(shortResult.written == 0);
                CHECK(output == std::array<std::byte, 5>{std::byte{0x5a}, std::byte{0x5a}, std::byte{0x5a},
                                                         std::byte{0x5a}, std::byte{0x5a}});
            }
        } else if (field[0] == "char-read") {
            REQUIRE(field.size() == 6);
            REQUIRE(number<std::size_t>(field[1]) == counts[1]++);
            auto encoded = bytes(field[2]);
            const auto result = readUtfChar(encoded);
            REQUIRE((field[4] == "ok" || field[4] == "EOFException" || field[4] == "UTFDataFormatException"));
            REQUIRE((field[3] == "-") == (field[4] != "ok"));
            CHECK(result.status == (field[4] == "ok"             ? Status::OK
                                    : field[4] == "EOFException" ? Status::INSUFFICIENT_INPUT
                                                                 : Status::INVALID_ENCODING));
            CHECK(result.consumed == number<std::size_t>(field[5]));
            CHECK(result.value == (field[3] == "-" ? 0 : number<std::int32_t>(field[3])));

            if (result.status == Status::OK) {
                encoded.push_back(std::byte{0x7f});
                const auto trailing = readUtfChar(encoded);
                CHECK(trailing.value == result.value);
                CHECK(trailing.consumed == result.consumed);
            }
        } else if (field[0] == "time") {
            REQUIRE(field.size() == 4);
            ++counts[2];
            const auto result = readCompactInt(bytes(field[2]));
            REQUIRE(result.status == Status::OK);
            CHECK(result.value == number<std::int32_t>(field[1]));
            CHECK(static_cast<std::int64_t>(result.value) * 1000 == number<std::int64_t>(field[3]));
        } else if (field[0] == "decimal-raw") {
            REQUIRE(field.size() == 6);
            REQUIRE(number<std::size_t>(field[1]) == counts[3]++);
            const auto encoded = bytes(field[3]);
            const auto result = readCompactLong(encoded);
            REQUIRE(result.status == Status::OK);
            CHECK(result.consumed == encoded.size());
            const auto raw = number<std::int64_t>(field[2]);
            CHECK(result.value == raw);
            std::array<std::byte, 9> output{};
            const auto written = writeCompactLong(raw, output);
            REQUIRE(written.status == Status::OK);
            CHECK(written.written == encoded.size());
            CHECK(std::equal(encoded.begin(), encoded.end(), output.begin()));
            checkDecimal(raw, field[4], field[5]);
        } else if (field[0] == "decimal-compose") {
            REQUIRE(field.size() == 6);
            REQUIRE(number<std::size_t>(field[1]) == counts[4]++);
            REQUIRE(field[2].size() == 16);
            static_cast<void>(number<std::uint64_t>(field[2], 16));
            checkDecimal(number<std::int64_t>(field[3]), field[4], field[5]);
        } else {
            REQUIRE(field == std::vector<std::string>{"complete", "primitives"});
            complete = true;
        }
    }

    CHECK(input.eof());
    CHECK(complete);
    CHECK(counts == std::array<std::size_t, 5>{14, 27, 5, 19, 13});
}

TEST_CASE("UTF_CHAR rejects invalid points before checking capacity and reads complete tails first") {
    for (const auto invalid : {-1, 0x110000, std::numeric_limits<std::int32_t>::max()}) {
        CHECK(writeUtfChar(invalid, {}).status == Status::INVALID_ENCODING);
    }

    for (const auto &encoded : {bytes("e000"), bytes("f00080")}) {
        const auto result = readUtfChar(encoded);
        CHECK(result.status == Status::INSUFFICIENT_INPUT);
        CHECK(result.consumed == encoded.size());
        CHECK(result.value == 0);
    }
}

TEST_CASE("Decimal rank zero classification and all finite ranks retain bounded presentation") {
    for (int rank = 1; rank <= 255; ++rank) {
        const WideDecimal zero{rank};
        CHECK(zero.kind() == DecimalKind::FINITE);
        CHECK(zero.toString() == "0");
        CHECK(std::bit_cast<std::uint64_t>(zero.toDouble()) == 0);

        for (const auto significand :
             {std::int64_t{1}, std::int64_t{-1}, std::int64_t{36028797018963967}, std::int64_t{-36028797018963968}}) {
            const auto raw = std::bit_cast<std::int64_t>((static_cast<std::uint64_t>(significand) << 8) |
                                                         static_cast<unsigned int>(rank));
            const WideDecimal value{raw};
            CHECK(value.kind() == DecimalKind::FINITE);
            CHECK(value.significand() == significand);
            CHECK(value.exponent() == 128 - rank);
            CHECK(value.toString().size() <= 32);
            CHECK(std::isfinite(value.toDouble()));
        }
    }
}
} // namespace

/** Run exact primitive observations and synthetic bounded checks.
 * @param argc Argument count including fixture path and optional doctest options.
 * @param argv Borrowed command arguments; argv[1] is the primitives TSV path.
 * @return Zero on selected test success, nonzero on usage or assertion failure.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: qdcxx_quote_value_tests <primitives.tsv> [doctest options]\n";

        return 1;
    }

    fixturePath = argv[1];
    doctest::Context context(argc - 1, argv + 1);
    context.setOption("no-colors", true);
    context.setOption("no-breaks", true);

    return context.run();
}
