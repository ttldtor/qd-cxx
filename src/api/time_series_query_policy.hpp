// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/time_helpers.hpp"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::api::detail {
/** Inclusive business-millisecond query and a finite covering interval of raw time-sequence indices. */
struct TimeSeriesQueryRange {
    std::int64_t fromTime;             ///< Inclusive requested business milliseconds.
    std::int64_t toTime;               ///< Inclusive requested business milliseconds.
    std::int64_t fromIndex;            ///< Inclusive lower raw candidate, meaningful when nonempty.
    std::int64_t toIndex;              ///< Inclusive upper raw candidate, meaningful when nonempty.
    std::int64_t subscriptionFromTime; ///< Safe temporary subscription lower time, meaningful when nonempty.
    bool empty;                        ///< Reversed range or no representable matching event.

    /** @param index Raw signed seconds and millisecond/sequence event index.
     * @return Whether the decoded business time belongs to the original inclusive interval.
     */
    [[nodiscard]] bool contains(std::int64_t index) const noexcept {
        const auto time = events::getTimeFromTimeSequence(index);

        return !empty && fromTime <= time && time <= toTime;
    }
};

/** Compute the smallest contiguous raw envelope that covers every representable business time.
 * The ten-bit millisecond field admits 1000..1023: those values overlap the following second.
 * The raw envelope may therefore contain false positives; callers must use contains before applying
 * the result-count limit. The provided subscriptionFromTime encodes to the beginning of the raw
 * lower second so an ordinary time-series selector covers the complete raw envelope.
 * Reversed or wholly unrepresentable ranges are empty.
 * @param fromTime Inclusive signed Unix milliseconds.
 * @param toTime Inclusive signed Unix milliseconds, including INT64_MAX as an unbounded upper end.
 * @return Non-owning scalar query policy without allocation or arithmetic overflow.
 */
[[nodiscard]] inline TimeSeriesQueryRange makeTimeSeriesQueryRange(std::int64_t fromTime,
                                                                   std::int64_t toTime) noexcept {
    constexpr std::int64_t PER_SECOND = 1000;
    constexpr std::int64_t MAX_PACKED_MILLIS = 1023;
    constexpr std::int64_t OVERLAP_MILLIS = MAX_PACKED_MILLIS - PER_SECOND;
    constexpr std::int64_t MIN_SECONDS = std::numeric_limits<std::int32_t>::min();
    constexpr std::int64_t MAX_SECONDS = std::numeric_limits<std::int32_t>::max();
    constexpr std::int64_t MIN_TIME = MIN_SECONDS * PER_SECOND;
    constexpr std::int64_t MAX_TIME = MAX_SECONDS * PER_SECOND + MAX_PACKED_MILLIS;
    TimeSeriesQueryRange range{fromTime, toTime, 0, 0, 0, true};

    if (fromTime > toTime || toTime < MIN_TIME || fromTime > MAX_TIME)
        return range;

    const auto lower = std::max(fromTime, MIN_TIME);
    const auto upper = std::min(toTime, MAX_TIME);
    const auto floorSeconds = [](std::int64_t value) {
        return value / PER_SECOND - (value % PER_SECOND < 0 ? 1 : 0);
    };
    auto firstSeconds = floorSeconds(lower);
    auto firstMillis = lower - firstSeconds * PER_SECOND;

    if (firstMillis <= OVERLAP_MILLIS && firstSeconds > MIN_SECONDS) {
        --firstSeconds;
        firstMillis += PER_SECOND;
    }

    auto lastSeconds = std::min(floorSeconds(upper), MAX_SECONDS);
    const auto lastMillis = upper - lastSeconds * PER_SECOND;
    const auto pack = [](std::int64_t seconds, std::int64_t millis, std::uint32_t sequence) {
        const auto bits = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(seconds)) << 32) |
                          (static_cast<std::uint64_t>(millis) << 22) | sequence;

        return std::bit_cast<std::int64_t>(bits);
    };
    range.subscriptionFromTime = firstSeconds * PER_SECOND;
    range.fromIndex = pack(firstSeconds, firstMillis, 0);
    range.toIndex = pack(lastSeconds, lastMillis, events::MAX_EVENT_SEQUENCE);
    range.empty = false;

    return range;
}

/** Finite independent output preparation credits. */
struct TimeSeriesQueryWork {
    std::size_t visits;      ///< Maximum candidate rows examined, including rejected rows.
    std::size_t comparisons; ///< Maximum ordering comparisons, including attempted exhaustion.
};
/** Atomic refusal for a detached result. */
enum class TimeSeriesQueryError {
    RESULT_LIMIT, ///< More matching rows than the configured complete output limit.
    WORK_LIMIT    ///< Candidate scan or sorting comparison budget was exhausted.
};

namespace time_series_query_detail {
struct WorkExhausted {};
} // namespace time_series_query_detail

/** Filter owned raw candidates, enforce the result limit and return deterministic chronological order.
 * Candidate scan is separately finite. Input ownership is consumed, including on refusal; no partial output
 * escapes. Sorting and truncation never change the retained collector. Event flags are cleared on success.
 * @tparam Event Owning event with no-throw move.
 * @tparam Index Callable returning the signed raw index without retaining its input.
 * @tparam Time Callable returning business milliseconds without retaining its input.
 * @tparam Clear Callable clearing event flags without retaining its input.
 * @param candidates Independently owned candidate values, consumed by this call.
 * @param range Original business interval and its raw envelope.
 * @param maximumResults Maximum selected output count, including zero.
 * @param work Mutable finite visit and comparison credits; attempted work is not refunded.
 * @param index Raw-index projection, valid for every candidate.
 * @param time Business-time projection, valid for every retained candidate.
 * @param clearFlags Event-flag normalization, valid for every retained candidate.
 * @return Complete independent ordered values or a typed resource/work refusal.
 */
template <class Event, class Index, class Time, class Clear>
[[nodiscard]] std::expected<std::vector<Event>, TimeSeriesQueryError>
selectTimeSeriesSnapshot(std::vector<Event> &&candidates, const TimeSeriesQueryRange &range, std::size_t maximumResults,
                         TimeSeriesQueryWork &work, Index index, Time time, Clear clearFlags) {
    static_assert(std::is_nothrow_move_constructible_v<Event> && std::is_nothrow_move_assignable_v<Event>);
    auto result = std::move(candidates);

    if (result.size() > work.visits) {
        work.visits = 0;

        return std::unexpected(TimeSeriesQueryError::WORK_LIMIT);
    }

    work.visits -= result.size();
    const auto end = std::remove_if(result.begin(), result.end(), [&](Event &event) {
        if (!range.contains(index(event)))
            return true;

        clearFlags(event);

        return false;
    });
    result.erase(end, result.end());

    if (result.size() > maximumResults)
        return std::unexpected(TimeSeriesQueryError::RESULT_LIMIT);

    try {
        std::sort(result.begin(), result.end(), [&](const Event &left, const Event &right) {
            if (!work.comparisons)
                throw time_series_query_detail::WorkExhausted{};

            --work.comparisons;

            if (time(left) != time(right))
                return time(left) < time(right);

            return index(left) < index(right);
        });
    } catch (const time_series_query_detail::WorkExhausted &) {
        return std::unexpected(TimeSeriesQueryError::WORK_LIMIT);
    }

    return result;
}

/** Typed convenience overload with the standard time-series event accessors.
 * @tparam Event Owning event with no-throw move, getIndex(), getTime() and writable eventFlags.
 * @param candidates Independently owned candidate values, consumed by this call.
 * @param range Original business interval and its raw envelope.
 * @param maximumResults Maximum selected output count, including zero.
 * @param work Mutable finite visit and comparison credits; attempted work is not refunded.
 * @return Complete independent ordered values or a typed resource/work refusal.
 */
template <class Event>
[[nodiscard]] std::expected<std::vector<Event>, TimeSeriesQueryError>
selectTimeSeriesSnapshot(std::vector<Event> &&candidates, const TimeSeriesQueryRange &range, std::size_t maximumResults,
                         TimeSeriesQueryWork &work) {
    return selectTimeSeriesSnapshot(
        std::move(candidates), range, maximumResults, work,
        [](const Event &event) {
            return event.getIndex();
        },
        [](const Event &event) {
            return event.getTime();
        },
        [](Event &event) {
            event.eventFlags = 0;
        });
}
} // namespace org::ttldtor::qdcxx::api::detail
