// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/core/wide_decimal.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <limits>

namespace org::ttldtor::qdcxx::core {
static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53,
              "WideDecimal views require IEEE 754 binary64");

namespace {
/// Rank occupies the low octet; rank 128 represents an unscaled integer.
constexpr unsigned int RANK_BITS = 8;
constexpr std::uint64_t RANK_MASK = 0xff;
constexpr int ZERO_EXPONENT_RANK = 128;
/// Maximum significand digits plus sign, decimal point, E, exponent and spare capacity.
constexpr std::size_t MAX_TEXT_BYTES = 32;
/// Plain notation permits this many zeroes outside the significand digits.
constexpr int MAX_PLAIN_ZEROES = 6;
/// Decimal base used only to remove insignificant trailing zeroes.
constexpr std::int64_t DECIMAL_RADIX = 10;
/// Canonical quiet NaN payload, independent of host library payload choices.
constexpr std::uint64_t QUIET_NAN_BITS = 0x7ff8000000000000ULL;
} // namespace

std::int64_t WideDecimal::significand() const noexcept {
    return raw >> RANK_BITS;
}

int WideDecimal::exponent() const noexcept {
    return ZERO_EXPONENT_RANK - static_cast<int>(static_cast<std::uint64_t>(raw) & RANK_MASK);
}

DecimalKind WideDecimal::kind() const noexcept {
    if ((static_cast<std::uint64_t>(raw) & RANK_MASK) != 0) {
        return DecimalKind::FINITE;
    }

    const auto value = significand();

    return value == 0  ? DecimalKind::NAN_VALUE
           : value > 0 ? DecimalKind::POSITIVE_INFINITY
                       : DecimalKind::NEGATIVE_INFINITY;
}

double WideDecimal::toDouble() const noexcept {
    switch (kind()) {
    case DecimalKind::NAN_VALUE:
        return std::bit_cast<double>(QUIET_NAN_BITS);
    case DecimalKind::POSITIVE_INFINITY:
        return std::numeric_limits<double>::infinity();
    case DecimalKind::NEGATIVE_INFINITY:
        return -std::numeric_limits<double>::infinity();
    case DecimalKind::FINITE:
        break;
    }

    const auto value = significand();

    if (value == 0) {
        return 0.0;
    }

    std::array<char, MAX_TEXT_BYTES> text{};
    auto *position = std::to_chars(text.data(), text.data() + text.size(), value).ptr;
    *position++ = 'e';
    const auto *end = std::to_chars(position, text.data() + text.size(), exponent()).ptr;
    double result{};
    // The complete 56-bit/rank domain fits both this buffer and finite binary64 exponents.
    static_cast<void>(std::from_chars(text.data(), end, result, std::chars_format::scientific));

    return result;
}

std::string WideDecimal::toString() const {
    switch (kind()) {
    case DecimalKind::NAN_VALUE:
        return "NaN";
    case DecimalKind::POSITIVE_INFINITY:
        return "Infinity";
    case DecimalKind::NEGATIVE_INFINITY:
        return "-Infinity";
    case DecimalKind::FINITE:
        break;
    }

    auto value = significand();

    if (value == 0) {
        return "0";
    }

    const bool negative = value < 0;

    if (negative) {
        value = -value;
    }

    auto scale = exponent();

    while (value % DECIMAL_RADIX == 0) {
        value /= DECIMAL_RADIX;
        ++scale;
    }

    std::array<char, MAX_TEXT_BYTES> digits{};
    const auto *end = std::to_chars(digits.data(), digits.data() + digits.size(), value).ptr;
    const auto count = static_cast<int>(end - digits.data());
    const auto decimalPoint = count + scale;
    std::string result;
    result.reserve(MAX_TEXT_BYTES);

    if (negative) {
        result += '-';
    }

    if (scale >= 0 && scale <= MAX_PLAIN_ZEROES) {
        result.append(digits.data(), static_cast<std::size_t>(count));
        result.append(static_cast<std::size_t>(scale), '0');
    } else if (scale < 0 && decimalPoint > 0) {
        result.append(digits.data(), static_cast<std::size_t>(decimalPoint));
        result += '.';
        result.append(digits.data() + decimalPoint, static_cast<std::size_t>(-scale));
    } else if (decimalPoint <= 0 && decimalPoint >= -MAX_PLAIN_ZEROES) {
        result += "0.";
        result.append(static_cast<std::size_t>(-decimalPoint), '0');
        result.append(digits.data(), static_cast<std::size_t>(count));
    } else {
        result += digits[0];

        if (count > 1) {
            result += '.';
            result.append(digits.data() + 1, static_cast<std::size_t>(count - 1));
        }

        result += 'E';
        std::array<char, MAX_TEXT_BYTES> exponentText{};
        const auto *exponentEnd =
            std::to_chars(exponentText.data(), exponentText.data() + exponentText.size(), decimalPoint - 1).ptr;
        result.append(exponentText.data(), static_cast<std::size_t>(exponentEnd - exponentText.data()));
    }

    return result;
}

} // namespace org::ttldtor::qdcxx::core
