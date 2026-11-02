// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "../core/wide_decimal_representation.hpp"
#include "qd/core/serial_field_type.hpp"
#include "qd/core/wide_decimal.hpp"

namespace org::ttldtor::qdcxx::events::detail {

/** Project a present validated event numeric field without changing the generic value.
 * @param type Canonical COMPACT_INT or WIDE_DECIMAL exact type, validated before this call.
 * @param raw Validated exact scalar representation for that type.
 * @return Exact integer decimal for COMPACT_INT, or unchanged raw WideDecimal bits.
 */
inline core::WideDecimal projectPresentDecimal(core::SerialFieldType type, std::int64_t raw) noexcept {
    return {type == core::SerialFieldType::COMPACT_INT ? raw * core::detail::RANK_RADIX + core::detail::INTEGER_RANK
                                                       : raw};
}

} // namespace org::ttldtor::qdcxx::events::detail
