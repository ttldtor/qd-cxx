// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/encoding/char_array.hpp"

#include "qd/encoding/utf_char.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::encoding {

WriteResult writeCharArray(std::optional<std::u16string_view> value, std::span<std::byte> output,
                           CharArrayLimits limits) noexcept {
    constexpr std::int32_t NULL_LENGTH = -1;   ///< Nullable count prefix.
    constexpr std::size_t MAX_COUNT_BYTES = 5; ///< Full-width compact-int count.
    constexpr std::size_t MAX_UNIT_BYTES = 3;  ///< One BMP UTFChar, including a surrogate.
    const auto count = value ? value->size() : 0;

    if (count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return {0, Status::INVALID_LENGTH};
    }

    if (count > limits.codeUnits) {
        return {0, Status::RESOURCE_LIMIT};
    }

    std::array<std::byte, MAX_COUNT_BYTES> prefix{};
    const auto encoded = writeCompactInt(value ? static_cast<std::int32_t>(count) : NULL_LENGTH, prefix);
    auto size = encoded.written;

    if (size > limits.encodedBytes) {
        return {0, Status::RESOURCE_LIMIT};
    }

    if (value) {
        std::array<std::byte, MAX_UNIT_BYTES> character{};

        for (const auto unit : *value) {
            const auto written = writeUtfChar(static_cast<std::int32_t>(unit), character).written;

            // Subtraction checks both the configured bound and size_t representability.
            if (written > limits.encodedBytes - size) {
                return {0, Status::RESOURCE_LIMIT};
            }

            size += written;
        }
    }

    if (size > output.size()) {
        return {0, Status::INSUFFICIENT_OUTPUT};
    }

    std::copy_n(prefix.begin(), encoded.written, output.begin());
    auto position = encoded.written;

    if (value) {
        for (const auto unit : *value) {
            position += writeUtfChar(static_cast<std::int32_t>(unit), output.subspan(position)).written;
        }
    }

    return {position, Status::OK};
}

namespace {
/** Shared allocator-independent character-array grammar.
 * @tparam String Explicit owned UTF-16 string type.
 * @tparam Result Matching primitive result type.
 * @param input Borrowed encoded bytes.
 * @param limits Finite byte/unit limits.
 * @param value Empty string carrying the selected allocator.
 * @return Exact primitive outcome, never a partial failed string.
 */
template <typename String, typename Result>
Result readArray(std::span<const std::byte> input, CharArrayLimits limits, CharArrayBoundary boundary, String value) {
    constexpr std::int64_t NULL_LENGTH = -1;
    constexpr std::int32_t MAX_BMP_UNIT = 0xffff;
    const auto bounded = input.first(std::min(input.size(), limits.encodedBytes));
    const auto prefix = readCompactLong(bounded);
    auto position = prefix.consumed;
    const auto exhausted = bounded.size() < input.size() ? Status::RESOURCE_LIMIT : Status::INSUFFICIENT_INPUT;

    if (prefix.status != Status::OK) {
        return {std::nullopt, position, 0, exhausted};
    }

    if (prefix.value < NULL_LENGTH || prefix.value > std::numeric_limits<std::int32_t>::max()) {
        return {std::nullopt, position, 0, Status::INVALID_LENGTH};
    }

    if (prefix.value == NULL_LENGTH) {
        return {std::nullopt, position, 0, Status::OK};
    }

    const auto count = static_cast<std::size_t>(prefix.value);

    // The message check uses the original boundary, not an artificial operational byte cutoff.
    if (boundary == CharArrayBoundary::MESSAGE && count > input.size() - position) {
        return {std::nullopt, position, 0, Status::INVALID_LENGTH};
    }

    if (count > limits.codeUnits) {
        return {std::nullopt, position, 0, Status::RESOURCE_LIMIT};
    }

    try {
        value.reserve(count);

        for (std::size_t index = 0; index < count; ++index) {
            // UTFChar examines its complete indicated tail before validation, as required by FIELD-TYPES.
            const auto character = readUtfChar(bounded.subspan(position));
            position += character.consumed;

            if (character.status != Status::OK) {
                return {std::nullopt, position, count,
                        character.status == Status::INSUFFICIENT_INPUT ? exhausted : character.status};
            }

            if (character.value > MAX_BMP_UNIT) {
                return {std::nullopt, position, count, Status::INVALID_ENCODING};
            }

            value.push_back(static_cast<char16_t>(character.value));
        }

        return {std::move(value), position, count, Status::OK};
    } catch (const std::bad_alloc &) {
        return {std::nullopt, position, count, Status::ALLOCATION_FAILURE};
    } catch (const std::length_error &) {
        return {std::nullopt, position, count, Status::ALLOCATION_FAILURE};
    }
}

} // namespace

CharArrayResult readCharArray(std::span<const std::byte> input, CharArrayLimits limits,
                              CharArrayBoundary boundary) noexcept {
    return readArray<std::u16string, CharArrayResult>(input, limits, boundary, {});
}

PmrCharArrayResult readCharArray(std::span<const std::byte> input, CharArrayLimits limits, CharArrayBoundary boundary,
                                 std::pmr::memory_resource &resource) {
    return readArray<std::pmr::u16string, PmrCharArrayResult>(input, limits, boundary, std::pmr::u16string{&resource});
}
} // namespace org::ttldtor::qdcxx::encoding
