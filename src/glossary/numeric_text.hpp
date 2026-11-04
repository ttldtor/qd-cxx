// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::glossary::detail {
/** @param text Borrowed exact legacy decimal text.
 * @return Parsed number or NaN for invalid syntax; no text is retained.
 */
double parseAdditionalNumber(std::u16string_view text) noexcept;
/** @param value Finite binary64, including signed zero.
 * @return Owned reference 15-digit text.
 * @throws std::bad_alloc Output allocation fails.
 */
std::u16string formatAdditionalNumber(double value);
/** @param value Any binary64 value.
 * @return Reference precision15 HALF_UP cleanup; nonfinite and signed zero pass through.
 * @throws std::bad_alloc Decimal conversion allocation fails.
 */
double roundPriceDecimal(double value);
} // namespace org::ttldtor::qdcxx::glossary::detail
