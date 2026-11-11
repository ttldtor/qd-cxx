// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/local_schema.hpp"

#include <algorithm>

namespace org::ttldtor::qdcxx::events::detail {
/** Check that a native indexed-order mapper and the HISTORY collector use the same field.
 * @param record Borrowed validated local record declaration.
 * @return True for the exact Index lookup bound to the native compact low word.
 */
inline bool hasOrderIndexCoordinate(const core::LocalRecord &record) noexcept {
    const auto *low =
        record.indexedTimeLayout ? std::get_if<core::IndexedTimeLowInt32>(&*record.indexedTimeLayout) : nullptr;
    const auto index = std::ranges::find_if(record.lookups, [](const auto &lookup) {
        return lookup.name == u"Index";
    });

    return low && index != record.lookups.end() && low->field == index->fieldId;
}
} // namespace org::ttldtor::qdcxx::events::detail
