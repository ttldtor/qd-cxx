// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/encoding/char_array.hpp"

#include "qd/encoding/utf_char.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::encoding {

CharArrayResult readCharArray(std::span<const std::byte> input, CharArrayLimits limits,
                              CharArrayBoundary boundary) noexcept {
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
        std::u16string value;
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

} // namespace org::ttldtor::qdcxx::encoding
