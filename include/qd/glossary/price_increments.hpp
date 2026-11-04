// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::glossary {
/** Rounding policies for a valid instrument price. */
enum class PriceRoundingMode {
    UP,         ///< Away from zero.
    DOWN,       ///< Toward zero.
    CEILING,    ///< Toward positive infinity.
    FLOOR,      ///< Toward negative infinity.
    HALF_UP,    ///< Nearest, ties away from zero.
    HALF_DOWN,  ///< Nearest, ties toward zero.
    HALF_EVEN,  ///< Nearest, ties to an even multiple.
    UNNECESSARY ///< Refuse prices that require rounding.
};

/** Finite bounds checked before retaining input or allocating range tables. */
struct PriceIncrementLimits {
    std::size_t textUnits = 1048576; ///< Maximum UTF-16 input/output code units.
    std::size_t ranges = 16384;      ///< Maximum ranges; zero admits only the empty value.
};

/** Immutable owned price ranges, reflected symmetrically across zero.
 * Values share immutable ownership when copied; moves leave an empty reusable source.
 * Exact original text defines equality. Queries are safe concurrently with other const queries;
 * callers synchronize assignment/destruction. No global cache or background work is used.
 * Decimal accuracy follows the reference domain of at most 14 significant/fractional digits.
 */
class PriceIncrements {
    /** Complete immutable model shared only by owning values. */
    struct State;
    std::shared_ptr<const State> state_;

    /** @param state Complete immutable owned state, or null for empty. */
    explicit PriceIncrements(std::shared_ptr<const State> state) noexcept;

    public:
    /** Construct the empty rule: increment zero, precision zero, empty text. */
    PriceIncrements() noexcept = default;
    /** @return Process-lifetime empty rule, safe before main. */
    [[nodiscard]] static const PriceIncrements &empty() noexcept;
    /** Parse exact text; null, empty and a signed decimal zero spelling produce the empty rule.
     * @param text UTF-16 input, borrowed only for this call.
     * @param limits Finite text/range limits; checked before retaining input.
     * @return Independent immutable ownership of the parsed rule.
     * @throws std::invalid_argument Invalid separators, values or range ordering.
     * @throws std::length_error A configured bound is exceeded.
     * @throws std::bad_alloc Allocation fails; no partial value is published.
     */
    [[nodiscard]] static PriceIncrements valueOf(std::optional<std::u16string_view> text,
                                                 PriceIncrementLimits limits = {});
    /** Render a single nonnegative finite increment with reference precision and parse it.
     * @param increment Nonnegative finite increment; either signed zero means empty.
     * @param limits Finite rendered text/range bounds.
     * @return Immutable owned rule; rendering can round the supplied binary value.
     * @throws std::invalid_argument Negative/nonfinite increment.
     * @throws std::length_error Configured bound exceeded.
     * @throws std::bad_alloc Allocation fails.
     */
    [[nodiscard]] static PriceIncrements valueOf(double increment, PriceIncrementLimits limits = {});
    /** Render alternating increments and upper limits; the last infinite limit is omitted.
     * @param increments Borrowed array with odd length; empty means the empty rule.
     * @param limits Finite rendered text/range bounds.
     * @return Immutable owned rule, reparsed after decimal rendering.
     * @throws std::invalid_argument Even length, invalid values or unordered limits.
     * @throws std::length_error Configured bound exceeded.
     * @throws std::bad_alloc Allocation fails.
     */
    [[nodiscard]] static PriceIncrements valueOf(std::span<const double> increments, PriceIncrementLimits limits = {});
    /** @return Exact owned text, borrowed until this value is assigned or destroyed. */
    [[nodiscard]] const std::u16string &getText() const noexcept;
    /** @return Independent copy of alternating increments/upper limits; empty gives {0}. */
    [[nodiscard]] std::vector<double> getPriceIncrements() const;
    /** @return Increment adjacent to zero, including zero for the empty rule. */
    [[nodiscard]] double getPriceIncrement() const noexcept;
    /** Select a range using the reference relative tolerance around breakpoints.
     * @param price Price, reflected for negative values; NaN selects the first range.
     * @param direction Negative/down, positive/up, zero/minimum at a breakpoint.
     * @return Appropriate increment, including zero for empty.
     */
    [[nodiscard]] double getPriceIncrement(double price, std::int32_t direction = 0) const noexcept;
    /** @return Decimal precision adjacent to zero, from zero through 18. */
    [[nodiscard]] int getPricePrecision() const noexcept;
    /** @param price Price, reflected when negative; NaN selects the first range.
     * @return Decimal precision [0,18], choosing the lower range at a tolerated breakpoint.
     */
    [[nodiscard]] int getPricePrecision(double price) const noexcept;
    /** @param price Price; NaN and values with increment zero are returned unchanged.
     * @param direction Negative/floor, positive/ceiling, zero/half-even.
     * @return Rounded price with the reference 15-significant-digit cleanup.
     * @throws std::bad_alloc Decimal cleanup allocation fails.
     */
    [[nodiscard]] double roundPrice(double price, std::int32_t direction = 0) const;
    /** @param price Price; NaN and values with increment zero are returned unchanged.
     * @param mode Rounding policy.
     * @return Rounded price with reference decimal cleanup.
     * @throws std::domain_error UNNECESSARY requires rounding.
     * @throws std::invalid_argument Unknown enum value for a nonempty non-NaN input.
     * @throws std::bad_alloc Decimal cleanup allocation fails.
     */
    [[nodiscard]] double roundPrice(double price, PriceRoundingMode mode) const;
    /** @param price Price to advance, including NaN or infinity.
     * @param direction Nonzero sign selects upward/downward movement.
     * @param step Nonnegative finite minimum step, even when direction is negative.
     * @return Price advanced by max(step, applicable increment), then half-even rounded.
     * @throws std::invalid_argument Zero direction or invalid step.
     * @throws std::bad_alloc Decimal cleanup allocation fails.
     */
    [[nodiscard]] double incrementPrice(double price, std::int32_t direction, double step = 0) const;
    /** @return Java UTF-16 text hash with signed32 wrapping. */
    [[nodiscard]] std::int32_t hashCode() const noexcept;
    /** @return Independent copy of the original text. */
    [[nodiscard]] std::u16string toString() const;
    /** @param other Rule compared by original text, independently of numeric equality.
     * @return Whether exact text is equal.
     */
    [[nodiscard]] bool operator==(const PriceIncrements &other) const noexcept;
};
} // namespace org::ttldtor::qdcxx::glossary
