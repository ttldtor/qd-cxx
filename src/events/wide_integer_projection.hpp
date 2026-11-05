// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <cstdint>
#include <limits>

namespace org::ttldtor::qdcxx::events::detail {
/** Exact WideDecimal.toLong path before any caller-specific integer narrowing.
 * @param raw Complete source wide bits; every rank and significand is accepted.
 * @return Nearest integer with half ties toward positive infinity, sign saturation and nonfinite defaults.
 */
inline std::int64_t projectWideInteger(std::int64_t raw) noexcept {
    constexpr int INTEGER_RANK = 128;
    constexpr int MAX_EXACT_POWER = 18;
    const auto significand = raw >> 8;
    const auto rank = static_cast<int>(static_cast<std::uint64_t>(raw) & 0xff);
    const auto saturated = significand > 0   ? std::numeric_limits<std::int64_t>::max()
                           : significand < 0 ? std::numeric_limits<std::int64_t>::min()
                                             : std::int64_t{0};

    if (!rank) {
        return saturated;
    }

    if (rank == INTEGER_RANK) {
        return significand;
    }

    const auto power = rank > INTEGER_RANK ? rank - INTEGER_RANK : INTEGER_RANK - rank;

    if (power > MAX_EXACT_POWER) {
        return rank > INTEGER_RANK ? 0 : saturated;
    }

    std::int64_t factor = 1;

    for (int index = 0; index < power; ++index) {
        factor *= 10;
    }

    if (rank > INTEGER_RANK) {
        const auto half = factor / 2;

        return significand >= 0 ? (significand / half + 1) >> 1 : ((significand + 1) / half) >> 1;
    }

    if (significand > std::numeric_limits<std::int64_t>::max() / factor ||
        significand < std::numeric_limits<std::int64_t>::min() / factor) {
        return saturated;
    }

    return significand * factor;
}

} // namespace org::ttldtor::qdcxx::events::detail
