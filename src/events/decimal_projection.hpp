// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "../core/wide_decimal_representation.hpp"
#include "qd/core/serial_field_type.hpp"
#include "qd/core/wide_decimal.hpp"

namespace org::ttldtor::qdcxx::events::detail {

/** Project a present validated event numeric field without changing the generic value.
 * @param field Canonical COMPACT_INT or WIDE_DECIMAL value, validated before this call.
 * @return Exact integer decimal for COMPACT_INT, or unchanged raw WideDecimal bits.
 */
inline core::WideDecimal projectPresentDecimal(const core::FieldValue &field) noexcept {
    return {field.type == core::SerialFieldType::COMPACT_INT
                ? field.raw * core::detail::RANK_RADIX + core::detail::INTEGER_RANK
                : field.raw};
}

} // namespace org::ttldtor::qdcxx::events::detail
