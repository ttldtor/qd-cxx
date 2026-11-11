// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace org::ttldtor::qdcxx::core {
/** Finite local delivery identities, never wire fields or event capabilities. */
inline constexpr std::size_t MAX_DELIVERY_PROJECTIONS = 5;
using DeliveryProjectionMask = std::uint32_t;
inline constexpr DeliveryProjectionMask NATIVE_DELIVERY_PROJECTION = 1;
inline constexpr DeliveryProjectionMask ALL_DELIVERY_PROJECTIONS = (1U << MAX_DELIVERY_PROJECTIONS) - 1;

/** @param mask Explicit requested local identities.
 * @return Whether this nonempty mask fits the immutable local delivery envelope.
 */
[[nodiscard]] constexpr bool validDeliveryProjectionMask(DeliveryProjectionMask mask) noexcept {
    return mask != 0 && (mask & ~ALL_DELIVERY_PROJECTIONS) == 0;
}

/** Owning original eligibility for each independently selected projection, without payload ownership.
 * @tparam Membership Fixed detached core or authoritative activation token.
 */
template <class Membership> struct CapturedDeliveryProjections {
    Membership physical{};           ///< Original coarse physical choice, independent of per-bit lineage.
    DeliveryProjectionMask mask = 0; ///< Original or remaining local identities, never recomputed from newer desire.
    std::array<Membership, MAX_DELIVERY_PROJECTIONS> memberships{}; ///< Original choices specific to each bit.

    /** @return Business occurrence count; listener fanout does not affect this value. */
    [[nodiscard]] std::size_t count() const noexcept {
        return std::popcount(mask);
    }
};
} // namespace org::ttldtor::qdcxx::core
