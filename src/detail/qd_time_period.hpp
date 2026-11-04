// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "java_double.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::detail {
/** Parse the QD TimePeriod source suffix; even inf is a finite numeric delay under lifetime bounds.
 * @param text Borrowed source suffix, already within the source-byte admission.
 * @return Nonnegative representable cooperative delay, or invalid/out-of-profile text.
 */
inline std::optional<std::chrono::milliseconds> parseQdTimePeriod(std::string_view text) {
    using Count = std::chrono::milliseconds::rep;
    const auto maximum = std::numeric_limits<Count>::max();
    // JDK8 Character.isDigit(char)/digit(char,10), captured by the bounded oracle.
    constexpr std::array<unsigned, 35> digitStarts{
        0x30,   0x660,  0x6f0,  0x7c0,  0x966,  0x9e6,  0xa66,  0xae6,  0xb66,  0xbe6,  0xc66,  0xce6,
        0xd66,  0xe50,  0xed0,  0xf20,  0x1040, 0x1090, 0x17e0, 0x1810, 0x1946, 0x19d0, 0x1a80, 0x1a90,
        0x1b50, 0x1bb0, 0x1c40, 0x1c50, 0xa620, 0xa8d0, 0xa900, 0xa9d0, 0xaa50, 0xabf0, 0xff10};
    const auto digit = [&](std::size_t offset) -> std::optional<std::pair<unsigned, std::size_t>> {
        if (offset == text.size()) {
            return {};
        }

        unsigned code = static_cast<unsigned char>(text[offset++]);
        unsigned count = 0;
        unsigned minimum = 0;

        if (code >= 0xc2 && code <= 0xdf) {
            code &= 0x1f;
            count = 1;
            minimum = 0x80;
        } else if (code >= 0xe0 && code <= 0xef) {
            code &= 0x0f;
            count = 2;
            minimum = 0x800;
        } else if (code >= 0x80) {
            return {};
        }

        if (count > text.size() - offset) {
            return {};
        }

        for (unsigned index = 0; index < count; ++index) {
            const auto next = static_cast<unsigned char>(text[offset++]);

            if ((next & 0xc0) != 0x80) {
                return {};
            }

            code = (code << 6) | (next & 0x3f);
        }

        if (code < minimum || (code >= 0xd800 && code <= 0xdfff)) {
            return {};
        }

        for (const auto start : digitStarts) {
            if (code >= start && code - start < 10) {
                return std::pair{code - start, offset};
            }
        }

        return {};
    };
    const auto asciiEqual = [](char actual, char lower) {
        return actual == lower || actual == static_cast<char>(lower - 'a' + 'A');
    };

    if ((text.size() == 3 && asciiEqual(text[0], 'i') && asciiEqual(text[1], 'n') && asciiEqual(text[2], 'f')) ||
        (text.size() == 4 && (text.substr(0, 2) == "\xc4\xb0" || text.substr(0, 2) == "\xc4\xb1") &&
         asciiEqual(text[2], 'n') && asciiEqual(text[3], 'f'))) {
        return std::chrono::milliseconds(maximum);
    }

    std::size_t position = 0;
    Count result = 0;
    bool any = false;
    const auto letter = [&](char value) {
        if (position == text.size()) {
            return false;
        }

        const auto current = text[position];

        return current == value || current == static_cast<char>(value + 'a' - 'A');
    };
    const auto digits = [&] {
        const auto start = position;

        while (const auto next = digit(position)) {
            position = next->second;
        }

        return start;
    };
    const auto integer = [&](std::size_t start, Count scale) {
        if (start == position) {
            return false;
        }

        std::uint64_t value = 0;

        for (auto offset = start; offset < position;) {
            const auto next = digit(offset);

            if (!next ||
                value > (static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) - next->first) / 10) {
                return false;
            }

            value = value * 10 + next->first;
            offset = next->second;
        }

        if (value > static_cast<std::uint64_t>((maximum - result) / scale)) {
            return false;
        }

        result += static_cast<Count>(value) * scale;
        any = true;

        return true;
    };

    if (letter('P')) {
        ++position;
    }

    auto start = digits();

    if (letter('D')) {
        if (!integer(start, 86400000)) {
            return {};
        }

        ++position;
        start = digits();
    }

    if (letter('T')) {
        if (start != position) {
            return {};
        }

        ++position;
        start = digits();
    }

    if (letter('H')) {
        if (!integer(start, 3600000)) {
            return {};
        }

        ++position;
        start = digits();
    }

    if (letter('M')) {
        if (!integer(start, 60000)) {
            return {};
        }

        ++position;
        start = digits();
    }

    if (position < text.size() && text[position] == '.') {
        ++position;
        (void)digits();
    }

    if (start != position) {
        std::u16string seconds;
        seconds.reserve(position - start);

        for (auto i = start; i < position; ++i) {
            if (static_cast<unsigned char>(text[i]) >= 0x80) {
                return {};
            }

            seconds.push_back(static_cast<char16_t>(text[i]));
        }

        const auto parsed = org::ttldtor::qdcxx::detail::parseJavaDouble(seconds);

        if (!parsed) {
            return {};
        }

        const auto scaled = *parsed * 1000;
        const auto whole = std::floor(scaled);
        auto millis = org::ttldtor::qdcxx::detail::javaLong(whole);

        if (millis != maximum && scaled - whole >= 0.5) {
            ++millis;
        }

        if (millis < 0 || millis > maximum - result) {
            return {};
        }

        result += millis;
        any = true;

        if (letter('S')) {
            ++position;
        } else if (text.substr(position, 2) == "\xc5\xbf") {
            position += 2;
        }
    }

    if (!any || position != text.size()) {
        return {};
    }

    return std::chrono::milliseconds(result);
}

} // namespace org::ttldtor::qdcxx::detail
