// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/ipf/instrument_profile.hpp"
#include <limits>
#include <optional>

namespace org::ttldtor::qdcxx::ipf {
namespace collector_detail {
/** Allocation-free inspection of existing value backing for collector admission. */
struct CollectorProfileAccess {
    /** Read retained UTF-16 units without converting numeric fields.
     * @param profile Stable source value.
     * @return Combined actual retained text/name lengths.
     */
    static std::size_t units(const InstrumentProfile &profile) noexcept {
        return profile.totalUnits_;
    }

    /** Read nonempty custom-field count without allocating a name list.
     * @param profile Stable source value.
     * @return Number of custom map entries.
     */
    static std::size_t fields(const InstrumentProfile &profile) noexcept {
        return profile.custom_.size();
    }

    /** Bound dynamically requested backing of one independent copy on supported STL profiles.
     * Includes a possible empty-map sentinel, custom node/value objects and every string capacity
     * plus terminator and library alignment-request padding. Inline capacity/padding are counted
     * conservatively again, never subtracted.
     * Supported MSVC/GCC tree headers fit four pointers; allocation regressions bind that allowance.
     * @param profile Immutable source value; inspection performs no allocation or conversion.
     * @return Conservative requested copy bytes, or no value on checked arithmetic overflow.
     */
    static std::optional<std::size_t> copyBacking(const InstrumentProfile &profile) noexcept {
        constexpr auto MAXIMUM = std::numeric_limits<std::size_t>::max();
        constexpr std::size_t TREE_HEADER_POINTERS = 4;
        constexpr std::size_t STRING_REQUEST_PADDING =
            64; // Covers qualified manual-alignment prefixes, conservatively also for SSO.
        constexpr auto NODE_BYTES =
            sizeof(InstrumentProfile::CustomValues::value_type) + TREE_HEADER_POINTERS * sizeof(void *);
        std::size_t bytes = NODE_BYTES;
        const auto add = [&](std::size_t amount) {
            if (amount > MAXIMUM - bytes) {
                return false;
            }

            bytes += amount;

            return true;
        };
        const auto text = [&](const std::u16string &value) {
            if (value.capacity() == MAXIMUM) {
                return false;
            }

            const auto units = value.capacity() + 1;

            return units <= MAXIMUM / sizeof(char16_t) && add(units * sizeof(char16_t)) && add(STRING_REQUEST_PADDING);
        };

        for (const auto &value : profile.values_) {
            if (!text(value.text)) {
                return {};
            }
        }

        for (const auto &[name, value] : profile.custom_) {
            if (!add(NODE_BYTES) || !text(name) || !text(value)) {
                return {};
            }
        }

        return bytes;
    }
};
} // namespace collector_detail
} // namespace org::ttldtor::qdcxx::ipf
