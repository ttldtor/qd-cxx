// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/events/event_mapping.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
/** Charge logical work without overflowing a cumulative sum.
 * @param count Requested count.
 * @param remaining Remaining allowance, decreased only on success.
 * @return Whether the complete charge fits.
 */
bool charge(std::size_t count, std::size_t &remaining) noexcept {
    if (count > remaining) {
        return false;
    }

    remaining -= count;

    return true;
}

/** Validate one borrowed lookup name with the same UTF-16 policy as local schema names.
 * @param name Nonempty exact UTF-16 key to inspect.
 * @return Whether every surrogate forms a complete pair.
 */
bool validKey(std::u16string_view name) noexcept {
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
        if (!charge(request.keys.size(), limits.keys) || !charge(request.types.size(), limits.typeAlternatives)) {
            return std::unexpected(MappingError::RESOURCE_LIMIT);
        }
    }

    for (const auto &lookup : record.lookups) {
        if (lookup.name.size() > limits.nameUnits || !charge(lookup.name.size(), limits.totalNameUnits)) {
            return std::unexpected(MappingError::RESOURCE_LIMIT);
        }
    }

    for (const auto &request : requests) {
        for (const auto key : request.keys) {
            if (key.size() > limits.nameUnits || !charge(key.size(), limits.totalNameUnits)) {
                return std::unexpected(MappingError::RESOURCE_LIMIT);
            }
        }
    }

    for (const auto &request : requests) {
        if (request.keys.empty() || request.types.empty()) {
            return std::unexpected(MappingError::INVALID_INPUT);
        }

        for (const auto key : request.keys) {
            if (!validKey(key)) {
                return std::unexpected(MappingError::INVALID_INPUT);
            }
        }

        for (const auto type : request.types) {
            if (!core::supportsSerialType(std::to_underlying(type), core::LocalProfile::EXTENDED_FIELDS_V1)) {
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
                    if (!charge(std::max(key.size(), lookup.name.size()), limits.comparisonWork) ||
                        !charge(1, limits.comparisonWork)) {
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
                    if (!charge(1, limits.comparisonWork)) {
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
    if (!context_.matches(record.context)) {
        return std::unexpected(MappingError::WRONG_CONTEXT);
    }

    if (record.values.size() != shape_.size()) {
        return std::unexpected(MappingError::INVALID_VALUES);
    }

    for (std::size_t i = 0; i < shape_.size(); ++i) {
        if (record.values[i].type != shape_[i] || !core::validFieldValue(record.values[i])) {
            return std::unexpected(MappingError::INVALID_VALUES);
        }
    }

    return {};
}

} // namespace org::ttldtor::qdcxx::events
