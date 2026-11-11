// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/event_mapping.hpp"
#include "qd/events/order_source.hpp"
#include <new>
#include <stdexcept>

namespace org::ttldtor::qdcxx::events::detail {
/** Resolve an exact market family and its independent source in a bounded preparation.
 * @param schema Validated source borrowed through return.
 * @param localId Explicit record ID.
 * @param family Expected base record name, without a source suffix.
 * @param limits Remaining mapping allowances, debited for name inspection.
 * @return Validated source ID, or an original preparation refusal.
 */
inline std::expected<std::int32_t, MappingError> prepareSourcedMarketRecord(const core::LocalSchema &schema,
                                                                            std::int32_t localId,
                                                                            std::u16string_view family,
                                                                            MappingLimits &limits) {
    if (!schema.recordContext(localId).valid()) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    const auto &name = schema.records()[static_cast<std::size_t>(localId)].name;

    if (name.size() > limits.nameUnits || name.size() > limits.totalNameUnits || name.size() > limits.comparisonWork) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    limits.totalNameUnits -= name.size();
    limits.comparisonWork -= name.size();

    if (!name.starts_with(family)) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    if (name.size() == family.size()) {
        return OrderSource::DEFAULT.id();
    }

    constexpr std::size_t MAX_SOURCE_UNITS = 4;
    const auto separator = family.size();
    const auto suffix = std::u16string_view(name).substr(separator + 1);

    if (name[separator] != u'#' || suffix.empty() || suffix.size() > MAX_SOURCE_UNITS) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    try {
        return OrderSource::valueOf(suffix).id();
    } catch (const std::invalid_argument &) {
        return std::unexpected(MappingError::INVALID_INPUT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace org::ttldtor::qdcxx::events::detail
