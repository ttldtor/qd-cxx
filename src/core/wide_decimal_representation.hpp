// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstdint>

namespace org::ttldtor::qdcxx::core::detail {

/// WideDecimal stores its rank in the low octet, below the signed significand.
inline constexpr unsigned int RANK_BITS = 8;
/// Mask selecting the rank without changing the raw signed representation.
inline constexpr std::uint64_t RANK_MASK = 0xff;
/// Rank for a zero decimal exponent, including the canonical finite integer zero.
inline constexpr int INTEGER_RANK = 128;
/// Multiplier placing an exact integer significand above the rank octet.
inline constexpr std::int64_t RANK_RADIX = std::int64_t{1} << RANK_BITS;

} // namespace org::ttldtor::qdcxx::core::detail
