// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/event_flags.hpp"

#include <bit>
#include <limits>
#include <type_traits>

namespace org::ttldtor::qdcxx::events {
namespace {
/** @tparam Signed Signed32 or signed64 word.
 * @param flags Raw word.
 * @param shift Arbitrary Java shift distance.
 * @return Unsigned representation of Java arithmetic-right-shifted flags.
 */
template <typename Signed> std::make_unsigned_t<Signed> shiftedBits(Signed flags, std::int32_t shift) noexcept {
    using Unsigned = std::make_unsigned_t<Signed>;
    constexpr auto SHIFT_MASK = std::numeric_limits<Unsigned>::digits - 1;
    const auto distance = static_cast<unsigned>(shift) & SHIFT_MASK;
    auto result = static_cast<Unsigned>(flags) >> distance;

    if (flags < 0 && distance != 0) {
        result |= ~(std::numeric_limits<Unsigned>::max() >> distance);
    }

    return result;
}

/** @tparam Signed Signed32 or signed64 word.
 * @param flags Original raw word.
 * @param mask Unshifted field mask.
 * @param shift Arbitrary Java shift distance.
 * @param bits Field replacement bits.
 * @return New signed raw word with unrelated bits preserved.
 */
template <typename Signed> Signed replaceBits(Signed flags, Signed mask, std::int32_t shift, Signed bits) noexcept {
    using Unsigned = std::make_unsigned_t<Signed>;
    constexpr auto SHIFT_MASK = std::numeric_limits<Unsigned>::digits - 1;
    const auto distance = static_cast<unsigned>(shift) & SHIFT_MASK;
    const auto field = static_cast<Unsigned>(mask) << distance;
    const auto replacement = (static_cast<Unsigned>(bits) & static_cast<Unsigned>(mask)) << distance;

    return std::bit_cast<Signed>((static_cast<Unsigned>(flags) & ~field) | replacement);
}
} // namespace

std::int32_t getBits(std::int32_t flags, std::int32_t mask, std::int32_t shift) noexcept {
    return std::bit_cast<std::int32_t>(shiftedBits(flags, shift) & static_cast<std::uint32_t>(mask));
}

std::int64_t getBits(std::int64_t flags, std::int64_t mask, std::int32_t shift) noexcept {
    return std::bit_cast<std::int64_t>(shiftedBits(flags, shift) & static_cast<std::uint64_t>(mask));
}

std::int32_t setBits(std::int32_t flags, std::int32_t mask, std::int32_t shift, std::int32_t bits) noexcept {
    return replaceBits(flags, mask, shift, bits);
}

std::int64_t setBits(std::int64_t flags, std::int64_t mask, std::int32_t shift, std::int64_t bits) noexcept {
    return replaceBits(flags, mask, shift, bits);
}
} // namespace org::ttldtor::qdcxx::events
