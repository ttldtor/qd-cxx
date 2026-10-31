// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <chrono>
#include <optional>
#include <ratio>

namespace org::ttldtor::qdcxx::transport::detail {

/** Form a representable positive monotonic deadline without overflowing duration conversion/addition.
 * Synthetic time points can exercise the same arithmetic without a real sleep or a global test clock.
 * @param now Explicit monotonic observation.
 * @param delay Positive millisecond interval.
 * @return Checked deadline, or empty when the interval/conversion/addition is not representable.
 */
[[nodiscard]] constexpr std::optional<std::chrono::steady_clock::time_point>
deadlineAfter(std::chrono::steady_clock::time_point now, std::chrono::milliseconds delay) noexcept {
    using Clock = std::chrono::steady_clock;
    static_assert(std::ratio_less_equal_v<Clock::period, std::milli>);
    constexpr auto MAX_MILLISECONDS = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::duration::max());

    if (delay <= std::chrono::milliseconds::zero() || delay > MAX_MILLISECONDS) {
        return std::nullopt;
    }

    const auto converted = std::chrono::duration_cast<Clock::duration>(delay);

    if (now > Clock::time_point::max() - converted) {
        return std::nullopt;
    }

    return now + converted;
}

} // namespace org::ttldtor::qdcxx::transport::detail
