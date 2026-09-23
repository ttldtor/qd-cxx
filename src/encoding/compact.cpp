// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/encoding/compact.hpp"

#include "compact_limits.hpp"

#include <bit>

namespace org::ttldtor::qdcxx::encoding {
namespace {
/// Wire octets contain eight bits, independent of the signed value being encoded.
constexpr unsigned int BITS_PER_BYTE = 8;
/// High bit examined first when counting the unary length prefix.
constexpr unsigned int PREFIX_HIGH_BIT = 0x80U;
/// One-byte compact payload; each leading length bit removes one of these bits.
constexpr unsigned int SINGLE_BYTE_PAYLOAD_MASK = 0x7fU;
/// Bits available in the signed payload per compact byte before the full-width escape.
constexpr unsigned int PAYLOAD_BITS_PER_BYTE = 7;
/// Low octet extracted from the big-endian payload.
constexpr unsigned int BYTE_MASK = 0xffU;
/// All-one prefix selects the full 64-bit payload.
constexpr unsigned int FULL_WIDTH_PREFIX = 0xffU;
/// Width of the Java long bit pattern, including its sign bit.
constexpr unsigned int LONG_BITS = 64;

/** Choose the shortest compact representation of a signed long.
 * @param value Signed value to encode.
 * @return Encoded byte count, including the prefix byte.
 */
std::size_t compactLongSize(std::int64_t value) noexcept {
    for (std::size_t size = 1; size < detail::MAX_COMPACT_LONG_BYTES; ++size) {
        // Each extra octet adds eight payload bits but spends one more prefix bit: net seven.
        // One payload bit carries the sign, giving the interval [-2^(width-1), 2^(width-1)).
        const auto limit = std::int64_t{1} << (PAYLOAD_BITS_PER_BYTE * size - 1);

        if (value >= -limit && value < limit) {
            return size;
        }
    }

    return detail::MAX_COMPACT_LONG_BYTES;
}
} // namespace

ReadResult<std::int64_t> readCompactLong(std::span<const std::byte> input) noexcept {
    if (input.empty()) {
        return {0, 0, Status::INSUFFICIENT_INPUT};
    }

    const auto first = std::to_integer<unsigned int>(input[0]);
    unsigned int leading = 0;

    while (leading < BITS_PER_BYTE && (first & (PREFIX_HIGH_BIT >> leading)) != 0) {
        ++leading;
    }

    const auto size = static_cast<std::size_t>(leading + 1);

    if (input.size() < size) {
        return {0, input.size(), Status::INSUFFICIENT_INPUT};
    }

    std::uint64_t bits = leading < PAYLOAD_BITS_PER_BYTE ? first & (SINGLE_BYTE_PAYLOAD_MASK >> leading) : 0;

    for (std::size_t index = 1; index < size; ++index) {
        bits = (bits << BITS_PER_BYTE) | std::to_integer<unsigned int>(input[index]);
    }

    // The escape carries all 64 bits; shorter forms gain seven signed payload bits per byte.
    const unsigned int width = leading == BITS_PER_BYTE ? LONG_BITS : PAYLOAD_BITS_PER_BYTE * (leading + 1);

    if (width < LONG_BITS && (bits & (std::uint64_t{1} << (width - 1))) != 0) {
        bits |= (~std::uint64_t{0}) << width;
    }

    return {std::bit_cast<std::int64_t>(bits), size, Status::OK};
}

ReadResult<std::int32_t> readCompactInt(std::span<const std::byte> input) noexcept {
    const auto result = readCompactLong(input);
    const auto lowBits = static_cast<std::uint32_t>(result.value);

    return {std::bit_cast<std::int32_t>(lowBits), result.consumed, result.status};
}

WriteResult writeCompactLong(std::int64_t value, std::span<std::byte> output) noexcept {
    const auto size = compactLongSize(value);

    if (output.size() < size) {
        return {0, Status::INSUFFICIENT_OUTPUT};
    }

    auto bits = static_cast<std::uint64_t>(value);

    for (std::size_t index = size - 1; index > 0; --index) {
        output[index] = static_cast<std::byte>(bits & BYTE_MASK);
        bits >>= BITS_PER_BYTE;
    }

    // The prefix has (size - 1) leading ones followed by zero, except the all-one escape.
    // Remaining low prefix bits hold the highest payload bits; sizes eight/nine have none.
    const auto prefix = size == detail::MAX_COMPACT_LONG_BYTES
                            ? FULL_WIDTH_PREFIX
                            : (BYTE_MASK << (BITS_PER_BYTE - (size - 1))) & BYTE_MASK;
    const auto mask = size - 1 < PAYLOAD_BITS_PER_BYTE ? SINGLE_BYTE_PAYLOAD_MASK >> (size - 1) : 0;
    output[0] = static_cast<std::byte>(prefix | (bits & mask));

    return {size, Status::OK};
}

WriteResult writeCompactInt(std::int32_t value, std::span<std::byte> output) noexcept {
    return writeCompactLong(value, output);
}

} // namespace org::ttldtor::qdcxx::encoding
