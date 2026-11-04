// Copyright (C) 2002 - 2024 Devexperts LLC.
// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, see https://mozilla.org/MPL/2.0/.
#include "../detail/java_double.hpp"
#include "numeric_text.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>

namespace org::ttldtor::qdcxx::glossary::detail {
namespace {
constexpr std::size_t NUMBER_BUFFER_CAPACITY = 64;
constexpr std::size_t FORMAT_PRECISION = 15;
constexpr int MAX_INTEGER_DECIMAL_EXPONENT = 16;
constexpr double SIGNED_LONG_LIMIT = 0x1p63;
constexpr int BINARY64_SIGNIFICAND_AND_MARGIN_BITS = 54;
} // namespace

double roundPriceDecimal(double value) {
    if (!std::isfinite(value) || value == 0) {
        return value;
    }

    auto magnitude = std::abs(value);
    std::array<char, NUMBER_BUFFER_CAPACITY> buffer{};
    auto conversion =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), magnitude, std::chars_format::scientific);
    std::string_view raw(buffer.data(), static_cast<std::size_t>(conversion.ptr - buffer.data()));
    auto e = raw.find('e');
    int exponent = 0;
    auto exponentText = raw.substr(e + 1);

    if (exponentText.front() == '+') {
        exponentText.remove_prefix(1);
    }

    std::from_chars(exponentText.data(), exponentText.data() + exponentText.size(), exponent);
    std::string digits;

    for (auto ch : raw.substr(0, e)) {
        if (ch != '.') {
            digits += ch;
        }
    }

    bool integerMantissa =
        exponent >= 0 && exponent <= MAX_INTEGER_DECIMAL_EXPONENT && magnitude == std::nearbyint(magnitude);

    if (integerMantissa) {
        auto integer =
            std::to_chars(buffer.data(), buffer.data() + buffer.size(), static_cast<std::uint64_t>(magnitude));
        digits.assign(buffer.data(), integer.ptr);
    }

    if (digits.size() <= FORMAT_PRECISION) {
        return value;
    }

    bool tie = digits[FORMAT_PRECISION] == '5' &&
               std::all_of(digits.begin() + FORMAT_PRECISION + 1, digits.end(), [](char ch) {
                   return ch == '0';
               });

    if (!tie && !integerMantissa) {
        return *org::ttldtor::qdcxx::detail::parseJavaDouble(formatAdditionalNumber(value));
    }

    if (tie) {
        // The numerical cleanup takes its exact tie fallback from Java Double.toString,
        // unlike the text formatter's shortest-digit rounding path.
        auto decimal = org::ttldtor::qdcxx::detail::decimalDigits(magnitude);
        digits = std::move(decimal.digits);
        exponent = decimal.exponent;

        if (magnitude < SIGNED_LONG_LIMIT && magnitude == std::nearbyint(magnitude)) {
            auto integer = static_cast<std::uint64_t>(magnitude);
            auto binaryExponent = static_cast<int>(std::bit_width(integer)) - 1;
            auto insignificantPower = std::max(0, binaryExponent - BINARY64_SIGNIFICAND_AND_MARGIN_BITS);
            auto radius = std::uint64_t{1} << insignificantPower;
            std::uint64_t divisor = 1;
            int discardedDigits = 0;

            while (radius >= 10) {
                radius /= 10;
                divisor *= 10;
                ++discardedDigits;
            }

            auto rounded = integer / divisor;

            if (divisor > 1 && integer % divisor >= divisor / 2) {
                ++rounded;
            }

            digits = std::to_string(rounded);
            exponent = static_cast<int>(digits.size()) + discardedDigits - 1;
        }
    }

    if (digits.size() > FORMAT_PRECISION) {
        bool up = digits[FORMAT_PRECISION] >= '5';
        digits.resize(FORMAT_PRECISION);

        if (up) {
            auto i = digits.size();

            while (i > 0 && digits[i - 1] == '9') {
                digits[--i] = '0';
            }

            if (i == 0) {
                digits = "1";
                ++exponent;
            } else {
                ++digits[i - 1];
            }
        }
    }

    auto ascii = (value < 0 ? "-" : "") + digits + "E" + std::to_string(exponent - static_cast<int>(digits.size()) + 1);

    return *org::ttldtor::qdcxx::detail::parseJavaDouble(std::u16string(ascii.begin(), ascii.end()));
}
} // namespace org::ttldtor::qdcxx::glossary::detail
