// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "decimal_projection.hpp"
#include "wide_integer_projection.hpp"
#include <bit>
#include <cmath>
#include <limits>

namespace org::ttldtor::qdcxx::events::detail {
/** Apply the production numeric getAsInt conversion before a delegate widens its result.
 * @param type Exact validated numeric type.
 * @param raw Complete canonical logical value.
 * @return Java signed32 narrowing, including decimal saturation and wide integer rounding.
 */
inline std::int32_t projectPresentInt(core::SerialFieldType type, std::int64_t raw) noexcept {
    if (type == core::SerialFieldType::DECIMAL) {
        const auto value = projectPresentBusinessDouble(type, raw);

        if (std::isnan(value)) {
            return 0;
        }

        if (value >= 0x1p31) {
            return std::numeric_limits<std::int32_t>::max();
        }

        if (value <= -0x1p31) {
            return std::numeric_limits<std::int32_t>::min();
        }

        return static_cast<std::int32_t>(value);
    }

    const auto value = type == core::SerialFieldType::WIDE_DECIMAL ? projectWideInteger(raw) : raw;

    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}
} // namespace org::ttldtor::qdcxx::events::detail
