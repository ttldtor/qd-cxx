// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstdint>
#include <string>

namespace org::ttldtor::qdcxx::core {

/** Exact classification of the wide decimal rank/significand representation. */
enum class DecimalKind {
    FINITE,            ///< Nonzero rank, including finite positive zero.
    NAN_VALUE,         ///< Zero rank and zero significand.
    POSITIVE_INFINITY, ///< Zero rank and positive significand, including noncanonical encodings.
    NEGATIVE_INFINITY  ///< Zero rank and negative significand, including noncanonical encodings.
};

/** Owned exact wide decimal bits with bounded presentation, without arithmetic or composition.
 * Finite values mean significand * 10^exponent. See spec/encoding/QUOTE-VALUES.md.
 */
struct WideDecimal {
    std::int64_t raw; ///< Unchanged signed wire bits; zero denotes NaN, not numeric zero.

    /** Classify the raw representation without normalization.
     * @return Finite, NaN or infinity classification.
     */
    [[nodiscard]] DecimalKind kind() const noexcept;

    /** Extract the signed high 56 bits, including for nonfinite representations.
     * @return Signed significand without losing raw rank bits in the owning value.
     */
    [[nodiscard]] std::int64_t significand() const noexcept;

    /** Extract the decimal exponent, which is meaningful as a scale only for finite values.
     * @return 128 minus the unsigned low eight bits, in -127..128.
     */
    [[nodiscard]] int exponent() const noexcept;

    /** Convert the exact decimal to nearest binary64 with ties to even, independent of locale.
     * @return Finite value, positive zero, signed infinity or canonical quiet NaN.
     */
    [[nodiscard]] double toDouble() const noexcept;

    /** Render decimal semantics using the bounded plain/scientific notation contract.
     * @return Owned ASCII text of at most 32 bytes; raw bits are never normalized or changed.
     */
    [[nodiscard]] std::string toString() const;
};

} // namespace org::ttldtor::qdcxx::core
