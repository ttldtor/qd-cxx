// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/encoding/utf_string.hpp"

#include <array>
#include <limits>
#include <utility>

namespace org::ttldtor::qdcxx::encoding {
namespace {
/// Null is distinct from the zero-length empty string in the compact-long envelope.
constexpr std::int64_t NULL_LENGTH = -1;
/// Java accepts Integer.MAX_VALUE * 4 encoded body bytes; this is not a local resource budget.
constexpr std::uint64_t MAX_BODY_BYTES = 8589934588ULL;
/// The compact-long length envelope needs at most one escape plus eight payload bytes.
constexpr std::size_t MAX_LENGTH_PREFIX_BYTES = 9;
/// Wire octet width, used when constructing a unary UTF lead prefix.
constexpr unsigned int BITS_PER_BYTE = 8;
/// Mask from which the leading ones of a multibyte UTF prefix are formed.
constexpr unsigned int BYTE_MASK = 0xffU;
/// ASCII occupies one byte and carries seven data bits.
constexpr std::uint32_t ASCII_LIMIT = 0x80;
constexpr unsigned int ASCII_PAYLOAD_MASK = 0x7f;
/// First code point requiring three encoded bytes.
constexpr std::uint32_t THREE_BYTE_MIN = 0x800;
/// First supplementary code point; subtract it before splitting into a surrogate pair.
constexpr std::uint32_t SUPPLEMENTARY_MIN = 0x10000;
/// Largest accepted Unicode code point (surrogates below it remain accepted by QD).
constexpr std::uint32_t MAX_CODE_POINT = 0x10ffff;
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
/// Zero is the internal classification result for an invalid lead byte.
constexpr std::size_t INVALID_SEQUENCE_SIZE = 0;
/// Masks and prefixes classify lead bytes without rejecting QD's overlong forms.
constexpr unsigned int TWO_BYTE_LEAD_MASK = 0xe0;
constexpr unsigned int TWO_BYTE_LEAD_PREFIX = 0xc0;
constexpr unsigned int THREE_BYTE_LEAD_MASK = 0xf0;
constexpr unsigned int THREE_BYTE_LEAD_PREFIX = 0xe0;
constexpr unsigned int FOUR_BYTE_LEAD_MASK = 0xf8;
constexpr unsigned int FOUR_BYTE_LEAD_PREFIX = 0xf0;
/// Continuation octets begin with 10 and contribute six low data bits.
constexpr unsigned int CONTINUATION_MASK = 0xc0;
constexpr unsigned int CONTINUATION_PREFIX = 0x80;
constexpr unsigned int CONTINUATION_PAYLOAD_BITS = 6;
constexpr unsigned int CONTINUATION_PAYLOAD_MASK = 0x3f;

/** Classify a lead byte using Java's accepted UTFString grammar.
 * @param first Unsigned lead octet.
 * @return Sequence size including the lead, or INVALID_SEQUENCE_SIZE for a bad lead.
 */
std::size_t sequenceSize(unsigned int first) noexcept {
    if (first < ASCII_LIMIT) {
        return ASCII_BYTES;
    }

    if ((first & TWO_BYTE_LEAD_MASK) == TWO_BYTE_LEAD_PREFIX) {
        return TWO_BYTE_SEQUENCE;
    }

    if ((first & THREE_BYTE_LEAD_MASK) == THREE_BYTE_LEAD_PREFIX) {
        return THREE_BYTE_SEQUENCE;
    }

    if ((first & FOUR_BYTE_LEAD_MASK) == FOUR_BYTE_LEAD_PREFIX) {
        return FOUR_BYTE_SEQUENCE;
    }

    return INVALID_SEQUENCE_SIZE;
}

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

UtfStringResult readUtfString(std::span<const std::byte> input, UtfStringLimits limits) {
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
    std::u16string value;

    while (remaining > 0) {
        if (position == input.size()) {
            return {std::nullopt, position, Status::INSUFFICIENT_INPUT};
        }

        const auto first = std::to_integer<unsigned int>(input[position++]);
        const auto size = sequenceSize(first);

        if (size == INVALID_SEQUENCE_SIZE) {
            return {std::nullopt, position, Status::INVALID_ENCODING};
        }

        // Java reads the entire tail before checking any continuation bits.
        if (input.size() - position < size - 1) {
            return {std::nullopt, input.size(), Status::INSUFFICIENT_INPUT};
        }

        // A multibyte lead spends size leading ones and a zero; the low (7 - size) bits carry data.
        std::uint32_t point = first & (size == ASCII_BYTES ? ASCII_PAYLOAD_MASK : ASCII_PAYLOAD_MASK >> size);
        bool valid = true;

        for (std::size_t index = 1; index < size; ++index) {
            const auto tail = std::to_integer<unsigned int>(input[position++]);
            valid = valid && (tail & CONTINUATION_MASK) == CONTINUATION_PREFIX;
            point = (point << CONTINUATION_PAYLOAD_BITS) | (tail & CONTINUATION_PAYLOAD_MASK);
        }

        if (!valid || point > MAX_CODE_POINT) {
            return {std::nullopt, position, Status::INVALID_ENCODING};
        }

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

        remaining -= static_cast<std::int64_t>(size);
    }

    if (remaining < 0) {
        return {std::nullopt, position, Status::INVALID_ENCODING};
    }

    return {std::move(value), position, Status::OK};
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

    std::array<std::byte, MAX_LENGTH_PREFIX_BYTES> prefix{};
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
