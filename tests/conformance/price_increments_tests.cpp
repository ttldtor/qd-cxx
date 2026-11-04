// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/glossary/price_increments.hpp"
#include <doctest/doctest.h>

#include <bit>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <sstream>
#include <type_traits>
#include <utility>

namespace {
thread_local std::ptrdiff_t failAllocationAfter = -1;
}

void *operator new(std::size_t size) {
    if (failAllocationAfter == 0) {
        throw std::bad_alloc();
    }

    if (failAllocationAfter > 0) {
        --failAllocationAfter;
    }

    if (auto memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }

    throw std::bad_alloc();
}

void *operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void *memory) noexcept {
    std::free(memory);
}

void operator delete[](void *memory) noexcept {
    std::free(memory);
}

void operator delete(void *memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void *memory, std::size_t) noexcept {
    std::free(memory);
}

namespace {
using namespace org::ttldtor::qdcxx::glossary;

/** @param text Borrowed fixture cells.
 * @param delimiter Cell delimiter.
 * @return Independent cells, preserving empty cells.
 */
std::vector<std::string> split(std::string_view text, char delimiter) {
    std::vector<std::string> result;
    std::size_t start = 0;

    for (;;) {
        auto end = text.find(delimiter, start);
        result.emplace_back(text.substr(start, end == text.npos ? end : end - start));

        if (end == text.npos) {
            return result;
        }

        start = end + 1;
    }
}

/** @param text Hexadecimal UTF-16 units or explicit null/empty marker.
 * @return Decoded owned optional text.
 */
std::optional<std::u16string> unhex(std::string_view text) {
    if (text == "~") {
        return std::nullopt;
    }

    std::u16string result;

    if (text != "-") {
        for (std::size_t i = 0; i < text.size(); i += 4) {
            result += static_cast<char16_t>(std::stoul(std::string(text.substr(i, 4)), nullptr, 16));
        }
    }

    return result;
}

/** @param text Optional owned fixture text, retained by the caller.
 * @return Borrowed optional view for one synchronous call.
 */
std::optional<std::u16string_view> view(const std::optional<std::u16string> &text) {
    return text ? std::optional<std::u16string_view>(*text) : std::nullopt;
}

/** @param text Exact UTF-16 text.
 * @return Hexadecimal encoding, with an explicit empty marker.
 */
std::string hex(std::u16string_view text) {
    if (text.empty()) {
        return "-";
    }

    constexpr std::string_view DIGITS = "0123456789abcdef";
    std::string result;

    for (auto ch : text) {
        for (int shift = 12; shift >= 0; shift -= 4) {
            result += DIGITS[(ch >> shift) & 15];
        }
    }

    return result;
}

/** @param value Any binary64 value.
 * @return Java doubleToLongBits hexadecimal representation, canonicalizing NaN.
 */
std::string bits(double value) {
    std::ostringstream out;
    out << std::hex << (std::isnan(value) ? std::uint64_t{0x7ff8000000000000} : std::bit_cast<std::uint64_t>(value));

    return out.str();
}

/** @param text Hexadecimal raw bits from the fixture.
 * @return Exact binary64 value.
 */
double number(std::string_view text) {
    return std::bit_cast<double>(static_cast<std::uint64_t>(std::stoull(std::string(text), nullptr, 16)));
}

/** @param value Owned model under test.
 * @return Public text/hash/ranges/precision observation.
 */
std::string describe(const PriceIncrements &value) {
    std::string ranges;

    for (auto item : value.getPriceIncrements()) {
        if (!ranges.empty()) {
            ranges += ',';
        }

        ranges += bits(item);
    }

    return hex(value.getText()) + "|" + std::to_string(value.hashCode()) + "|" + ranges + "|" +
           std::to_string(value.getPricePrecision());
}

/** @tparam F Synchronous observation function.
 * @param operation Operation producing an owned observation string.
 * @return Observation or the expected Java-compatible error category.
 */
template <typename F> std::string observe(F operation) {
    try {
        return operation();
    } catch (const std::domain_error &) {
        return "ARITHMETIC";
    } catch (const std::invalid_argument &) {
        return "INVALID";
    }
}

const auto BEFORE_MAIN_TEXT = PriceIncrements::empty().getText();
static_assert(std::is_nothrow_copy_constructible_v<PriceIncrements>);
static_assert(std::is_nothrow_move_constructible_v<PriceIncrements>);
} // namespace

TEST_CASE("pinned production price increment observations") {
    std::ifstream input(std::filesystem::path(ORG_TTLDTOR_QDCXX_SOURCE_DIR) /
                        "tests/fixtures/price_increments/oracle.tsv");
    REQUIRE(input.good());
    std::string line;
    std::size_t rows = 0;

    for (; std::getline(input, line); ++rows) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        auto c = split(line, '\t');
        CAPTURE(rows);
        CAPTURE(line);

        if (c[0] == "P") {
            auto text = unhex(c[1]);
            CHECK(observe([&] {
                      return describe(PriceIncrements::valueOf(view(text)));
                  }) == c[2]);
        } else if (c[0] == "S") {
            CHECK(observe([&] {
                      return describe(PriceIncrements::valueOf(number(c[1])));
                  }) == c[2]);
        } else if (c[0] == "A") {
            std::vector<double> values;

            if (c[1] != "-" && c[1] != "~") {
                for (auto cell : split(c[1], ',')) {
                    values.push_back(number(cell));
                }
            }

            CHECK(observe([&] {
                      return describe(PriceIncrements::valueOf(std::span<const double>(values)));
                  }) == c[2]);
        } else if (c[0] == "D") {
            auto value = PriceIncrements::valueOf(number(c[2]));
            CHECK(bits(value.roundPrice(number(c[1]))) == c[3]);
        } else {
            REQUIRE(c[0] == "Q");
            REQUIRE(c.size() == 10);
            auto text = unhex(c[1]);
            auto value = PriceIncrements::valueOf(view(text));
            auto price = number(c[2]);
            auto direction = static_cast<std::int32_t>(std::stoll(c[3]));
            CHECK(bits(value.getPriceIncrement(price, direction)) == c[5]);
            CHECK(value.getPricePrecision(price) == std::stoi(c[6]));
            CHECK(bits(value.roundPrice(price, direction)) == c[7]);
            auto modes = split(c[8], ',');
            REQUIRE(modes.size() == 8);

            for (std::size_t i = 0; i < modes.size(); ++i) {
                CAPTURE(i);
                CHECK(observe([&] {
                          return bits(value.roundPrice(price, static_cast<PriceRoundingMode>(i)));
                      }) == modes[i]);
            }

            CHECK(observe([&] {
                      return bits(value.incrementPrice(price, direction, number(c[4])));
                  }) == c[9]);
        }
    }

    CHECK(rows == 42594);
}

TEST_CASE("owned immutable prices, exact identity and finite admission") {
    CHECK(BEFORE_MAIN_TEXT.empty());
    auto empty = PriceIncrements::empty();
    CHECK(empty.getPriceIncrements() == std::vector<double>{0});
    auto text = std::u16string(u"0.01 1; 0.05");
    auto value = PriceIncrements::valueOf(std::u16string_view(text));
    text.assign(u"changed");
    CHECK(value.getText() == u"0.01 1; 0.05");
    auto ranges = value.getPriceIncrements();
    ranges[0] = 9;
    CHECK(value.getPriceIncrement() == .01);
    auto copy = value;
    auto moved = std::move(value);
    CHECK(value == empty);
    CHECK(value.getPriceIncrement() == 0);
    CHECK(copy == moved);
    CHECK_FALSE(PriceIncrements::valueOf(std::u16string_view(u".01")) == PriceIncrements::valueOf(.01));
    CHECK_NOTHROW((void)PriceIncrements::valueOf(std::u16string_view(u".01"), {3, 1}));
    CHECK_THROWS_AS((void)PriceIncrements::valueOf(std::u16string_view(u".01"), {2, 1}), std::length_error);
    CHECK_THROWS_AS((void)PriceIncrements::valueOf(std::u16string_view(u".01"), {3, 0}), std::length_error);
    CHECK_NOTHROW((void)PriceIncrements::valueOf(std::nullopt, {0, 0}));
    CHECK_THROWS_AS((void)PriceIncrements::valueOf(std::u16string_view(u"0"), {0, 0}), std::length_error);
    CHECK_THROWS_AS((void)PriceIncrements::valueOf(std::span<const double>(ranges), {8, 2}), std::length_error);
    CHECK_NOTHROW((void)PriceIncrements::valueOf(std::u16string_view(u"0.01 1; 0.05"), {12, 2}));
    CHECK_THROWS_AS((void)PriceIncrements::valueOf(std::u16string_view(u"0.01 1; 0.05"), {12, 1}), std::length_error);
    auto nan = std::bit_cast<double>(std::uint64_t{0x7ff8000000000123});
    CHECK(std::bit_cast<std::uint64_t>(moved.roundPrice(nan)) == std::bit_cast<std::uint64_t>(nan));
    CHECK_THROWS_AS((void)moved.roundPrice(.014, PriceRoundingMode::UNNECESSARY), std::domain_error);
    CHECK_THROWS_AS((void)moved.roundPrice(.014, static_cast<PriceRoundingMode>(99)), std::invalid_argument);
    CHECK(empty.roundPrice(.014, static_cast<PriceRoundingMode>(99)) == .014);
}

TEST_CASE("allocation failure cannot replace an existing immutable rule") {
    auto original = PriceIncrements::valueOf(std::u16string_view(u"0.05"));
    bool completed = false;
    int failures = 0;

    for (int allocation = 0; allocation < 64 && !completed; ++allocation) {
        auto value = original;
        failAllocationAfter = allocation;
        try {
            value = PriceIncrements::valueOf(std::u16string_view(u"0.01 1; 0.05 10; 0.1"));
            failAllocationAfter = -1;
            completed = true;
            CHECK(value.getPriceIncrement() == .01);
        } catch (const std::bad_alloc &) {
            failAllocationAfter = -1;
            ++failures;
            CHECK(value == original);
        }
    }

    CHECK(completed);
    CHECK(failures >= 4);
}
