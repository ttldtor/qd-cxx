// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstddef>
#include <string_view>

namespace org::ttldtor::qdcxx::core::detail {

/** Check the nonempty, well-formed UTF-16 policy for schema names and explicit lookup keys.
 * Embedded NUL is valid; this policy does not apply to string field values or symbols.
 * @param name Borrowed record or lookup name.
 * @return Whether the name is nonempty and every surrogate belongs to a complete pair.
 */
inline bool validSchemaKey(std::u16string_view name) noexcept {
    constexpr char16_t HIGH_START = 0xd800;
    constexpr char16_t LOW_START = 0xdc00;
    constexpr char16_t SURROGATE_END = 0xdfff;

    if (name.empty()) {
        return false;
    }

    for (std::size_t i = 0; i < name.size(); ++i) {
        if (name[i] >= HIGH_START && name[i] < LOW_START) {
            if (++i == name.size() || name[i] < LOW_START || name[i] > SURROGATE_END) {
                return false;
            }
        } else if (name[i] >= LOW_START && name[i] <= SURROGATE_END) {
            return false;
        }
    }

    return true;
}

/** Charge a logical count without overflowing arithmetic.
 * Callers retain responsibility for the order of validation and charges.
 * @param count Requested logical elements.
 * @param remaining Remaining caller budget, decreased only on success.
 * @return Whether the complete request fits.
 */
inline bool chargeBudget(std::size_t count, std::size_t &remaining) noexcept {
    if (count > remaining) {
        return false;
    }

    remaining -= count;

    return true;
}

} // namespace org::ttldtor::qdcxx::core::detail
