// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::detail {
/** Decimal digits and the exponent of the leading digit for a positive finite binary64. */
struct DecimalDigits {
    std::string digits; ///< Significant decimal digits without a point or sign.
    int exponent;       ///< Power of ten of the leading digit.
};

/** Generate the reference binary64 decimal digits without choosing a consumer's rendering policy.
 * @param value Positive finite nonzero binary64 value.
 * @return Exact reference digit sequence and its leading decimal exponent.
 * @throws std::bad_alloc String allocation fails.
 */
DecimalDigits decimalDigits(double value);
/** Compare a positive finite normal binary64 with complete fixed-point decimal text.
 * @param value Positive finite normal binary64 value.
 * @param text Unsigned decimal digits with at most one point, without exponent.
 * @return Negative, zero or positive when the binary value is below, equal or above the text.
 */
int compareBinaryToDecimal(double value, std::string_view text);
/** Parse Java binary64 text, including trimming, hexadecimal literals and overflow.
 * @param value Borrowed UTF-16 text, not retained.
 * @return Parsed value, preserving signed zero; nullopt for invalid syntax.
 * @throws std::bad_alloc Temporary ASCII allocation fails; never converted into a syntax error.
 * @throws std::length_error Temporary string exceeds its maximum size.
 */
std::optional<double> parseJavaDouble(std::u16string_view value);
} // namespace org::ttldtor::qdcxx::detail
