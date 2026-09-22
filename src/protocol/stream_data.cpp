// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/protocol/stream_data.hpp"

#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"

#include <algorithm>
#include <bit>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::protocol {
StreamDataContext::StreamDataContext(std::vector<StreamRecordPlan> plans,
                                     std::vector<std::vector<core::FieldValue>> defaults,
                                     core::LocalProfile profile) noexcept
    : plans_(std::move(plans)), defaults_(std::move(defaults)), profile_(profile) {
}

std::span<const core::FieldValue> StreamDataContext::defaults(std::int32_t localId) const noexcept {
    if (localId < 0 || static_cast<std::size_t>(localId) >= defaults_.size()) {
        return {};
    }

    return defaults_[static_cast<std::size_t>(localId)];
}

std::expected<StreamDataContext, BindingError>
StreamDataContext::create(const core::LocalSchema &schema, std::span<const WireRecordDescription> descriptions,
                          ContextLimits limits) {
    if (schema.records().size() > limits.schema.records) {
        return std::unexpected(BindingError::RESOURCE_LIMIT);
    }

    for (const auto &record : schema.records()) {
        if (record.fields.size() > limits.schema.fields || record.lookups.size() > limits.schema.lookups ||
            record.name.size() > limits.schema.nameUnits || record.name.size() > limits.schema.totalNameUnits) {
            return std::unexpected(BindingError::RESOURCE_LIMIT);
        }

        limits.schema.fields -= record.fields.size();
        limits.schema.lookups -= record.lookups.size();
        limits.schema.totalNameUnits -= record.name.size();

        for (const auto &key : record.lookups) {
            if (key.name.size() > limits.schema.nameUnits || key.name.size() > limits.schema.totalNameUnits) {
                return std::unexpected(BindingError::RESOURCE_LIMIT);
            }

            limits.schema.totalNameUnits -= key.name.size();
        }
    }

    if (descriptions.size() > limits.descriptions) {
        return std::unexpected(BindingError::RESOURCE_LIMIT);
    }

    try {
        std::vector<std::vector<core::FieldValue>> defaults;

        if (schema.profile() != core::LocalProfile::COMPACT_INT_V1) {
            for (const auto &record : schema.records()) {
                std::vector<core::FieldValue> shape;

                for (const auto &field : record.fields) {
                    shape.push_back({static_cast<core::SerialFieldType>(field.serialTypeId), 0});
                }

                defaults.push_back(std::move(shape));
            }
        }

        std::vector<StreamRecordPlan> plans;

        for (const auto &description : descriptions) {
            for (const auto &existing : plans) {
                if (existing.binding.senderId == description.senderId) {
                    return std::unexpected(BindingError::INVALID_DESCRIPTION);
                }
            }

            auto bound = bindRecord(description, schema, limits.bindings);

            if (!bound) {
                return std::unexpected(bound.error());
            }

            limits.bindings.fields -= description.fields.size();
            limits.bindings.totalNameUnits -= description.name.size();

            for (const auto &field : description.fields) {
                limits.bindings.totalNameUnits -= field.name.size();
            }

            std::size_t count = 0;

            if (bound->localRecordId) {
                count = schema.records()[static_cast<std::size_t>(*bound->localRecordId)].fields.size();
            }

            plans.push_back({std::move(*bound), count});
        }

        return StreamDataContext(std::move(plans), std::move(defaults), schema.profile());
    } catch (const std::bad_alloc &) {
        return std::unexpected(BindingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(BindingError::ALLOCATION_FAILURE);
    }
}

const StreamRecordPlan *StreamDataContext::find(std::int32_t senderId) const noexcept {
    for (const auto &plan : plans_) {
        if (plan.binding.senderId == senderId) {
            return &plan;
        }
    }

    return nullptr;
}

namespace {
/** Decode the measured shared zero-flags wire record layout.
 * @param payload Complete borrowed data payload.
 * @param context Validated immutable bindings.
 * @param limits Finite record, field and output bounds.
 * @return Owned prefix and first-error diagnostics.
 */
StreamDataResult decodeDataPayload(std::span<const std::byte> payload, const StreamDataContext &context,
                                   StreamDataLimits limits) {
    StreamDataResult result;

    if (payload.size() > limits.payloadBytes) {
        result.status = StreamDataStatus::RESOURCE_LIMIT;

        return result;
    }

    encoding::SymbolReader symbols(limits.symbol, 0);
    try {
        while (result.consumed < payload.size()) {
            result.stage = StreamDataStage::RECORD;
            result.errorOffset = result.consumed;

            if (limits.processedRecords == 0) {
                result.status = StreamDataStatus::RESOURCE_LIMIT;

                return result;
            }

            --limits.processedRecords;
            ++result.processedRecords;
            result.stage = StreamDataStage::SYMBOL;
            auto symbol = symbols.read(payload.subspan(result.consumed));
            result.consumed += symbol.consumed;
            result.symbolStatus = symbol.status;

            if (symbol.status != encoding::SymbolStatus::OK) {
                switch (symbol.status) {
                case encoding::SymbolStatus::TRUNCATED_INPUT:
                    result.status = StreamDataStatus::TRUNCATED_PAYLOAD;
                    break;
                case encoding::SymbolStatus::RESOURCE_LIMIT:
                    result.status = StreamDataStatus::RESOURCE_LIMIT;
                    break;
                case encoding::SymbolStatus::ALLOCATION_FAILURE:
                    result.status = StreamDataStatus::ALLOCATION_FAILURE;
                    break;
                default:
                    result.status = StreamDataStatus::INVALID_SYMBOL;
                    break;
                }

                return result;
            }

            result.stage = StreamDataStage::FLAGS;
            result.errorOffset = result.consumed;

            if (symbol.value->eventFlags != 0) {
                result.status = StreamDataStatus::UNSUPPORTED_PROFILE;

                return result;
            }

            result.stage = StreamDataStage::RECORD_ID;
            auto id = encoding::readCompactInt(payload.subspan(result.consumed));
            result.consumed += id.consumed;

            if (id.status != encoding::Status::OK) {
                result.status = StreamDataStatus::TRUNCATED_PAYLOAD;

                return result;
            }

            result.errorOffset = result.consumed;

            if (id.value < 0) {
                result.status = StreamDataStatus::INVALID_ID;

                return result;
            }

            const auto *plan = context.find(id.value);

            if (plan == nullptr) {
                result.status = StreamDataStatus::UNDESCRIBED_ID;

                return result;
            }

            result.stage = StreamDataStage::OUTPUT;
            std::vector<std::int32_t> values;
            std::vector<core::FieldValue> typedValues;
            const bool typed = context.profile() != core::LocalProfile::COMPACT_INT_V1;
            std::size_t currentFieldUnits = 0;

            if (plan->binding.localRecordId) {
                if (limits.emittedRecords == 0 || plan->localFieldCount > limits.emittedValues ||
                    symbol.value->units.size() > limits.ownedSymbolUnits) {
                    result.status = StreamDataStatus::RESOURCE_LIMIT;

                    return result;
                }

                if (typed) {
                    const auto shape = context.defaults(*plan->binding.localRecordId);
                    typedValues.assign(shape.begin(), shape.end());
                } else {
                    values.resize(plan->localFieldCount, 0);
                }
            }

            for (const auto &field : plan->binding.fields) {
                result.stage = StreamDataStage::FIELD;
                result.errorOffset = result.consumed;

                if (limits.wireFields == 0) {
                    result.status = StreamDataStatus::RESOURCE_LIMIT;

                    return result;
                }

                --limits.wireFields;
                ++result.wireFields;
                std::int64_t raw = 0;
                std::optional<std::u16string> units;

                if (field.type == core::SerialFieldType::UTF_CHAR_ARRAY) {
                    const encoding::CharArrayLimits allowance{
                        std::min(limits.fieldString.encodedBytes, limits.fieldBytes),
                        std::min({limits.fieldString.codeUnits, limits.fieldUnits,
                                  limits.ownedFieldUnits - currentFieldUnits})};
                    auto value = encoding::readCharArray(payload.subspan(result.consumed), allowance,
                                                         encoding::CharArrayBoundary::MESSAGE);
                    result.consumed += value.consumed;
                    limits.fieldBytes -= value.consumed;
                    result.fieldBytes += value.consumed;
                    limits.fieldUnits -= value.chargedUnits;
                    result.fieldUnits += value.chargedUnits;

                    if (value.status != encoding::Status::OK) {
                        switch (value.status) {
                        case encoding::Status::INSUFFICIENT_INPUT:
                            result.status = StreamDataStatus::TRUNCATED_PAYLOAD;
                            break;
                        case encoding::Status::RESOURCE_LIMIT:
                            result.status = StreamDataStatus::RESOURCE_LIMIT;
                            break;
                        case encoding::Status::ALLOCATION_FAILURE:
                            result.status = StreamDataStatus::ALLOCATION_FAILURE;
                            break;
                        default:
                            result.status = StreamDataStatus::INVALID_FIELD;
                            break;
                        }

                        return result;
                    }

                    units = std::move(value.units);
                } else if (field.type == core::SerialFieldType::UTF_CHAR) {
                    const auto value = encoding::readUtfChar(payload.subspan(result.consumed));
                    result.consumed += value.consumed;

                    if (value.status != encoding::Status::OK) {
                        result.status = value.status == encoding::Status::INSUFFICIENT_INPUT
                                            ? StreamDataStatus::TRUNCATED_PAYLOAD
                                            : StreamDataStatus::INVALID_FIELD;

                        return result;
                    }

                    raw = value.value;
                } else {
                    const auto value = encoding::readCompactLong(payload.subspan(result.consumed));
                    result.consumed += value.consumed;

                    if (value.status != encoding::Status::OK) {
                        result.status = StreamDataStatus::TRUNCATED_PAYLOAD;

                        return result;
                    }

                    raw = field.type == core::SerialFieldType::WIDE_DECIMAL
                              ? value.value
                              : std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value.value));
                }

                if (field.targetFieldId) {
                    if (typed) {
                        auto &target = typedValues[*field.targetFieldId];
                        // Replacement decoding must fit while the old value is still owned.
                        currentFieldUnits -= target.units ? target.units->size() : 0;
                        currentFieldUnits += units ? units->size() : 0;
                        target = {field.type, raw, std::move(units)};
                    } else {
                        values[*field.targetFieldId] = static_cast<std::int32_t>(raw);
                    }
                }
            }

            result.stage = StreamDataStage::OUTPUT;
            result.errorOffset = result.consumed;

            if (plan->binding.localRecordId) {
                const auto units = symbol.value->units.size();
                result.records.push_back({*plan->binding.localRecordId, std::move(*symbol.value), std::move(values),
                                          std::move(typedValues)});
                --limits.emittedRecords;
                limits.emittedValues -= plan->localFieldCount;
                limits.ownedSymbolUnits -= units;
                limits.ownedFieldUnits -= currentFieldUnits;
                result.ownedFieldUnits += currentFieldUnits;
            }
        }

        result.stage = StreamDataStage::END;
        result.errorOffset = result.consumed;
    } catch (const std::bad_alloc &) {
        result.status = StreamDataStatus::ALLOCATION_FAILURE;
    } catch (const std::length_error &) {
        result.status = StreamDataStatus::ALLOCATION_FAILURE;
    }

    return result;
}
} // namespace

StreamDataResult decodeStreamData(std::span<const std::byte> payload, const StreamDataContext &context,
                                  StreamDataLimits limits) {
    return decodeDataPayload(payload, context, limits);
}

StreamDataResult decodeRawData(std::span<const std::byte> payload, const StreamDataContext &context,
                               StreamDataLimits limits) {
    return decodeDataPayload(payload, context, limits);
}
} // namespace org::ttldtor::qdcxx::protocol
