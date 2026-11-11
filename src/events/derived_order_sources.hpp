// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/core/delivery_projection.hpp"
#include "qd/events/derived_order.hpp"
#include "qd/events/order_source.hpp"

namespace org::ttldtor::qdcxx::events::detail {
/** Exact local bit identities shared by source selection and owning group projection. */
inline constexpr core::DeliveryProjectionMask DERIVED_BID = 1U << 1;
inline constexpr core::DeliveryProjectionMask DERIVED_ASK = 1U << 2;
inline constexpr core::DeliveryProjectionMask DERIVED_UNITARY_BID = 1U << 3;
inline constexpr core::DeliveryProjectionMask DERIVED_UNITARY_ASK = 1U << 4;
inline constexpr core::DeliveryProjectionMask DERIVED_SEPARATE_PAIR = DERIVED_BID | DERIVED_ASK;
inline constexpr core::DeliveryProjectionMask DERIVED_UNITARY_PAIR = DERIVED_UNITARY_BID | DERIVED_UNITARY_ASK;
static_assert(static_cast<std::size_t>(DerivedOrderRole::UNITARY_ASK) + 2 == core::MAX_DELIVERY_PROJECTIONS);

/** Actual production source identities, independent of the ordinary source-high32 Order layout. */
struct DerivedOrderSourceIds {
    std::int32_t bid;     ///< Separate bid source.
    std::int32_t ask;     ///< Separate ask source.
    std::int32_t unitary; ///< Shared source for the indivisible bid/ask pair.
};

/** Inspect the finite actual source family without preparing or allocating another mapper.
 * @param kind Verified supported physical family, established by route preparation.
 * @param exchange Actual Quote record suffix, zero for composite/MM.
 * @return Complete known bid/ask/unitary IDs; no input storage is retained.
 */
inline DerivedOrderSourceIds derivedOrderSourceIds(DerivedOrderKind kind, char16_t exchange) noexcept {
    if (kind == DerivedOrderKind::MARKET_MAKER) {
        return {OrderSource::AGGREGATE_BID.id(), OrderSource::AGGREGATE_ASK.id(), OrderSource::AGGREGATE.id()};
    }

    return exchange ? DerivedOrderSourceIds{OrderSource::REGIONAL_BID.id(), OrderSource::REGIONAL_ASK.id(),
                                            OrderSource::REGIONAL.id()}
                    : DerivedOrderSourceIds{OrderSource::COMPOSITE_BID.id(), OrderSource::COMPOSITE_ASK.id(),
                                            OrderSource::COMPOSITE.id()};
}
} // namespace org::ttldtor::qdcxx::events::detail
