// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/short_string.hpp"

#include <array>
#include <bit>
#include <cstddef>

namespace org::ttldtor::qdcxx::events {
namespace {
constexpr std::size_t MAX_SHORT_STRING_UNITS = 8;
constexpr char16_t MAX_SHORT_STRING_CHARACTER = 0xff;
constexpr unsigned BITS_PER_CHARACTER = 8;
} // namespace

std::expected<std::int64_t, EventValueError> encodeShortString(std::optional<std::u16string_view> text) noexcept {
    if (!text) {
        return 0;
    }

    if (text->size() > MAX_SHORT_STRING_UNITS) {
        return std::unexpected(EventValueError::SHORT_STRING_TOO_LONG);
    }

    std::uint64_t code = 0;

    for (const auto character : *text) {
        if (character > MAX_SHORT_STRING_CHARACTER) {
            return std::unexpected(EventValueError::INVALID_SHORT_STRING_CHARACTER);
        }

        if (character != 0) {
            code = (code << BITS_PER_CHARACTER) | character;
        }
    }

    return std::bit_cast<std::int64_t>(code);
}

std::optional<std::u16string> decodeShortString(std::int64_t code) {
    if (code == 0) {
        return std::nullopt;
    }

    std::array<char16_t, MAX_SHORT_STRING_UNITS> reverse{};
    std::size_t length = 0;
    auto remaining = static_cast<std::uint64_t>(code);

    while (remaining != 0) {
        const auto character = static_cast<char16_t>(remaining & MAX_SHORT_STRING_CHARACTER);

        if (character != 0) {
            reverse[length++] = character;
        }

        remaining >>= BITS_PER_CHARACTER;
    }

    std::u16string result(length, 0);

    for (std::size_t index = 0; index < length; ++index) {
        result[index] = reverse[length - index - 1];
    }

    return result;
}
} // namespace org::ttldtor::qdcxx::events
