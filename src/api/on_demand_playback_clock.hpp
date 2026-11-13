// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>

namespace org::ttldtor::qdcxx::api::detail {

/** Finite replay clock. The worker supplies monotonic samples and validated rates. */
class OnDemandPlaybackClock {
    using Clock = std::chrono::steady_clock;
    std::optional<Clock::time_point> tick_;
    long double positionMs_ = 0;
    double speed_ = 0;

    public:
    void start(Clock::time_point now) noexcept {
        if (!tick_)
            tick_ = now;
    }

    /** Settle elapsed time at the previous rate, then apply the new rate. */
    void advance(double speed, Clock::time_point now, std::int64_t maximumOffsetMs) noexcept {
        if (tick_) {
            const auto elapsed = std::chrono::duration<long double, std::milli>(now - *tick_).count();
            positionMs_ = std::min(static_cast<long double>(maximumOffsetMs), positionMs_ + elapsed * speed_);
            tick_ = now;
        }

        speed_ = speed;
    }

    [[nodiscard]] bool started() const noexcept {
        return tick_.has_value();
    }

    [[nodiscard]] double speed() const noexcept {
        return speed_;
    }

    [[nodiscard]] std::int64_t position() const noexcept {
        return static_cast<std::int64_t>(positionMs_);
    }

    /** Refine an existing bounded worker wake without representing distant low-rate deadlines. */
    [[nodiscard]] Clock::time_point wakeBefore(std::int64_t offsetMs, Clock::time_point now,
                                               Clock::time_point fallback) const noexcept {
        if (!tick_ || speed_ == 0 || fallback <= now)
            return fallback;

        const auto distance = (static_cast<long double>(offsetMs) - positionMs_) / speed_;
        const auto elapsed = std::chrono::duration<long double, std::milli>(now - *tick_).count();
        const auto remaining = distance - elapsed;
        const auto allowance = std::chrono::duration<long double, std::milli>(fallback - now).count();

        if (remaining <= 0 || remaining >= allowance)
            return fallback;

        return now + std::chrono::ceil<Clock::duration>(std::chrono::duration<long double, std::milli>{remaining});
    }
};

} // namespace org::ttldtor::qdcxx::api::detail
