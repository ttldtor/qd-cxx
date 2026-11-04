// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "timezone_provider.hpp"
#include <limits>

namespace org::ttldtor::qdcxx::time::detail::provider {
// The backend-free target keeps fixed bindings available without native data or discovery.
Error initialize(const std::filesystem::path &, Limits, Stats &) noexcept {
    return Error::NOT_READY;
}

Error localToUtc(const char *, std::int64_t, std::int64_t &) noexcept {
    return Error::NOT_READY;
}

Error offsetAt(const char *, std::int64_t, std::int64_t &) noexcept {
    return Error::NOT_READY;
}

Error resolve(const char *, Zone &) noexcept {
    return Error::NOT_READY;
}

const char *canonicalName(Zone) noexcept {
    return "";
}

Error offsets(Zone, std::int64_t, std::int64_t &, std::int64_t &) noexcept {
    return Error::NOT_READY;
}

Error localOffsets(Zone, std::int64_t, std::int64_t &, std::int64_t &, int &) noexcept {
    return Error::NOT_READY;
}

std::int64_t maximumUtcMillis() noexcept {
    return std::numeric_limits<std::int64_t>::max();
}

bool legacyBoundaries(std::int64_t, std::int64_t, std::int64_t &, std::int64_t &) noexcept {
    return false;
}
} // namespace org::ttldtor::qdcxx::time::detail::provider
