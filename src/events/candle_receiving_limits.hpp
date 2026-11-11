// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>

namespace org::ttldtor::qdcxx::events::detail {
// Candle restoration owns both parsed identity and base text. Prepay attribute
// extraction/normalization, physical text construction, and their disposal.
inline constexpr std::size_t CANDLE_SYMBOL_TEXT_PASSES = 32;
inline constexpr std::size_t CANDLE_SYMBOL_FIXED_BYTES = 1024;
} // namespace org::ttldtor::qdcxx::events::detail
