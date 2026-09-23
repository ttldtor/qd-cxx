// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace org::ttldtor::qdcxx::inspect::detail {

/** Parse one complete unsigned CLI budget; leading zeroes are permitted.
 * @param text Borrowed operand containing only ASCII decimal digits.
 * @param value Receives the count on success, unchanged on failure.
 * @return Whether the operand fits both uint64_t and size_t without signs or whitespace.
 */
inline bool parseUnsignedCount(std::string_view text, std::size_t &value) noexcept {
    if (text.empty()) {
        return false;
    }

    std::uint64_t parsed{};
    // Unsigned from_chars rejects signs and whitespace; full consumption excludes any suffix.
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);

    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        parsed > std::numeric_limits<std::size_t>::max()) {
        return false;
    }

    value = static_cast<std::size_t>(parsed);

    return true;
}

} // namespace org::ttldtor::qdcxx::inspect::detail
