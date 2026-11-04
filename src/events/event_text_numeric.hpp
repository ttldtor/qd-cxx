// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events::detail {
/** Select and format the current Java binary64 decimal without ambient locale state.
 * @param value Any IEEE 754 binary64 value, including signed zeros and nonfinite values.
 * @return Independently owned UTF-16 text of at most 24 units.
 * @throws std::bad_alloc Output allocation fails.
 * @throws std::length_error The implementation cannot allocate the bounded output.
 */
inline std::u16string eventDoubleText(double value) {
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);

    if (std::isnan(value)) {
        return u"NaN";
    }

    if (std::isinf(value)) {
        return std::signbit(value) ? u"-Infinity" : u"Infinity";
    }

    if (value == 0) {
        return std::signbit(value) ? u"-0.0" : u"0.0";
    }

    constexpr std::size_t CONVERSION_CAPACITY = 32;
    std::array<char, CONVERSION_CAPACITY> buffer{};
    const auto magnitude = std::abs(value);
    auto converted =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), magnitude, std::chars_format::scientific);

    if (converted.ec != std::errc{}) {
        throw std::length_error("Binary64 scientific conversion capacity exceeded");
    }

    auto scientific = std::string_view(buffer.data(), converted.ptr);
    auto exponentPosition = scientific.find('e');

    // Java selects the nearest decimal of length one OR two when a one-digit decimal round-trips.
    // Scientific precision one finds that candidate; removing a trailing zero restores its unique significand.
    if (exponentPosition == 1) {
        converted =
            std::to_chars(buffer.data(), buffer.data() + buffer.size(), magnitude, std::chars_format::scientific, 1);

        if (converted.ec != std::errc{}) {
            throw std::length_error("Binary64 two-digit conversion capacity exceeded");
        }

        scientific = std::string_view(buffer.data(), converted.ptr);
        exponentPosition = scientific.find('e');
    }

    std::string digits;

    for (auto unit : scientific.substr(0, exponentPosition)) {
        if (unit != '.') {
            digits.push_back(unit);
        }
    }

    while (digits.size() > 1 && digits.back() == '0') {
        digits.pop_back();
    }

    auto exponentText = scientific.substr(exponentPosition + 1);

    if (exponentText.front() == '+') {
        exponentText.remove_prefix(1);
    }

    int exponent = 0;
    std::from_chars(exponentText.data(), exponentText.data() + exponentText.size(), exponent);
    std::string text;

    if (exponent >= -3 && exponent < 7) {
        const auto point = exponent + 1;

        if (point <= 0) {
            text = "0." + std::string(static_cast<std::size_t>(-point), '0') + digits;
        } else if (static_cast<std::size_t>(point) >= digits.size()) {
            text = digits + std::string(static_cast<std::size_t>(point) - digits.size(), '0') + ".0";
        } else {
            text = digits;
            text.insert(static_cast<std::size_t>(point), 1, '.');
        }
    } else {
        text = digits.substr(0, 1) + "." + (digits.size() == 1 ? "0" : digits.substr(1)) + "E";
        std::array<char, 12> exponentBuffer{};
        const auto exponentEnd =
            std::to_chars(exponentBuffer.data(), exponentBuffer.data() + exponentBuffer.size(), exponent).ptr;
        text.append(exponentBuffer.data(), exponentEnd);
    }

    if (std::signbit(value)) {
        text.insert(text.begin(), '-');
    }

    return {text.begin(), text.end()};
}
} // namespace org::ttldtor::qdcxx::events::detail
