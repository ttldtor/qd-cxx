#include "qd/protocol/stream_data.hpp"

#include "qd/encoding/compact.hpp"

#include <bit>
#include <new>
#include <stdexcept>
#include <utility>

namespace qd::protocol {
StreamDataContext::StreamDataContext(std::vector<StreamRecordPlan> plans) noexcept : plans_(std::move(plans)) {
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

        return StreamDataContext(std::move(plans));
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

StreamDataResult decodeStreamData(std::span<const std::byte> payload, const StreamDataContext &context,
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

            if (plan->binding.localRecordId) {
                if (limits.emittedRecords == 0 || plan->localFieldCount > limits.emittedValues ||
                    symbol.value->units.size() > limits.ownedSymbolUnits) {
                    result.status = StreamDataStatus::RESOURCE_LIMIT;

                    return result;
                }

                values.resize(plan->localFieldCount, 0);
            }

            for (const auto &field : plan->binding.fields) {
                result.stage = StreamDataStage::FIELD;
                result.errorOffset = result.consumed;

                if (limits.wireFields == 0) {
                    result.status = StreamDataStatus::RESOURCE_LIMIT;

                    return result;
                }

                --limits.wireFields;
                const auto value = encoding::readCompactLong(payload.subspan(result.consumed));
                result.consumed += value.consumed;

                if (value.status != encoding::Status::OK) {
                    result.status = StreamDataStatus::TRUNCATED_PAYLOAD;

                    return result;
                }

                if (field.targetFieldId) {
                    // Java FLD_INT stores the signed low 32 bits of the compact-long value.
                    values[*field.targetFieldId] = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value.value));
                }
            }

            result.stage = StreamDataStage::OUTPUT;
            result.errorOffset = result.consumed;

            if (plan->binding.localRecordId) {
                const auto units = symbol.value->units.size();
                result.records.push_back({*plan->binding.localRecordId, std::move(*symbol.value), std::move(values)});
                --limits.emittedRecords;
                limits.emittedValues -= plan->localFieldCount;
                limits.ownedSymbolUnits -= units;
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
} // namespace qd::protocol
