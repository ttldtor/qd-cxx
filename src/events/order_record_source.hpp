// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/events/event_mapping.hpp"
#include "qd/events/order_source.hpp"
#include <new>
#include <stdexcept>

namespace org::ttldtor::qdcxx::events::detail {
/** Resolve the existing record-source suffix while charging its complete name inspection.
 * @param name Exact immutable record name, borrowed only through return.
 * @param limits Remaining preparation allowances; attempted name work is debited before parsing.
 * @return Independently owned source or original input/resource/allocation refusal.
 */
inline std::expected<OrderSource, MappingError> prepareRecordOrderSource(std::u16string_view name,
                                                                         MappingLimits &limits) {
    if (name.size() > limits.nameUnits || name.size() > limits.totalNameUnits || name.size() > limits.comparisonWork) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    limits.totalNameUnits -= name.size();
    limits.comparisonWork -= name.size();
    try {
        const auto separator = name.rfind(u'#');

        if (separator == std::u16string_view::npos) {
            return OrderSource::DEFAULT;
        }

        const auto suffix = name.substr(separator + 1);

        if (suffix.empty() || suffix.size() > 4) {
            return std::unexpected(MappingError::INVALID_INPUT);
        }

        return OrderSource::valueOf(suffix);
    } catch (const std::invalid_argument &) {
        return std::unexpected(MappingError::INVALID_INPUT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace org::ttldtor::qdcxx::events::detail
