// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstdint>
#include <limits>

namespace org::ttldtor::qdcxx::core {

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

        if (amount > std::numeric_limits<std::uint64_t>::max() - value) {
            value = std::numeric_limits<std::uint64_t>::max();
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

} // namespace org::ttldtor::qdcxx::core
