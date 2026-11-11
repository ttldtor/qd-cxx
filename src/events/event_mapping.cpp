// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/events/event_mapping.hpp"

#include "../core/schema_validation.hpp"

#include <algorithm>
#include <bit>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
/** Check complete local values with one canonical validator for their concrete representation.
 * @tparam Record RecordValueView or BorrowedRecordValueView.
 * @tparam Validator Internal nonthrowing canonical field validator.
 * @param context Expected immutable record provenance.
 * @param shape Expected complete exact field types.
 * @param record Borrowed context and values, retained only during validation.
 * @param validator Canonical validator for the selected owning or borrowed field type.
 * @return Success or provenance/complete-shape error without allocating or copying payloads.
 */
template <typename Record, typename Validator>
std::expected<void, MappingError> validateValues(const core::RecordContext &context,
                                                 std::span<const core::SerialFieldType> shape, Record record,
                                                 Validator validator) noexcept {
    if (!context.matches(record.context)) {
        return std::unexpected(MappingError::WRONG_CONTEXT);
    }

    if (record.values.size() != shape.size()) {
        return std::unexpected(MappingError::INVALID_VALUES);
    }

    for (std::size_t i = 0; i < shape.size(); ++i) {
        if (record.values[i].type != shape[i] || !validator(record.values[i], i)) {
            return std::unexpected(MappingError::INVALID_VALUES);
        }
    }

    return {};
}

/** Validate complete indexed-removal words without broadening ordinary UTF_CHAR values.
 * @tparam Record Owning or borrowed complete field representation.
 * @tparam Validator Its unchanged canonical field validator.
 * @param context Exact expected provenance.
 * @param shape Entire retained source shape.
 * @param record Complete caller fields, retained only during this call.
 * @param layout Actual source-time layout copied by the prepared mapper.
 * @param rawTime Full signed removal identity.
 * @param canonical Unchanged ordinary field validator.
 * @return Complete validation or typed refusal, without allocating or modifying fields.
 */
template <class Record, class Validator>
std::expected<void, MappingError>
validateRemovalValues(const core::RecordContext &context, std::span<const core::SerialFieldType> shape, Record record,
                      const core::IndexedTimeLayout &layout, std::int64_t rawTime, Validator canonical) noexcept {
    if (!context.matches(record.context)) {
        return std::unexpected(MappingError::WRONG_CONTEXT);
    }

    if (record.values.size() != shape.size()) {
        return std::unexpected(MappingError::INVALID_VALUES);
    }

    const auto bits = static_cast<std::uint64_t>(rawTime);
    const auto high = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32));
    const auto low = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits));
    const auto *pair = std::get_if<core::IndexedTimePairInt32>(&layout);

    if (pair) {
        if (pair->firstField >= shape.size() || pair->secondField >= shape.size() ||
            pair->firstField == pair->secondField || record.values[pair->firstField].raw != high ||
            record.values[pair->secondField].raw != low) {
            return std::unexpected(MappingError::INVALID_VALUES);
        }
    } else if (const auto *single = std::get_if<core::IndexedTimeSingleInt64>(&layout)) {
        if (single->field >= shape.size() || record.values[single->field].raw != rawTime) {
            return std::unexpected(MappingError::INVALID_VALUES);
        }
    } else {
        const auto field = std::get<core::IndexedTimeLowInt32>(layout).field;

        if (field >= shape.size() || record.values[field].raw != low) {
            return std::unexpected(MappingError::INVALID_VALUES);
        }
    }

    return validateValues(context, shape, record, [&](const auto &field, std::size_t index) {
        if (canonical(field)) {
            return true;
        }

        return pair && (index == pair->firstField || index == pair->secondField) &&
               field.type == core::SerialFieldType::UTF_CHAR && !field.units && !field.bytes;
    });
}
} // namespace

EventMapping::EventMapping(core::RecordContext context, std::vector<core::SerialFieldType> shape,
                           std::vector<std::optional<std::size_t>> selections) noexcept
    : context_(std::move(context)), shape_(std::move(shape)), selections_(std::move(selections)) {
}

const core::RecordContext &EventMapping::context() const noexcept {
    return context_;
}

std::span<const std::optional<std::size_t>> EventMapping::selections() const noexcept {
    return selections_;
}

std::expected<EventMapping, MappingError> EventMapping::prepare(const core::LocalSchema &schema, std::int32_t localId,
                                                                std::span<const FieldRequest> requests,
                                                                MappingLimits limits) {
    auto context = schema.recordContext(localId);

    if (!context.valid()) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    const auto &record = schema.records()[static_cast<std::size_t>(localId)];

    if (record.fields.size() > limits.fields || record.lookups.size() > limits.lookups ||
        requests.size() > limits.selections) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    for (const auto &request : requests) {
        if (!core::detail::chargeBudget(request.keys.size(), limits.keys) ||
            !core::detail::chargeBudget(request.types.size(), limits.typeAlternatives)) {
            return std::unexpected(MappingError::RESOURCE_LIMIT);
        }
    }

    for (const auto &lookup : record.lookups) {
        if (lookup.name.size() > limits.nameUnits ||
            !core::detail::chargeBudget(lookup.name.size(), limits.totalNameUnits)) {
            return std::unexpected(MappingError::RESOURCE_LIMIT);
        }
    }

    for (const auto &request : requests) {
        for (const auto key : request.keys) {
            if (key.size() > limits.nameUnits || !core::detail::chargeBudget(key.size(), limits.totalNameUnits)) {
                return std::unexpected(MappingError::RESOURCE_LIMIT);
            }
        }
    }

    for (const auto &request : requests) {
        if (request.keys.empty() || request.types.empty()) {
            return std::unexpected(MappingError::INVALID_INPUT);
        }

        for (const auto key : request.keys) {
            if (!core::detail::validSchemaKey(key)) {
                return std::unexpected(MappingError::INVALID_INPUT);
            }
        }

        for (const auto type : request.types) {
            if (!core::supportsSerialType(std::to_underlying(type), core::LocalProfile::BINARY_FIELDS_V1)) {
                return std::unexpected(MappingError::INVALID_INPUT);
            }
        }
    }

    try {
        std::vector<core::SerialFieldType> shape;
        shape.reserve(record.fields.size());

        for (const auto &field : record.fields) {
            shape.push_back(static_cast<core::SerialFieldType>(field.serialTypeId));
        }

        std::vector<std::optional<std::size_t>> selections;
        selections.reserve(requests.size());

        for (const auto &request : requests) {
            std::optional<std::size_t> selected;

            for (const auto key : request.keys) {
                for (const auto &lookup : record.lookups) {
                    if (!core::detail::chargeBudget(std::max(key.size(), lookup.name.size()), limits.comparisonWork) ||
                        !core::detail::chargeBudget(1, limits.comparisonWork)) {
                        return std::unexpected(MappingError::RESOURCE_LIMIT);
                    }

                    if (key == lookup.name) {
                        if (selected && *selected != lookup.fieldId) {
                            return std::unexpected(MappingError::AMBIGUOUS_FIELD);
                        }

                        selected = lookup.fieldId;
                    }
                }
            }

            if (!selected && request.required) {
                return std::unexpected(MappingError::MISSING_REQUIRED);
            }

            if (selected) {
                bool compatible = false;

                for (const auto type : request.types) {
                    if (!core::detail::chargeBudget(1, limits.comparisonWork)) {
                        return std::unexpected(MappingError::RESOURCE_LIMIT);
                    }

                    compatible = (type == shape[*selected]) || compatible;
                }

                if (!compatible) {
                    return std::unexpected(MappingError::INCOMPATIBLE_TYPE);
                }
            }

            selections.push_back(selected);
        }

        return EventMapping(std::move(context), std::move(shape), std::move(selections));
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}

std::expected<void, MappingError> EventMapping::validate(RecordValueView record) const noexcept {
    return validateValues(context_, shape_, record, [](const auto &field, std::size_t) {
        return core::validFieldValue(field);
    });
}

std::expected<void, MappingError> EventMapping::validateView(BorrowedRecordValueView record) const noexcept {
    return validateValues(context_, shape_, record, [](const auto &field, std::size_t) {
        return core::validFieldValueView(field);
    });
}

std::expected<void, MappingError> EventMapping::validateIndexedRemoval(RecordValueView record,
                                                                       const core::IndexedTimeLayout &layout,
                                                                       std::int64_t rawTime) const noexcept {
    return validateRemovalValues(context_, shape_, record, layout, rawTime, [](const auto &field) {
        return core::validFieldValue(field);
    });
}

std::expected<void, MappingError> EventMapping::validateIndexedRemoval(BorrowedRecordValueView record,
                                                                       const core::IndexedTimeLayout &layout,
                                                                       std::int64_t rawTime) const noexcept {
    return validateRemovalValues(context_, shape_, record, layout, rawTime, [](const auto &field) {
        return core::validFieldValueView(field);
    });
}
} // namespace org::ttldtor::qdcxx::events
