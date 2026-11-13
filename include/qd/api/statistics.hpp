// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace org::ttldtor::qdcxx::api {

/** A reader-owned detached baseline; modifying the same instance concurrently requires caller synchronization.
 * @tparam Snapshot EndpointStatistics or SubscriptionStatistics detached value type.
 */
template <class Snapshot> class StatisticsBaseline {
    Snapshot value_;

    public:
    /** Retain a detached observation without retaining its runtime owner.
     * @param value Snapshot used as the initial baseline.
     */
    explicit StatisticsBaseline(Snapshot value) noexcept : value_(std::move(value)) {
    }

    /** Replace this reader's baseline; no producer or other reader is modified.
     * @param value New detached observation.
     */
    void reset(Snapshot value) noexcept {
        value_ = std::move(value);
    }

    /** Access the retained detached snapshot.
     * @return Reference valid until this baseline is reset or destroyed.
     */
    [[nodiscard]] const Snapshot &value() const noexcept {
        return value_;
    }
};

/** A rate derived from lifetime totals; its interval may include reconnects. */
struct LifetimeStatisticRate {
    double perSecond; ///< Items or bytes per second in the selected counter's units.
};

namespace detail {
/** Check that both cuts identify the same owner and observation generation.
 * @tparam Snapshot Detached snapshot type with ownerIdentity and generation fields.
 * @param first Earlier cut.
 * @param last Later cut.
 * @return Whether both non-null identities and generations match.
 */
template <class Snapshot> bool sameStatisticsOwner(const Snapshot &first, const Snapshot &last) noexcept {
    return first.ownerIdentity && first.ownerIdentity == last.ownerIdentity && first.generation == last.generation;
}

/** Compute an exact unsigned delta only from available, ordered counters.
 * @param first Earlier counter.
 * @param last Later counter.
 * @return Delta or no value if unavailable, saturated or decreasing.
 */
inline std::optional<std::uint64_t> statisticDelta(core::StatisticCounter first, core::StatisticCounter last) noexcept {
    if (first.availability != core::StatisticAvailability::AVAILABLE ||
        last.availability != core::StatisticAvailability::AVAILABLE || last.value < first.value) {
        return std::nullopt;
    }

    return last.value - first.value;
}

/** Calculate a finite rate without integer scaling or time-point subtraction overflow.
 * @param first Earlier publication time.
 * @param last Later publication time.
 * @param delta Exact count delta.
 * @return Rounded double count per second, or no value for nonpositive time or nonfinite conversion.
 */
inline std::optional<LifetimeStatisticRate> lifetimeRate(std::chrono::steady_clock::time_point first,
                                                         std::chrono::steady_clock::time_point last,
                                                         std::uint64_t delta) noexcept {
    if (last <= first) {
        return std::nullopt;
    }

    using Duration = std::chrono::steady_clock::duration;
    using UnsignedRep = std::make_unsigned_t<Duration::rep>;
    // Unsigned subtraction preserves the positive distance even across the signed origin.
    const UnsignedRep ticks = static_cast<UnsignedRep>(last.time_since_epoch().count()) -
                              static_cast<UnsignedRep>(first.time_since_epoch().count());
    const auto elapsed = static_cast<long double>(ticks) * Duration::period::num / Duration::period::den;

    if (!(elapsed > 0)) {
        return std::nullopt;
    }

    const auto rate = static_cast<long double>(delta) / elapsed;

    if (!std::isfinite(rate) || rate > (std::numeric_limits<double>::max)()) {
        return std::nullopt;
    }

    return LifetimeStatisticRate{static_cast<double>(rate)};
}
} // namespace detail

/** Calculate a lifetime transport count rate between two coherent cuts.
 * @param first Earlier detached endpoint cut.
 * @param last Later detached endpoint cut.
 * @param metric Cumulative connection counter selected identically in both cuts; null is unavailable.
 * @return Lifetime rate, potentially crossing reconnect, or no value for incompatible or unavailable inputs.
 * A writtenBytes rate is unavailable after unknown write acceptance. The result does not drive network progress.
 */
[[nodiscard]] inline std::optional<LifetimeStatisticRate>
statisticsLifetimeRate(const EndpointStatistics &first, const EndpointStatistics &last,
                       core::StatisticCounter transport::ConnectionStatistics::*metric) noexcept {
    if (!metric || !detail::sameStatisticsOwner(first, last) ||
        (metric == &transport::ConnectionStatistics::writtenBytes &&
         (first.connection.writeAcceptanceUnknown || last.connection.writeAcceptanceUnknown))) {
        return std::nullopt;
    }

    const auto delta = detail::statisticDelta(first.connection.*metric, last.connection.*metric);

    if (!delta) {
        return std::nullopt;
    }

    return detail::lifetimeRate(first.observedAt, last.observedAt, *delta);
}

/** Calculate a lifetime subscription count rate between two coherent cuts.
 * @param first Earlier detached subscription cut.
 * @param last Later detached subscription cut.
 * @param metric Cumulative subscription counter selected identically in both cuts; null or duration fields are
 * unavailable.
 * @return Lifetime rate, potentially crossing reconnect, or no value for incompatible or unavailable inputs.
 */
[[nodiscard]] inline std::optional<LifetimeStatisticRate>
statisticsLifetimeRate(const SubscriptionStatistics &first, const SubscriptionStatistics &last,
                       core::StatisticCounter SubscriptionStatistics::*metric) noexcept {
    if (!metric || metric == &SubscriptionStatistics::callbackDurationTotal ||
        metric == &SubscriptionStatistics::callbackDurationMaximum || !detail::sameStatisticsOwner(first, last)) {
        return std::nullopt;
    }

    const auto delta = detail::statisticDelta(first.*metric, last.*metric);

    if (!delta) {
        return std::nullopt;
    }

    return detail::lifetimeRate(first.observedAt, last.observedAt, *delta);
}

/** Compute the interval-weighted mean completed callback duration.
 * @param first Earlier detached subscription cut.
 * @param last Later detached subscription cut.
 * @return Mean nanoseconds as a rounded double, or no value for incompatible, missing, saturated or zero-weight cuts.
 */
[[nodiscard]] inline std::optional<double> statisticsCallbackMean(const SubscriptionStatistics &first,
                                                                  const SubscriptionStatistics &last) noexcept {
    if (!detail::sameStatisticsOwner(first, last) || last.observedAt <= first.observedAt) {
        return std::nullopt;
    }

    auto initialTotal = first.callbackDurationTotal;

    if (initialTotal.availability == core::StatisticAvailability::NO_SAMPLE && initialTotal.value == 0 &&
        first.callbackDurationSamples.availability == core::StatisticAvailability::AVAILABLE &&
        first.callbackDurationSamples.value == 0) {
        initialTotal.availability = core::StatisticAvailability::AVAILABLE;
    }

    const auto total = detail::statisticDelta(initialTotal, last.callbackDurationTotal);
    const auto samples = detail::statisticDelta(first.callbackDurationSamples, last.callbackDurationSamples);

    if (!total || !samples || *samples == 0) {
        return std::nullopt;
    }

    return static_cast<double>(static_cast<long double>(*total) / static_cast<long double>(*samples));
}

/** An interval rate qualified to one established source epoch. */
struct EpochStatisticRate {
    double perSecond;          ///< Items or bytes per second in the selected counter's units.
    std::uint64_t sourceEpoch; ///< Positive, non-saturated established source ordinal.
};

/** Calculate a transport rate for one qualified established epoch.
 * @param first Earlier endpoint cut.
 * @param last Later endpoint cut.
 * @param metric Cumulative counter selected identically in both cuts.
 * @return Epoch-labelled rate or no value when epoch identity or rate inputs are unavailable.
 */
[[nodiscard]] inline std::optional<EpochStatisticRate>
statisticsEpochRate(const EndpointStatistics &first, const EndpointStatistics &last,
                    core::StatisticCounter transport::ConnectionStatistics::*metric) noexcept {
    const auto epoch = first.connection.sourceEpoch;

    if (!epoch || *epoch == 0 || *epoch == (std::numeric_limits<std::uint64_t>::max)() ||
        epoch != last.connection.sourceEpoch ||
        first.connection.establishedConnections.availability != core::StatisticAvailability::AVAILABLE ||
        last.connection.establishedConnections.availability != core::StatisticAvailability::AVAILABLE ||
        first.connection.establishedConnections.value != *epoch ||
        last.connection.establishedConnections.value != *epoch) {
        return std::nullopt;
    }

    const auto rate = statisticsLifetimeRate(first, last, metric);

    if (!rate) {
        return std::nullopt;
    }

    return EpochStatisticRate{rate->perSecond, *epoch};
}

/** Calculate a subscription rate for one qualified established epoch.
 * @param first Earlier subscription cut.
 * @param last Later subscription cut.
 * @param metric Cumulative counter selected identically in both cuts.
 * @return Epoch-labelled rate or no value when epoch identity or rate inputs are unavailable.
 */
[[nodiscard]] inline std::optional<EpochStatisticRate>
statisticsEpochRate(const SubscriptionStatistics &first, const SubscriptionStatistics &last,
                    core::StatisticCounter SubscriptionStatistics::*metric) noexcept {
    const auto epoch = first.sourceEpoch;

    if (!epoch || *epoch == 0 || *epoch == (std::numeric_limits<std::uint64_t>::max)() || epoch != last.sourceEpoch) {
        return std::nullopt;
    }

    const auto rate = statisticsLifetimeRate(first, last, metric);

    if (!rate) {
        return std::nullopt;
    }

    return EpochStatisticRate{rate->perSecond, *epoch};
}
} // namespace org::ttldtor::qdcxx::api
