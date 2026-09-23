// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstddef>

namespace org::ttldtor::qdcxx::encoding::detail {

/// One full-width escape prefix plus eight payload octets; see spec/encoding/PRIMITIVES.md.
inline constexpr std::size_t MAX_COMPACT_LONG_BYTES = 9;

} // namespace org::ttldtor::qdcxx::encoding::detail
