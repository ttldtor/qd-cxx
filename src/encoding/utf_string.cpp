// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/encoding/utf_string.hpp"

#include "compact_limits.hpp"
#include "qd/encoding/utf_char.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>
#include <utility>

namespace org::ttldtor::qdcxx::encoding {
namespace {
/// Null is distinct from the zero-length empty string in the compact-long envelope.
constexpr std::int64_t NULL_LENGTH = -1;
/// Java accepts Integer.MAX_VALUE * 4 encoded body bytes; this is not a local resource budget.
constexpr std::uint64_t MAX_BODY_BYTES = 8589934588ULL;
/// Wire octet width, used when constructing a unary UTF lead prefix.
constexpr unsigned int BITS_PER_BYTE = 8;
/// Mask from which the leading ones of a multibyte UTF prefix are formed.
constexpr unsigned int BYTE_MASK = 0xffU;
/// ASCII occupies one byte and carries seven data bits.
constexpr std::uint32_t ASCII_LIMIT = 0x80;
/// First code point requiring three encoded bytes.
constexpr std::uint32_t THREE_BYTE_MIN = 0x800;
/// First supplementary code point; subtract it before splitting into a surrogate pair.
constexpr std::uint32_t SUPPLEMENTARY_MIN = 0x10000;
/// Inclusive ranges of high and low UTF-16 surrogate units.
constexpr std::uint32_t HIGH_SURROGATE_MIN = 0xd800;
constexpr std::uint32_t HIGH_SURROGATE_MAX = 0xdbff;
constexpr std::uint32_t LOW_SURROGATE_MIN = 0xdc00;
constexpr std::uint32_t LOW_SURROGATE_MAX = 0xdfff;
/// Each surrogate contributes ten bits to the supplementary code point offset.
constexpr unsigned int SURROGATE_PAYLOAD_BITS = 10;
constexpr std::uint32_t SURROGATE_PAYLOAD_MASK = 0x3ff;
/// A supplementary code point occupies two UTF-16 units.
constexpr std::size_t SURROGATE_PAIR_UNITS = 2;
/// Encoded sequence sizes, including their lead byte.
constexpr std::size_t ASCII_BYTES = 1;
constexpr std::size_t TWO_BYTE_SEQUENCE = 2;
constexpr std::size_t THREE_BYTE_SEQUENCE = 3;
constexpr std::size_t FOUR_BYTE_SEQUENCE = 4;
/// Continuation octets begin with 10 and contribute six low data bits.
constexpr unsigned int CONTINUATION_PREFIX = 0x80;
constexpr unsigned int CONTINUATION_PAYLOAD_BITS = 6;
constexpr unsigned int CONTINUATION_PAYLOAD_MASK = 0x3f;

/** Read one writer character, combining only a valid adjacent surrogate pair.
 * @param value Borrowed input units.
 * @param index Valid unit index, advanced over one unit or a pair.
 * @return Code point or unchanged isolated surrogate unit.
 */
std::uint32_t nextCharacter(std::u16string_view value, std::size_t &index) noexcept {
    std::uint32_t point = value[index++];

    if (point >= HIGH_SURROGATE_MIN && point <= HIGH_SURROGATE_MAX && index < value.size() &&
        value[index] >= LOW_SURROGATE_MIN && value[index] <= LOW_SURROGATE_MAX) {
        point = SUPPLEMENTARY_MIN + ((point - HIGH_SURROGATE_MIN) << SURROGATE_PAYLOAD_BITS) +
                (value[index++] - LOW_SURROGATE_MIN);
    }

    return point;
}

/** Count canonical body bytes for a code point or isolated surrogate.
 * @param point Value returned by nextCharacter.
 * @return One through four encoded bytes.
 */
std::size_t characterSize(std::uint32_t point) noexcept {
    return point < ASCII_LIMIT         ? ASCII_BYTES
           : point < THREE_BYTE_MIN    ? TWO_BYTE_SEQUENCE
           : point < SUPPLEMENTARY_MIN ? THREE_BYTE_SEQUENCE
                                       : FOUR_BYTE_SEQUENCE;
}
} // namespace

namespace {
/** Shared allocator-independent UTFString grammar.
 * @tparam String Explicit owned UTF-16 string type.
 * @tparam Result Matching primitive result type.
 * @param input Borrowed encoded bytes.
 * @param limits Finite byte/unit limits.
 * @param value Empty string carrying the selected allocator.
 * @return Exact primitive outcome, never a partial failed string.
 */
template <typename String, typename Result>
Result readText(std::span<const std::byte> input, UtfStringLimits limits, String value) {
    const auto prefix = readCompactLong(input);
    auto position = prefix.consumed;

    if (prefix.status != Status::OK) {
        return {std::nullopt, position, prefix.status};
    }

    if (prefix.value < NULL_LENGTH || prefix.value > static_cast<std::int64_t>(MAX_BODY_BYTES)) {
        return {std::nullopt, position, Status::INVALID_LENGTH};
    }

    if (prefix.value == NULL_LENGTH) {
        return {std::nullopt, position, Status::OK};
    }

    if (static_cast<std::uint64_t>(prefix.value) > limits.bodyBytes) {
        return {std::nullopt, position, Status::RESOURCE_LIMIT};
    }

    auto remaining = prefix.value;

    if constexpr (std::is_same_v<String, std::pmr::u16string>) {
        // A final supplementary overrun can append one extra unit before invalid-body classification.
        const auto bodyUnits = static_cast<std::uint64_t>(prefix.value) + 1;
        const auto reserved = std::min(
            {bodyUnits, static_cast<std::uint64_t>(limits.codeUnits), static_cast<std::uint64_t>(input.size())});
        value.reserve(static_cast<std::size_t>(reserved));
    }

    while (remaining > 0) {
        // The complete available remainder preserves full-tail consumption past the declared body.
        const auto character = readUtfChar(input.subspan(position));
        position += character.consumed;

        if (character.status != Status::OK) {
            return {std::nullopt, position, character.status};
        }

        const auto point = static_cast<std::uint32_t>(character.value);
        const std::size_t units = point >= SUPPLEMENTARY_MIN ? SURROGATE_PAIR_UNITS : 1;

        if (units > limits.codeUnits - value.size()) {
            return {std::nullopt, position, Status::RESOURCE_LIMIT};
        }

        if (units == SURROGATE_PAIR_UNITS) {
            value.push_back(
                static_cast<char16_t>(HIGH_SURROGATE_MIN + ((point - SUPPLEMENTARY_MIN) >> SURROGATE_PAYLOAD_BITS)));
            value.push_back(
                static_cast<char16_t>(LOW_SURROGATE_MIN + ((point - SUPPLEMENTARY_MIN) & SURROGATE_PAYLOAD_MASK)));
        } else {
            value.push_back(static_cast<char16_t>(point));
        }

        remaining -= static_cast<std::int64_t>(character.consumed);
    }

    if (remaining < 0) {
        return {std::nullopt, position, Status::INVALID_ENCODING};
    }

    return {std::move(value), position, Status::OK};
}

} // namespace

UtfStringResult readUtfString(std::span<const std::byte> input, UtfStringLimits limits) {
    return readText<std::u16string, UtfStringResult>(input, limits, {});
}

PmrUtfStringResult readUtfString(std::span<const std::byte> input, UtfStringLimits limits,
                                 std::pmr::memory_resource &resource) {
    return readText<std::pmr::u16string, PmrUtfStringResult>(input, limits, std::pmr::u16string{&resource});
}

WriteResult writeUtfString(std::optional<std::u16string_view> value, std::span<std::byte> output,
                           UtfStringLimits limits) noexcept {
    std::uint64_t bodySize = 0;

    if (value) {
        if (value->size() > MAX_BODY_BYTES) {
            return {0, Status::INVALID_LENGTH};
        }

        if (value->size() > limits.codeUnits) {
            return {0, Status::RESOURCE_LIMIT};
        }

        for (std::size_t index = 0; index < value->size();) {
            const auto size = characterSize(nextCharacter(*value, index));

            if (size > MAX_BODY_BYTES - bodySize) {
                return {0, Status::INVALID_LENGTH};
            }

            bodySize += size;
        }

        if (bodySize > limits.bodyBytes) {
            return {0, Status::RESOURCE_LIMIT};
        }
    }

    std::array<std::byte, detail::MAX_COMPACT_LONG_BYTES> prefix{};
    const auto encoded = writeCompactLong(value ? static_cast<std::int64_t>(bodySize) : NULL_LENGTH, prefix);

    if (bodySize > std::numeric_limits<std::size_t>::max() - encoded.written || output.size() < encoded.written ||
        bodySize > output.size() - encoded.written) {
        return {0, Status::INSUFFICIENT_OUTPUT};
    }

    std::size_t position = 0;

    for (; position < encoded.written; ++position) {
        output[position] = prefix[position];
    }

    if (value) {
        for (std::size_t index = 0; index < value->size();) {
            const auto point = nextCharacter(*value, index);
            const auto size = characterSize(point);

            if (size == ASCII_BYTES) {
                output[position++] = static_cast<std::byte>(point);
            } else {
                // Form size leading ones; high code-point bits follow the zero separator.
                output[position++] = static_cast<std::byte>((BYTE_MASK << (BITS_PER_BYTE - size)) |
                                                            (point >> (CONTINUATION_PAYLOAD_BITS * (size - 1))));

                for (std::size_t tail = size - 1; tail > 0; --tail) {
                    output[position++] = static_cast<std::byte>(
                        CONTINUATION_PREFIX |
                        ((point >> (CONTINUATION_PAYLOAD_BITS * (tail - 1))) & CONTINUATION_PAYLOAD_MASK));
                }
            }
        }
    }

    return {position, Status::OK};
}
} // namespace org::ttldtor::qdcxx::encoding
