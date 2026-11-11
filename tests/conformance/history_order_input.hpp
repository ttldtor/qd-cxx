// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/local_schema.hpp"
#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

namespace history_test {
/** Actual default physical Order declarations and zero values, including VOID and wide-value tails.
 * @return Owning peer schema/fields; invalid fixture preparation throws before any peer starts.
 */
inline auto orderInput() {
    namespace q = org::ttldtor::qdcxx;
    using Type = q::core::SerialFieldType;
    constexpr std::array names{u"Void", u"Index",         u"Time",  u"Sequence",       u"Price", u"Price$VoidTail",
                               u"Size", u"Size$VoidTail", u"Count", u"Count$VoidTail", u"Flags", u"MMID"};
    constexpr std::array types{Type::VOID_FIELD,   Type::COMPACT_INT, Type::TIME_SECONDS, Type::SEQUENCE,
                               Type::WIDE_DECIMAL, Type::VOID_FIELD,  Type::WIDE_DECIMAL, Type::VOID_FIELD,
                               Type::WIDE_DECIMAL, Type::VOID_FIELD,  Type::COMPACT_INT,  Type::SHORT_STRING};
    q::core::LocalRecord physical{0, u"Order", false, {}, {}};
    std::vector<q::core::FieldValue> fields;

    for (std::size_t index = 0; index < names.size(); ++index) {
        physical.fields.push_back({static_cast<std::int32_t>(types[index])});
        physical.lookups.push_back({names[index], index});
        fields.push_back({types[index], 0});
    }

    const std::array records{std::move(physical)};
    auto schema =
        q::core::LocalSchema::create(records, {16, 128, 256, 128, 16384}, q::core::LocalProfile::BINARY_FIELDS_V1);

    if (!schema)
        throw std::runtime_error("physical Order fixture preparation failed");

    return std::pair{std::move(*schema), std::move(fields)};
}
} // namespace history_test
