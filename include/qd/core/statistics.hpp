// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

namespace org::ttldtor::qdcxx::core {

namespace detail {
struct DeliveryStatisticsState;
}

/** Availability belongs to each measurement; absent measurements are never an invented zero. */
enum class StatisticAvailability {
    AVAILABLE,   ///< Exact measurement, including an idle zero.
    UNSUPPORTED, ///< No producer for the selected profile.
    NO_SAMPLE,   ///< Supported timing producer has not completed a sample.
    SATURATED    ///< Sticky overflow; value is UINT64_MAX and deltas are unavailable.
};

/** Detached finite unsigned measurement; mutation is serialized by its owning producer. */
struct StatisticCounter {
    std::uint64_t value = 0; ///< Count in the measurement's documented units.
    StatisticAvailability availability = StatisticAvailability::AVAILABLE; ///< Explicit per-field state.

    /** Add actual work without wrapping, allocating or changing an unsupported measurement.
     * @param amount Exact additional work in the same units.
     */
    void add(std::uint64_t amount) noexcept {
        if (availability != StatisticAvailability::AVAILABLE) {
            return;
        }

        if (amount > (std::numeric_limits<std::uint64_t>::max)() - value) {
            value = (std::numeric_limits<std::uint64_t>::max)();
            availability = StatisticAvailability::SATURATED;

            return;
        }

        value += amount;
    }

    /** Fold a retired source with the same supported metric definition.
     * @param other Detached cumulative source measurement.
     */
    void merge(StatisticCounter other) noexcept {
        if (other.availability == StatisticAvailability::UNSUPPORTED ||
            other.availability == StatisticAvailability::NO_SAMPLE) {
            availability = other.availability;

            return;
        }

        add(other.value);

        if (other.availability == StatisticAvailability::SATURATED &&
            availability == StatisticAvailability::AVAILABLE) {
            availability = StatisticAvailability::SATURATED;
        }
    }
};

/** Detached subscription-local lifetime cut; event occurrences are counted before listener fanout. */
struct SubscriptionStatistics {
    std::shared_ptr<const void> ownerIdentity;        ///< Non-reused opaque owner token, retaining no delivery owner.
    std::uint64_t generation = 1;                     ///< Owner lifetime; reconnect does not reset totals.
    std::chrono::steady_clock::time_point observedAt; ///< Last actual local accounting mutation.
    std::optional<std::uint64_t> sourceEpoch; ///< Current established endpoint source ordinal, absent when inactive.
    std::size_t desiredSelectors = 0;         ///< Accepted logical selectors, independent of event-type expansion.
    std::uint64_t desiredRevision = 0;        ///< Accepted facade revision qualifying desiredSelectors.
    std::size_t activeSelectors = 0;          ///< Actually applied logical selectors, never peer acknowledgement.
    std::optional<std::uint64_t> activeDesiredRevision; ///< Logical revision of the complete applied mapping.
    StatisticCounter admittedEvents;                    ///< Accepted subscription delivery occurrences.
    StatisticCounter deliveredEvents;                   ///< Whole selected event-listener dispatch completed normally.
    StatisticCounter droppedEvents;          ///< Known incoming overflow rejection plus admitted overflow victims.
    StatisticCounter rejectedOverflowEvents; ///< Incoming occurrences never admitted.
    StatisticCounter evictedOverflowEvents;  ///< Admitted occurrences retired by overflow.
    StatisticCounter conflatedEvents;        ///< Admitted pending TICKER values superseded by accepted updates.
    StatisticCounter discardedEvents;        ///< Admitted local cancellations, excluding overflow/conflation/failure.
    StatisticCounter callbackFailedEvents;   ///< Events in a batch whose selected user dispatch threw.
    std::size_t pendingEvents = 0;           ///< Retained source-view and delivery-FIFO occurrences.
    std::size_t inFlightEvents = 0;          ///< Selected events until the complete adapter invocation retires.
    std::size_t queueCapacityEvents = 0;     ///< Effective reserved delivery slots, including TICKER view slots.
    StatisticCounter callbackCalls;          ///< Actual user event-listener invocations, including throws.
    StatisticCounter callbackFailures;       ///< Actual throwing event-listener invocations.
    StatisticCounter callbackDurationTotal{0, StatisticAvailability::NO_SAMPLE}; ///< Completed invocation nanoseconds.
    StatisticCounter callbackDurationMaximum{0, StatisticAvailability::NO_SAMPLE}; ///< Maximum completed nanoseconds.
    StatisticCounter callbackDurationSamples; ///< Completed normal or throwing user invocations.
    bool final = false; ///< All native, selected and observer/callback captures have actually retired.
};

} // namespace org::ttldtor::qdcxx::core
