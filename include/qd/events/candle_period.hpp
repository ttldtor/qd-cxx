// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** Candle aggregation unit. This value model does not aggregate or receive events. */
enum class CandleType {
    TICK,           ///< A number of individual ticks.
    SECOND,         ///< A number of seconds.
    MINUTE,         ///< A number of minutes.
    HOUR,           ///< A number of hours.
    DAY,            ///< A number of days.
    WEEK,           ///< A number of weeks.
    MONTH,          ///< A number of months; its duration estimate uses thirty days.
    OPTEXP,         ///< A number of option expirations; its duration estimate uses thirty days.
    YEAR,           ///< A number of years; its duration estimate uses 365 days.
    VOLUME,         ///< A trade volume.
    PRICE,          ///< A price range with the next open equal to the previous close.
    PRICE_MOMENTUM, ///< A price range with the next open one tick beyond the previous close.
    PRICE_RENKO     ///< A price range based on successive highs or lows.
};
/** Return the canonical short name, including m for MINUTE and mo for MONTH.
 * @param type Valid aggregation unit.
 * @return Borrowed immutable static UTF-16 text; valid for the program lifetime.
 * @throws std::invalid_argument The enum value is invalid.
 */
[[nodiscard]] std::u16string_view candleTypeShortName(CandleType type);
/** Parse a canonical short name, a case-insensitive enum-name prefix or a plural enum name.
 * Ambiguous prefixes select the first enum in declaration order. Plural s must be lowercase.
 * @param text Borrowed UTF-16 input; no trimming, normalization or input retention.
 * @return Parsed aggregation unit.
 * @throws std::invalid_argument The text is empty or does not identify a unit.
 */
[[nodiscard]] CandleType parseCandleType(std::u16string_view text);
/** Return the unit's duration estimate; no calendar or trading-session calculations occur.
 * @param type Valid aggregation unit.
 * @return Milliseconds; MONTH/OPTEXP use thirty days, YEAR uses 365, non-time units use zero.
 * @throws std::invalid_argument The enum value is invalid.
 */
[[nodiscard]] std::int64_t candleTypeIntervalMillis(CandleType type);

/** Independent aggregation value and unit. Every binary64 multiplier is accepted.
 * No input storage, runtime, callback or process configuration is retained. Concurrent const
 * operations are independent; mutation of the same object requires external synchronization.
 * See spec/events/CANDLE-PERIOD.md for non-roundtripping nonfinite and signed-zero spellings.
 */
class CandlePeriod final {
    double value_;
    CandleType type_;

    public:
    /** Construct an independent period; the default is one tick.
     * @param value Any binary64 multiplier, including negative, nonfinite and signed-zero values.
     * @param type Valid aggregation unit.
     * @throws std::invalid_argument The enum value is invalid.
     */
    constexpr explicit CandlePeriod(double value = 1, CandleType type = CandleType::TICK) : value_(value), type_(type) {
        if (static_cast<unsigned>(type) > static_cast<unsigned>(CandleType::PRICE_RENKO)) {
            throw std::invalid_argument("Invalid candle type");
        }
    }

    /** @return Unmodified multiplier, including its signed zero or NaN payload. */
    [[nodiscard]] double getValue() const noexcept;
    /** @return Valid aggregation unit. */
    [[nodiscard]] CandleType getType() const noexcept;
    /** Multiply the unit estimate by the multiplier, then apply Java double-to-long conversion.
     * @return Truncated milliseconds; NaN becomes zero and overflow saturates to signed64 limits.
     * Non-time units multiplied by infinity produce NaN and therefore zero.
     */
    [[nodiscard]] std::int64_t getPeriodIntervalMillis() const noexcept;
    /** Render the reference canonical spelling; one omits its multiplier, either zero renders 0.
     * @return Independently owned UTF-16 period text.
     * @throws std::bad_alloc Output allocation fails.
     * @throws std::length_error Output exceeds string capacity.
     */
    [[nodiscard]] std::u16string toString() const;
    /** Parse the numeric prefix made of ASCII digits, dot, signs and e/E, then parse the unit.
     * An absent numeric prefix means one. NaN/Infinity and hexadecimal literals are not admitted
     * by this scanner even though construction and rendering accept nonfinite multipliers.
     * @param text Borrowed UTF-16 input; no trimming or input retention.
     * @return Independent period value.
     * @throws std::invalid_argument The numeric prefix or aggregation unit is invalid.
     * @throws std::bad_alloc Temporary numeric parsing allocation fails.
     * @throws std::length_error Temporary string exceeds capacity.
     */
    [[nodiscard]] static CandlePeriod parse(std::u16string_view text);
    /** Compare the unit and Java Double.compare equivalence of the multiplier.
     * @param other Independent period to compare.
     * @return True for equal units and bit-equal values; all NaNs compare equal, signed zeros differ.
     */
    [[nodiscard]] bool operator==(const CandlePeriod &other) const noexcept;

    static const CandlePeriod TICK;    ///< One tick.
    static const CandlePeriod DAY;     ///< One day.
    static const CandlePeriod DEFAULT; ///< The default, one tick.
};
} // namespace org::ttldtor::qdcxx::events
