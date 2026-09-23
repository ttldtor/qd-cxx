// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::test_support {
/** Split evidence rows without dropping empty cells, including the final cell.
 * @param text Complete row.
 * @param delimiter Cell separator.
 * @return Ordered owned cells.
 */
inline std::vector<std::string> split(std::string_view text, char delimiter = '\t') {
    std::vector<std::string> result;
    std::size_t start = 0;

    for (;;) {
        const auto end = text.find(delimiter, start);
        result.emplace_back(text.substr(start, end - start));

        if (end == text.npos) {
            return result;
        }

        start = end + 1;
    }
}

/** Decode lowercase UTF-16 evidence without interpreting Unicode or nullable policy.
 * @param text Four hex digits per unit; dash alone denotes empty.
 * @param grammarError Caller-specific diagnostic for malformed cells.
 * @return Owned exact units including NUL and unpaired surrogates, or throw on malformed input.
 */
inline std::u16string decodeUnits(std::string_view text, std::string_view grammarError) {
    if (text == "-") {
        return {};
    }

    constexpr std::size_t HEX_DIGITS_PER_UNIT = 4;

    if (text.empty() || text.size() % HEX_DIGITS_PER_UNIT != 0 ||
        text.find_first_not_of("0123456789abcdef") != text.npos) {
        throw std::runtime_error(std::string(grammarError));
    }

    std::u16string result;

    for (std::size_t i = 0; i < text.size(); i += HEX_DIGITS_PER_UNIT) {
        unsigned value{};
        // The whole-cell grammar already guarantees four valid hexadecimal digits.
        static_cast<void>(std::from_chars(text.data() + i, text.data() + i + HEX_DIGITS_PER_UNIT, value, 16));
        result.push_back(static_cast<char16_t>(value));
    }

    return result;
}

/** Encode exact UTF-16 comparison units without production formatting helpers.
 * @param value Borrowed units, without nullable interpretation.
 * @return Lowercase hex or the dash empty sentinel.
 */
inline std::string hexUnits(std::u16string_view value) {
    if (value.empty()) {
        return "-";
    }

    constexpr std::string_view DIGITS = "0123456789abcdef";
    std::string result;

    for (auto unit : value) {
        // Emit the four nibbles of one UTF-16 unit, most significant first.
        for (int shift = 12; shift >= 0; shift -= 4) {
            result += DIGITS[(unit >> shift) & 15];
        }
    }

    return result;
}
} // namespace org::ttldtor::qdcxx::test_support
