// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstdint>

namespace org::ttldtor::qdcxx::replay::detail {
/** Positive elapsed source milliseconds, including the full difference between signed extremes. */
constexpr std::uint64_t positiveElapsedMs(std::int64_t timestamp, std::int64_t origin) noexcept {
    return timestamp > origin ? static_cast<std::uint64_t>(timestamp) - static_cast<std::uint64_t>(origin) : 0;
}
} // namespace org::ttldtor::qdcxx::replay::detail
