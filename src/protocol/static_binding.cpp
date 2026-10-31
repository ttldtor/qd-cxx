// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/protocol/static_binding.hpp"
#include "field_time_binding.hpp"
#include "live_decode.hpp"

#include "qd/core/serial_field_type.hpp"

#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace org::ttldtor::qdcxx::protocol {

std::expected<RecordBinding, BindingError> bindRecord(const WireRecordDescription &description,
                                                      const core::LocalSchema &schema, BindingLimits limits) {
    return detail::bindRecordWithPrefix(description, schema, limits, 0);
}

namespace {
/** Shared exact-type binding algorithm, independent of the action-container allocator.
 * @tparam Description Complete ordinary or explicit-resource description.
 * @tparam Plan Corresponding owned binding type.
 * @param description Borrowed complete sender description.
 * @param schema Immutable local schema.
 * @param limits Finite name/action allowances.
 * @param prefixFields Physical prefix which takes precedence over local field lookup.
 * @param plan Empty action owner carrying the selected allocator.
 * @return Complete owned plan or ordinary first binding failure.
 */
template <typename Description, typename Plan>
std::expected<Plan, BindingError> bindDescription(const Description &description, const core::LocalSchema &schema,
                                                  BindingLimits limits, std::size_t prefixFields, Plan plan) {
    if (description.fields.size() > limits.fields) {
        return std::unexpected(BindingError::RESOURCE_LIMIT);
    }

    if (description.senderId < 0 || description.name.empty()) {
        return std::unexpected(BindingError::INVALID_DESCRIPTION);
    }

    if (description.name.size() > limits.nameUnits || description.name.size() > limits.totalNameUnits) {
        return std::unexpected(BindingError::RESOURCE_LIMIT);
    }

    limits.totalNameUnits -= description.name.size();

    for (const auto &field : description.fields) {
        if (field.name.empty() || field.serialTypeId < 0 || field.serialTypeId > MAX_WIRE_SERIAL_TYPE_ID) {
            return std::unexpected(BindingError::INVALID_DESCRIPTION);
        }

        if (field.name.size() > limits.nameUnits || field.name.size() > limits.totalNameUnits) {
            return std::unexpected(BindingError::RESOURCE_LIMIT);
        }

        limits.totalNameUnits -= field.name.size();

        if (!core::supportsSerialType(field.serialTypeId, schema.profile())) {
            return std::unexpected(BindingError::UNSUPPORTED_PROFILE);
        }
    }

    const core::LocalRecord *local = nullptr;

    for (const auto &record : schema.records()) {
        if (std::u16string_view(record.name) == std::u16string_view(description.name)) {
            local = &record;
            break;
        }
    }

    try {
        if constexpr (std::is_same_v<Plan, detail::PmrRecordBinding>) {
            plan.fields.reserve(description.fields.size());
        }

        if (local != nullptr) {
            plan.localRecordId = local->id;
        }

        std::size_t position = 0;

        for (const auto &field : description.fields) {
            FieldBinding action{std::nullopt, static_cast<core::SerialFieldType>(field.serialTypeId)};

            if (local != nullptr && position >= prefixFields) {
                for (const auto &key : local->lookups) {
                    if (std::u16string_view(key.name) == std::u16string_view(field.name)) {
                        if (local->fields[key.fieldId].serialTypeId != field.serialTypeId) {
                            return std::unexpected(BindingError::UNSUPPORTED_PROFILE);
                        }

                        action.targetFieldId = key.fieldId;
                        break;
                    }
                }
            }

            plan.fields.push_back(action);
            ++position;
        }

        return plan;
    } catch (const std::bad_alloc &) {
        return std::unexpected(BindingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(BindingError::ALLOCATION_FAILURE);
    }
}

} // namespace

std::expected<RecordBinding, BindingError> detail::bindRecordWithPrefix(const WireRecordDescription &description,
                                                                        const core::LocalSchema &schema,
                                                                        BindingLimits limits,
                                                                        std::size_t prefixFields) {
    return bindDescription(description, schema, limits, prefixFields, RecordBinding{description.senderId, {}, {}});
}

std::expected<detail::PmrRecordBinding, BindingError>
detail::bindPmrRecord(const PmrWireRecordDescription &description, const core::LocalSchema &schema,
                      BindingLimits limits, std::size_t prefixFields, std::pmr::memory_resource &resource) {
    return bindDescription(description, schema, limits, prefixFields, PmrRecordBinding{description.senderId, resource});
}
} // namespace org::ttldtor::qdcxx::protocol
