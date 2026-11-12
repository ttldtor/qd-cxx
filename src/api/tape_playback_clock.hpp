// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../replay/relative_time.hpp"
#include "qd/api/feed.hpp"

#include <algorithm>

namespace org::ttldtor::qdcxx::api::detail {
inline constexpr std::uint32_t NORMAL_TAPE_SPEED_PERCENT = 100;
inline constexpr std::uint32_t MAXIMUM_TAPE_SPEED_PERCENT = 10'000;
inline constexpr auto MAXIMUM_TAPE_SOURCE_SPAN = std::chrono::days{365};

/** Validate relative options before publishing a source. Immediate mode ignores speed and span. */
constexpr bool validTapePlayback(TapeSourceOptions options) noexcept {
    return options.playback == TapePlaybackPolicy::IMMEDIATE ||
           (options.playback == TapePlaybackPolicy::SOURCE_TIME && options.speedPercent != 0 &&
            options.speedPercent <= MAXIMUM_TAPE_SPEED_PERCENT && options.maximumSourceSpan.count() > 0 &&
            options.maximumSourceSpan <= MAXIMUM_TAPE_SOURCE_SPAN);
}

/** Owner-thread scheduling state. The caller supplies monotonic samples and handles waits and cancellation. */
class TapePlaybackClock {
    using Clock = std::chrono::steady_clock;
    TapeSourceOptions options_;
    std::optional<std::int64_t> firstTimestamp_;
    std::optional<Clock::time_point> origin_;
    std::optional<Clock::time_point> deadline_;
    std::optional<Clock::time_point> pausedAt_;
    std::uint64_t elapsedHighWaterMs_ = 0;

    public:
    explicit TapePlaybackClock(TapeSourceOptions options) noexcept : options_(options) {
    }

    [[nodiscard]] bool paced() const noexcept {
        return options_.playback == TapePlaybackPolicy::SOURCE_TIME;
    }

    /** Plan one complete eligible envelope. An absent timestamp creates no wait or timing origin. */
    [[nodiscard]] bool schedule(std::optional<std::int64_t> timestamp, Clock::time_point now) noexcept {
        deadline_.reset();

        if (!paced() || !timestamp) {
            return true;
        }

        if (!firstTimestamp_) {
            firstTimestamp_ = timestamp;
            origin_ = now;

            return true;
        }

        const auto elapsed = replay::detail::positiveElapsedMs(*timestamp, *firstTimestamp_);

        if (elapsed > static_cast<std::uint64_t>(options_.maximumSourceSpan.count())) {
            return false;
        }

        const auto nextHighWater = std::max(elapsed, elapsedHighWaterMs_);
        const auto speed = options_.speedPercent;
        const auto delay = std::chrono::milliseconds((nextHighWater * NORMAL_TAPE_SPEED_PERCENT + speed - 1) / speed);
        const auto ticks = std::chrono::duration_cast<Clock::duration>(delay);

        if (*origin_ > Clock::time_point::max() - ticks) {
            return false;
        }

        elapsedHighWaterMs_ = nextHighWater;
        deadline_ = *origin_ + ticks;

        return true;
    }

    /** Apply the latest coalesced pause intent without charging time spent paused. */
    [[nodiscard]] bool setPaused(bool paused, Clock::time_point now) noexcept {
        if (paused == pausedAt_.has_value()) {
            return true;
        }

        if (paused) {
            pausedAt_ = now;

            return true;
        }

        const auto duration = now - *pausedAt_;
        const auto latest = Clock::time_point::max() - duration;

        if ((origin_ && *origin_ > latest) || (deadline_ && *deadline_ > latest)) {
            return false;
        }

        if (origin_) {
            *origin_ += duration;
        }

        if (deadline_) {
            *deadline_ += duration;
        }

        pausedAt_.reset();

        return true;
    }

    /** @return Whether intake may proceed at the supplied monotonic sample. */
    [[nodiscard]] bool ready(Clock::time_point now) const noexcept {
        return !pausedAt_ && (!deadline_ || now >= *deadline_);
    }

    /** @return Actual next scheduled delivery, if currently waiting for source time. */
    [[nodiscard]] std::optional<Clock::time_point> deadline() const noexcept {
        return pausedAt_ ? std::nullopt : deadline_;
    }

    /** Retire the deadline of the envelope handed to the queue admission path. */
    void delivered() noexcept {
        deadline_.reset();
    }
};
} // namespace org::ttldtor::qdcxx::api::detail
