// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/encoding/compact.hpp"

namespace org::ttldtor::qdcxx::encoding {

/** Read one UTF_CHAR code point, accepting overlong forms and isolated surrogate points.
 * A complete indicated tail is consumed before validation. This is independent of UTFString
 * framing and preserves supplementary points as int32. No input or continuation state is retained.
 * @param input Caller-owned bytes borrowed for this call; trailing bytes remain untouched.
 * @return Code point and one through four consumed bytes on OK; otherwise zero and exact consumed
 * prefix. Missing tails report INSUFFICIENT_INPUT; bad leads/tails/points report INVALID_ENCODING.
 */
[[nodiscard]] ReadResult<std::int32_t> readUtfChar(std::span<const std::byte> input) noexcept;

/** Write a canonical UTF_CHAR code point atomically, using at most four bytes.
 * @param value Code point in 0..0x10FFFF, including isolated surrogate points.
 * @param output Caller-owned writable bytes borrowed for this call; excess bytes remain untouched.
 * @return Written size on OK; zero on INVALID_ENCODING or INSUFFICIENT_OUTPUT with unchanged output.
 * Invalid points are rejected before capacity is checked.
 */
[[nodiscard]] WriteResult writeUtfChar(std::int32_t value, std::span<std::byte> output) noexcept;

} // namespace org::ttldtor::qdcxx::encoding
