// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "../core/wide_decimal_representation.hpp"
#include "qd/core/serial_field_type.hpp"
#include "qd/core/wide_decimal.hpp"
#include "qd/events/event_mapping.hpp"

#include <array>
#include <bit>
#include <limits>

namespace org::ttldtor::qdcxx::events::detail {

/** Select a source business snapshot only while the associated public raw decimal is unchanged.
 * @tparam Count Fixed number of numeric fields, four for Quote and ten for Profile.
 * @param value Current public decimal value, including manual raw edits.
 * @param snapshots Optional inline owning projection state; no source data is borrowed.
 * @param index Valid constant numeric property index smaller than Count.
 * @return Source business double or the current WideDecimal view for manual/modified fields; no allocation.
 */
template <std::size_t Count>
inline double businessValue(core::WideDecimal value,
                            const std::optional<std::array<BusinessNumericSnapshot, Count>> &snapshots,
                            std::size_t index) noexcept {
    if (snapshots && (*snapshots)[index].wideRaw == value.raw) {
        return (*snapshots)[index].directValue;
    }

    return value.toDouble();
}

/** Convert a validated numeric scalar through its actual Java business-double representation.
 * @param type Exact type from NUMERIC_TYPES, selected before projection.
 * @param raw Complete source bits, before any LONG-to-wide decimal composition.
 * @return Binary64 business value; NaNs may use the native canonical quiet payload.
 */
inline double projectPresentBusinessDouble(core::SerialFieldType type, std::int64_t raw) noexcept {
    if (type == core::SerialFieldType::WIDE_DECIMAL) {
        return core::WideDecimal{raw}.toDouble();
    }

    if (type != core::SerialFieldType::DECIMAL) {
        return static_cast<double>(raw);
    }

    constexpr unsigned STANDARD_SHIFT = 4;
    constexpr unsigned EXTRA_SHIFT = 7;
    constexpr unsigned POWER_MASK = 0xf;
    constexpr unsigned EXTRA_CODE_MASK = 7;
    constexpr unsigned UNITY_POWER = 9;
    constexpr std::array<int, 8> EXTRA_DIVISORS{-1, -1, 10000000, 100000000, 0, 0, 128, -1};
    constexpr std::array<double, 10> POWERS{1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
    const auto tiny = static_cast<std::int32_t>(raw);
    const auto power = static_cast<unsigned>(tiny) & POWER_MASK;

    if (!power) {
        const auto divisor = EXTRA_DIVISORS[(static_cast<unsigned>(tiny) >> STANDARD_SHIFT) & EXTRA_CODE_MASK];
        const auto mantissa = tiny >> (divisor >= 0 ? EXTRA_SHIFT : STANDARD_SHIFT);

        if (divisor > 0) {
            return static_cast<double>(mantissa) / divisor;
        }

        if (!mantissa) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return mantissa > 0 ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();
    }

    const auto mantissa = static_cast<double>(tiny >> STANDARD_SHIFT);

    return power <= UNITY_POWER ? mantissa * POWERS[UNITY_POWER - power] : mantissa / POWERS[power - UNITY_POWER];
}

/// Exact plain integer and decimal identities accepted by Java numeric mapping accessors.
inline constexpr std::array NUMERIC_TYPES{core::SerialFieldType::UTF_CHAR,    core::SerialFieldType::INT,
                                          core::SerialFieldType::COMPACT_INT, core::SerialFieldType::DECIMAL,
                                          core::SerialFieldType::LONG,        core::SerialFieldType::WIDE_DECIMAL};

/// Exact scalar identities whose first Java physical int slot can be read without conversion.
inline constexpr std::array RAW_INT_TYPES{
    core::SerialFieldType::UTF_CHAR,     core::SerialFieldType::INT,          core::SerialFieldType::COMPACT_INT,
    core::SerialFieldType::DECIMAL,      core::SerialFieldType::SHORT_STRING, core::SerialFieldType::TIME_SECONDS,
    core::SerialFieldType::SEQUENCE,     core::SerialFieldType::DATE,         core::SerialFieldType::LONG,
    core::SerialFieldType::WIDE_DECIMAL, core::SerialFieldType::TIME_MILLIS};

/** Read the first Java physical int slot of a validated logical scalar.
 * @param type Exact scalar type from RAW_INT_TYPES.
 * @param raw Complete canonical logical value, including both slots for a long type.
 * @return Signed first slot; long fields expose their high 32 bits.
 */
inline std::int32_t projectPhysicalInt(core::SerialFieldType type, std::int64_t raw) noexcept {
    constexpr unsigned PHYSICAL_INT_BITS = 32;
    const auto representation = core::representationBits(std::to_underlying(type));

    if (representation == std::to_underlying(core::FieldRepresentation::LONG) ||
        representation == std::to_underlying(core::FieldRepresentation::WIDE_DECIMAL) ||
        representation == std::to_underlying(core::FieldRepresentation::TIME_MILLIS)) {
        return static_cast<std::int32_t>(raw >> PHYSICAL_INT_BITS);
    }

    return static_cast<std::int32_t>(raw);
}

/** Compose a bounded signed integer and decimal scale with Java's exact integer rounding.
 * @param significand Signed integer magnitude before decimal scaling.
 * @param scale Decimal precision in -128..8, as produced by supported numeric inputs.
 * @return Canonical WideDecimal bits, including special rank zero and normalized finite zero.
 */
inline core::WideDecimal composeEventDecimal(std::int64_t significand, int scale) noexcept {
    constexpr auto MIN_SIGNIFICAND = std::numeric_limits<std::int64_t>::min() >> core::detail::RANK_BITS;
    constexpr auto MAX_SIGNIFICAND = std::numeric_limits<std::int64_t>::max() >> core::detail::RANK_BITS;
    constexpr auto INTEGER_RANK = core::detail::INTEGER_RANK;
    constexpr std::int64_t DECIMAL_RADIX = 10;
    auto rank = scale + INTEGER_RANK;

    if (rank <= 0) {
        return {significand == 0 ? 0 : significand > 0 ? core::detail::RANK_RADIX : -core::detail::RANK_RADIX};
    }

    int reduction = 0;

    if (significand < MIN_SIGNIFICAND || significand > MAX_SIGNIFICAND) {
        // Include the half-unit window that rounds into the fitting significand interval.
        if (significand >= MIN_SIGNIFICAND * 10 - 5 && significand < MAX_SIGNIFICAND * 10 + 5) {
            reduction = 1;
        } else if (significand >= MIN_SIGNIFICAND * 100 - 50 && significand < MAX_SIGNIFICAND * 100 + 50) {
            reduction = 2;
        } else {
            reduction = 3;
        }
    }

    if (reduction) {
        constexpr std::array<std::int64_t, 4> POWERS{1, 10, 100, 1000};
        const auto halfDivisor = POWERS[reduction] / 2;
        // Half ties round toward positive infinity, including negative long extremes.
        significand = significand >= 0 ? (significand / halfDivisor + 1) >> 1 : ((significand + 1) / halfDivisor) >> 1;
        rank -= reduction;
    }

    if (!significand) {
        return {INTEGER_RANK};
    }

    while (rank > INTEGER_RANK && significand % DECIMAL_RADIX == 0) {
        significand /= DECIMAL_RADIX;
        --rank;
    }

    while (rank < INTEGER_RANK && significand >= MIN_SIGNIFICAND / DECIMAL_RADIX &&
           significand <= MAX_SIGNIFICAND / DECIMAL_RADIX) {
        significand *= DECIMAL_RADIX;
        ++rank;
    }

    // Java deliberately moves a one-decimal-place fitting value to rank 130.
    if (rank == INTEGER_RANK + 1 && significand >= MIN_SIGNIFICAND / DECIMAL_RADIX &&
        significand <= MAX_SIGNIFICAND / DECIMAL_RADIX) {
        significand *= DECIMAL_RADIX;
        ++rank;
    }

    return {std::bit_cast<std::int64_t>((static_cast<std::uint64_t>(significand) << core::detail::RANK_BITS) |
                                        static_cast<std::uint64_t>(rank))};
}

/** Project a present validated event numeric field without changing the generic value.
 * @param type Exact type from NUMERIC_TYPES, validated before this call.
 * @param raw Validated exact scalar representation for that type.
 * @return Java getAsWideDecimal projection, with unchanged raw bits only for WIDE_DECIMAL.
 */
inline core::WideDecimal projectPresentDecimal(core::SerialFieldType type, std::int64_t raw) noexcept {
    if (type == core::SerialFieldType::WIDE_DECIMAL) {
        return {raw};
    }

    if (type == core::SerialFieldType::DECIMAL) {
        constexpr unsigned STANDARD_SHIFT = 4;
        constexpr unsigned EXTRA_SHIFT = 7;
        constexpr unsigned EXTRA_MASK = 0x7f;
        constexpr unsigned STANDARD_MASK = 0xf;
        constexpr unsigned FRACTION_128_FLAG = 0x60;
        constexpr std::int64_t FRACTION_128_MULTIPLIER = 78125;
        constexpr int UNITY_POWER = 9;
        constexpr std::array EXTRA_PRECISION{-128, -128, 7, 8, -128, -128, 7, -128};
        const auto tiny = static_cast<std::int32_t>(raw);
        const auto flag = static_cast<unsigned>(tiny) & EXTRA_MASK;
        const auto power = flag & STANDARD_MASK;
        const auto extra = flag >> STANDARD_SHIFT;
        const auto shift = power != 0 || extra == 0 || extra == 1 || extra == 7 ? STANDARD_SHIFT : EXTRA_SHIFT;
        const auto mantissa = flag == FRACTION_128_FLAG ? (std::int64_t{tiny} >> EXTRA_SHIFT) * FRACTION_128_MULTIPLIER
                                                        : std::int64_t{tiny} >> shift;

        return composeEventDecimal(mantissa, power ? static_cast<int>(power) - UNITY_POWER : EXTRA_PRECISION[extra]);
    }

    return composeEventDecimal(raw, 0);
}

} // namespace org::ttldtor::qdcxx::events::detail
