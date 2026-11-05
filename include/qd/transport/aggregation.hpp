// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <chrono>

namespace org::ttldtor::qdcxx::transport {
/** Maximum accepted local server-aggregation request; peer-reported ranges have their own domain. */
inline constexpr std::chrono::milliseconds MAXIMUM_REQUESTED_AGGREGATION_PERIOD{86400000};

/** Owning peer-reported period range. An absent optional value represents UNKNOWN, including legacy peers. */
struct AggregationPeriodInfo {
    std::chrono::milliseconds minimum{0}; ///< Minimum period actually reported, in milliseconds; zero is known zero.
    std::chrono::milliseconds maximum{0}; ///< Maximum reported period, at least minimum; no local request inference.
    /** Compare reported values, independently of request or source identity.
     * @param other Owning peer range.
     * @return Equality of both millisecond bounds.
     */
    bool operator==(const AggregationPeriodInfo &other) const noexcept = default;
};
} // namespace org::ttldtor::qdcxx::transport
