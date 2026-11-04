// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/event_value.hpp"

#include <cstdint>
#include <expected>

namespace org::ttldtor::qdcxx::events {
inline constexpr std::int32_t MAX_EVENT_SEQUENCE = (1 << 22) - 1; ///< Largest unsigned 22-bit event sequence.
/** Combine milliseconds and raw nanosecond remainder with Java signed64 wrapping.
 * @param timeMillis Milliseconds since 1970-01-01T00:00:00Z; every signed64 value is accepted.
 * @param timeNanoPart Raw signed32 remainder; customary 0..999999 is not enforced.
 * @return Low signed64 bits of timeMillis * 1000000 + timeNanoPart.
 */
[[nodiscard]] std::int64_t getNanosFromMillisAndNanoPart(std::int64_t timeMillis, std::int32_t timeNanoPart) noexcept;
/** @param timeNanos Signed64 nanoseconds since the Unix epoch.
 * @return Mathematical floor of timeNanos / 1000000, including negative timestamps.
 */
[[nodiscard]] std::int64_t getMillisFromNanos(std::int64_t timeNanos) noexcept;
/** @param timeNanos Signed64 nanoseconds since the Unix epoch.
 * @return Nonnegative remainder in 0..999999; no allocation or retained state.
 */
[[nodiscard]] std::int32_t getNanoPartFromNanos(std::int64_t timeNanos) noexcept;
/** @param timeMillis Signed64 milliseconds since the Unix epoch.
 * @return Floor seconds saturated to signed32 limits, as in the production TimeUtil.
 */
[[nodiscard]] std::int32_t getSecondsFromTime(std::int64_t timeMillis) noexcept;
/** @param timeMillis Signed64 milliseconds since the Unix epoch.
 * @return Nonnegative millisecond remainder in 0..999, even if seconds saturate.
 */
[[nodiscard]] std::int32_t getMillisFromTime(std::int64_t timeMillis) noexcept;
/** @param timeSequence Raw signed seconds in high32 and millisecond/sequence bits in low32.
 * @return Business milliseconds, including all ten millisecond bits (0..1023).
 */
[[nodiscard]] std::int64_t getTimeFromTimeSequence(std::int64_t timeSequence) noexcept;
/** @param timeSequence Complete raw packed time/sequence word.
 * @return Low22 sequence bits; no uniqueness or monotonicity guarantee.
 */
[[nodiscard]] std::int32_t getSequenceFromTimeSequence(std::int64_t timeSequence) noexcept;
/** Replace business time while preserving the existing low22 sequence bits.
 * @param timeSequence Previous packed raw word.
 * @param timeMillis New signed64 milliseconds; seconds saturate to signed32, remainder uses floor modulus.
 * @return New packed word; its millisecond component is in 0..999.
 */
[[nodiscard]] std::int64_t withTimeSequenceTime(std::int64_t timeSequence, std::int64_t timeMillis) noexcept;
/** Replace only the low22 sequence bits, preserving all remaining bits.
 * @param timeSequence Previous raw packed word, including reserved millisecond patterns.
 * @param sequence New sequence in 0..MAX_EVENT_SEQUENCE.
 * @return New packed word or INVALID_SEQUENCE; the caller's original word is unchanged.
 */
[[nodiscard]] std::expected<std::int64_t, EventValueError> withTimeSequenceSequence(std::int64_t timeSequence,
                                                                                    std::int32_t sequence) noexcept;
/** Convert a proleptic Gregorian year/month/day using the production DayUtil arithmetic.
 * @param year Any signed32 astronomical year, including year zero and negative years.
 * @param month Month in 1..12; it is the only validated calendar component.
 * @param day Any signed32 day; values outside the month's usual range normalize arithmetically.
 * @return Signed32 days since 1970-01-01, with Java int wrapping, or INVALID_MONTH.
 */
[[nodiscard]] std::expected<std::int32_t, EventValueError> getDayIdByYearMonthDay(std::int32_t year, std::int32_t month,
                                                                                  std::int32_t day) noexcept;
/** Convert a signed packed Gregorian date using DayUtil's signed decimal decomposition.
 * @param yearMonthDay yearSign * (abs(year)*10000 + month*100 + day), with +1 for year zero; every signed32 input is
 * examined.
 * @return Signed32 day ID with Java int wrapping, or INVALID_MONTH for the extracted month.
 */
[[nodiscard]] std::expected<std::int32_t, EventValueError> getDayIdByYearMonthDay(std::int32_t yearMonthDay) noexcept;
/** Apply DayUtil's inverse conversion, including its complete signed32 overflow behavior.
 * This is a numeric compatibility operation; overflowed extreme inputs need not round-trip as calendar dates.
 * @param dayId Any signed32 number of days since 1970-01-01.
 * @return Sign(year) * (abs(year)*10000 + month*100 + day), with Java int wrapping.
 */
[[nodiscard]] std::int32_t getYearMonthDayByDayId(std::int32_t dayId) noexcept;
} // namespace org::ttldtor::qdcxx::events
