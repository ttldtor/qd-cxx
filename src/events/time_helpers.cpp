// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/time_helpers.hpp"

#include <array>
#include <bit>
#include <limits>

namespace org::ttldtor::qdcxx::events {
namespace {
constexpr std::int64_t NANOS_PER_MILLI = 1000000;
constexpr std::int64_t MILLIS_PER_SECOND = 1000;
constexpr unsigned SECONDS_SHIFT = 32;
constexpr unsigned MILLIS_SHIFT = 22;
constexpr std::uint64_t PACKED_MILLIS_MASK = 0x3ff;
constexpr std::array<std::int32_t, 13> DAYS_BEFORE_MONTH{0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

/** Explicit Java int arithmetic; intermediates never overflow a C++ signed integer.
 * @param value Wider or unsigned intermediate whose low32 bits are required.
 * @return Low signed32 bits, independent of implementation-defined signed narrowing.
 */
std::int32_t intBits(std::int64_t value) noexcept {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

/** @param value Signed numerator.
 * @param divisor Strictly positive divisor.
 * @return Mathematical floor quotient; the divisor excludes signed-minimum/-1 overflow.
 */
std::int64_t floorDivide(std::int64_t value, std::int64_t divisor) noexcept {
    const auto quotient = value / divisor;

    return quotient - (value % divisor < 0 ? 1 : 0);
}

/** @param value Signed numerator.
 * @param divisor Strictly positive divisor.
 * @return Nonnegative mathematical remainder.
 */
std::int64_t floorRemainder(std::int64_t value, std::int64_t divisor) noexcept {
    const auto remainder = value % divisor;

    return remainder < 0 ? remainder + divisor : remainder;
}
} // namespace

std::int64_t getNanosFromMillisAndNanoPart(std::int64_t timeMillis, std::int32_t timeNanoPart) noexcept {
    return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(timeMillis) * NANOS_PER_MILLI +
                                       static_cast<std::uint64_t>(static_cast<std::int64_t>(timeNanoPart)));
}

std::int64_t getMillisFromNanos(std::int64_t timeNanos) noexcept {
    return floorDivide(timeNanos, NANOS_PER_MILLI);
}

std::int32_t getNanoPartFromNanos(std::int64_t timeNanos) noexcept {
    return static_cast<std::int32_t>(floorRemainder(timeNanos, NANOS_PER_MILLI));
}

std::int32_t getSecondsFromTime(std::int64_t timeMillis) noexcept {
    const auto seconds = floorDivide(timeMillis, MILLIS_PER_SECOND);

    if (seconds < std::numeric_limits<std::int32_t>::min()) {
        return std::numeric_limits<std::int32_t>::min();
    }

    if (seconds > std::numeric_limits<std::int32_t>::max()) {
        return std::numeric_limits<std::int32_t>::max();
    }

    return static_cast<std::int32_t>(seconds);
}

std::int32_t getMillisFromTime(std::int64_t timeMillis) noexcept {
    return static_cast<std::int32_t>(floorRemainder(timeMillis, MILLIS_PER_SECOND));
}

std::int64_t getTimeFromTimeSequence(std::int64_t timeSequence) noexcept {
    const auto raw = static_cast<std::uint64_t>(timeSequence);
    const auto seconds = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(raw >> SECONDS_SHIFT));

    return static_cast<std::int64_t>(seconds) * MILLIS_PER_SECOND +
           static_cast<std::int64_t>((raw >> MILLIS_SHIFT) & PACKED_MILLIS_MASK);
}

std::int32_t getSequenceFromTimeSequence(std::int64_t timeSequence) noexcept {
    return static_cast<std::int32_t>(static_cast<std::uint64_t>(timeSequence) & MAX_EVENT_SEQUENCE);
}

std::int64_t withTimeSequenceTime(std::int64_t timeSequence, std::int64_t timeMillis) noexcept {
    const auto seconds = static_cast<std::uint64_t>(static_cast<std::int64_t>(getSecondsFromTime(timeMillis)));
    const auto millis = static_cast<std::uint64_t>(getMillisFromTime(timeMillis));

    return std::bit_cast<std::int64_t>((seconds << SECONDS_SHIFT) | (millis << MILLIS_SHIFT) |
                                       static_cast<std::uint64_t>(getSequenceFromTimeSequence(timeSequence)));
}

std::expected<std::int64_t, EventValueError> withTimeSequenceSequence(std::int64_t timeSequence,
                                                                      std::int32_t sequence) noexcept {
    if (sequence < 0 || sequence > MAX_EVENT_SEQUENCE) {
        return std::unexpected(EventValueError::INVALID_SEQUENCE);
    }

    return std::bit_cast<std::int64_t>(
        (static_cast<std::uint64_t>(timeSequence) & ~static_cast<std::uint64_t>(MAX_EVENT_SEQUENCE)) |
        static_cast<std::uint64_t>(sequence));
}

std::expected<std::int32_t, EventValueError> getDayIdByYearMonthDay(std::int32_t year, std::int32_t month,
                                                                    std::int32_t day) noexcept {
    if (month < 1 || month > 12) {
        return std::unexpected(EventValueError::INVALID_MONTH);
    }

    auto dayOfYear = intBits(static_cast<std::int64_t>(DAYS_BEFORE_MONTH[static_cast<std::size_t>(month)]) + day - 1);

    if (month > 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
        dayOfYear = intBits(static_cast<std::int64_t>(dayOfYear) + 1);
    }

    // DayUtil subtracts one in the int domain before its three floor divisions.
    const auto precedingYear = intBits(static_cast<std::int64_t>(year) - 1);
    auto result = intBits(static_cast<std::int64_t>(year) * 365);
    result = intBits(static_cast<std::int64_t>(result) + floorDivide(precedingYear, 4));
    result = intBits(static_cast<std::int64_t>(result) - floorDivide(precedingYear, 100));
    result = intBits(static_cast<std::int64_t>(result) + floorDivide(precedingYear, 400));
    result = intBits(static_cast<std::int64_t>(result) + dayOfYear);

    constexpr std::int32_t EPOCH_DAY_OFFSET = 719527;

    return intBits(static_cast<std::int64_t>(result) - EPOCH_DAY_OFFSET);
}

std::expected<std::int32_t, EventValueError> getDayIdByYearMonthDay(std::int32_t yearMonthDay) noexcept {
    if (yearMonthDay >= 0) {
        return getDayIdByYearMonthDay(yearMonthDay / 10000, yearMonthDay / 100 % 100, yearMonthDay % 100);
    }

    const auto magnitude = intBits(-static_cast<std::int64_t>(yearMonthDay));

    return getDayIdByYearMonthDay(intBits(-static_cast<std::int64_t>(magnitude / 10000)), magnitude / 100 % 100,
                                  magnitude % 100);
}

std::int32_t getYearMonthDayByDayId(std::int32_t dayId) noexcept {
    // March-based 400/100/4/1-year cycles reproduce DayUtil, including its initial int overflow.
    constexpr std::int32_t MARCH_EPOCH_SHIFT = 2472632;
    constexpr std::int32_t DAYS_PER_400_YEARS = 146097;
    constexpr std::int32_t DAYS_PER_CENTURY = 36524;
    constexpr std::int32_t DAYS_PER_FOUR_YEARS = 1461;
    const auto shifted = intBits(static_cast<std::int64_t>(dayId) + MARCH_EPOCH_SHIFT);
    const auto cycles400 = static_cast<std::int32_t>(floorDivide(shifted, DAYS_PER_400_YEARS));
    const auto days400 =
        intBits(static_cast<std::int64_t>(shifted) - static_cast<std::int64_t>(cycles400) * DAYS_PER_400_YEARS);
    const auto centuries = (days400 / DAYS_PER_CENTURY + 1) * 3 / 4;
    const auto daysCentury = days400 - centuries * DAYS_PER_CENTURY;
    const auto cycles4 = daysCentury / DAYS_PER_FOUR_YEARS;
    const auto days4 = daysCentury - cycles4 * DAYS_PER_FOUR_YEARS;
    const auto years1 = (days4 / 365 + 1) * 3 / 4;
    const auto days1 = days4 - years1 * 365;
    const auto years = cycles400 * 400 + centuries * 100 + cycles4 * 4 + years1;
    const auto months = (days1 * 5 + 308) / 153 - 2;
    const auto days = days1 - (months + 4) * 153 / 5 + 122;
    const auto year = years - 4800 + (months + 2) / 12;
    const auto month = (months + 2) % 12 + 1;
    const auto day = days + 1;
    const auto magnitude = intBits(static_cast<std::int64_t>(year < 0 ? -year : year) * 10000 + month * 100 + day);

    return year >= 0 ? magnitude : intBits(-static_cast<std::int64_t>(magnitude));
}
} // namespace org::ttldtor::qdcxx::events
