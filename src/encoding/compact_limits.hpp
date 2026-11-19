// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace org::ttldtor::qdcxx::encoding::detail {

/// One full-width escape prefix plus eight payload octets; see spec/encoding/PRIMITIVES.md.
inline constexpr std::size_t MAX_COMPACT_LONG_BYTES = 9;
/// One prefix bit per byte leaves seven payload bits per width increment.
inline constexpr unsigned int PAYLOAD_BITS_PER_BYTE = 7;

/** Choose the shortest compact representation of a signed long.
 * @param value Signed value to encode.
 * @return Encoded byte count, including the prefix byte.
 */
[[nodiscard]] inline std::size_t compactLongSize(std::int64_t value) noexcept {
    for (std::size_t size = 1; size < MAX_COMPACT_LONG_BYTES; ++size) {
        // One payload bit carries the sign, giving the interval [-2^(width-1), 2^(width-1)).
        const auto limit = std::int64_t{1} << (PAYLOAD_BITS_PER_BYTE * size - 1);

        if (value >= -limit && value < limit) {
            return size;
        }
    }

    return MAX_COMPACT_LONG_BYTES;
}

/** Write a compact long into an exact-sized output range.
 * @param value Signed value to encode.
 * @param output Writable bytes whose count is compactLongSize(value).
 */
void writeCompactLongExact(std::int64_t value, std::span<std::byte> output) noexcept;

} // namespace org::ttldtor::qdcxx::encoding::detail
