// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/protocol/live_session.hpp"
#include "qd/transport/aggregation.hpp"
#include <array>
#include <optional>

namespace org::ttldtor::qdcxx::transport::detail {
/** Fixed canonical request text; empty encodes reset, independently of a peer result. */
struct AggregationRequestText {
    std::array<char16_t, 32> units{}; ///< Fixed request-only storage; no heap allocation or borrowed input.
    std::size_t size = 0;             ///< Written UTF-16 units, excluding any terminator.

    /** @return Borrowed canonical text while this fixed value is retained. */
    std::u16string_view view() const noexcept {
        return {units.data(), size};
    }
};

/** @param period Validated optional local request. @return Java-compatible canonical bounded TimePeriod text. */
[[nodiscard]] AggregationRequestText aggregationRequestText(std::optional<std::chrono::milliseconds> period) noexcept;
/** @param text Borrowed peer TimePeriodInfo text.
 * @param work Actual mutable parse visits/text/bytes; exhaustion conservatively yields unavailable.
 * @param complete Optional complete-inspection flag; false after work refusal; bounded-profile rejection is complete
 * UNKNOWN.
 * @return Exact known range or UNKNOWN for invalid/unavailable/out-of-profile text; never a request echo.
 */
[[nodiscard]] std::optional<AggregationPeriodInfo>
parseAggregationPeriodInfo(std::u16string_view text, protocol::LiveWork &work, bool *complete = nullptr) noexcept;
/** @param descriptor Actual accumulated peer descriptor.
 * @param families Configured TICKER/STREAM/HISTORY family presence; does not add unsupported families.
 * @param work Actual inspection/parse credits.
 * @param complete Optional flag separating insufficient work from a complete UNKNOWN report.
 * @return Union of known peer-reported ranges for applicable matching SEND DATA messages.
 */
[[nodiscard]] std::optional<AggregationPeriodInfo> peerAggregationPeriodInfo(protocol::LiveProtocolView descriptor,
                                                                             std::array<bool, 3> families,
                                                                             protocol::LiveWork &work,
                                                                             bool *complete = nullptr) noexcept;
} // namespace org::ttldtor::qdcxx::transport::detail
