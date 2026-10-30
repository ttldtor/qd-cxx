// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/static_binding.hpp"

namespace org::ttldtor::qdcxx::protocol::detail {
/** Count the exact raw metadata prefix before zero-width VOID elision.
 * @param description Borrowed physical sender declaration.
 * @return Zero, one or two recognized fields.
 */
inline std::size_t eventTimeFieldCount(const WireRecordDescription &description) noexcept {
    if (description.fields.empty() || description.fields[0].name != u"EventTime" ||
        description.fields[0].serialTypeId != 56) {
        return 0;
    }

    return description.fields.size() > 1 && description.fields[1].name == u"EventSequence" &&
                   description.fields[1].serialTypeId == 72
               ? 2
               : 1;
}

/** Find the exclusive physical action bound of leading effective Event fields.
 * @param description Borrowed declaration, including explicit zero-width VOID actions.
 * @return Action position before the first non-VOID ordinary field, or the full action count.
 */
inline std::size_t removalPrefixFieldCount(const WireRecordDescription &description) noexcept {
    for (std::size_t index = 0; index < description.fields.size(); ++index) {
        const auto &field = description.fields[index];

        if (field.serialTypeId != 0 && !field.name.starts_with(u"Event")) {
            return index;
        }
    }

    return description.fields.size();
}

/** Bind with virtual prefix precedence before local lookup; ordinary public binding is unchanged.
 * @param description Complete borrowed declaration.
 * @param schema Borrowed validated local declarations.
 * @param limits Finite action/name bounds.
 * @param prefixFields Leading fields consumed as metadata without a local target.
 * @return Owned actions or the ordinary binding error.
 */
[[nodiscard]] std::expected<RecordBinding, BindingError> bindRecordWithPrefix(const WireRecordDescription &description,
                                                                              const core::LocalSchema &schema,
                                                                              BindingLimits limits,
                                                                              std::size_t prefixFields);
} // namespace org::ttldtor::qdcxx::protocol::detail
