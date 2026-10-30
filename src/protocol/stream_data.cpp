// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/protocol/stream_data.hpp"

#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"

#include <algorithm>
#include <bit>
#include <limits>
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
            core::RecordContext recordContext;
            std::optional<ResolvedIndexedTimeLayout> indexedLayout;

            if (bound->localRecordId) {
                const auto &local = schema.records()[static_cast<std::size_t>(*bound->localRecordId)];
                count = local.fields.size();
                recordContext = schema.recordContext(*bound->localRecordId);

                if (local.indexedTimeLayout) {
                    // Current validated field IDs equal dense positions; make this resolution explicit.
                    if (const auto *pair = std::get_if<core::IndexedTimePairInt32>(&*local.indexedTimeLayout)) {
                        indexedLayout = ResolvedIndexedTimePairInt32{pair->firstField, pair->secondField};
                    } else {
                        indexedLayout = ResolvedIndexedTimeSingleInt64{
                            std::get<core::IndexedTimeSingleInt64>(*local.indexedTimeLayout).field};
                    }
                }
            }

            plans.push_back({std::move(*bound), count, std::move(recordContext), std::move(indexedLayout)});
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
/** Read a signed big-endian scalar without retaining input or allocating storage.
 * @param input Borrowed remaining field bytes.
 * @param width Exact admitted physical width, one, two or four bytes.
 * @return Sign-extended value, or all available prefix bytes consumed on truncation.
 */
encoding::ReadResult<std::int64_t> readFixedScalar(std::span<const std::byte> input, std::size_t width) noexcept {
    constexpr std::size_t BITS_PER_BYTE = 8;
    const auto consumed = std::min(input.size(), width);

    if (consumed != width) {
        return {0, consumed, encoding::Status::INSUFFICIENT_INPUT};
    }

    std::uint32_t raw = 0;

    for (const auto byte : input.first(width)) {
        raw = (raw << BITS_PER_BYTE) | std::to_integer<std::uint32_t>(byte);
    }

    const auto bits = width * BITS_PER_BYTE;
    const auto signedValue = (raw & (std::uint32_t{1} << (bits - 1))) != 0
                                 ? static_cast<std::int64_t>(raw) - (std::int64_t{1} << bits)
                                 : static_cast<std::int64_t>(raw);

    return {signedValue, consumed, encoding::Status::OK};
}

/** Decode one exact scalar grammar after the caller charges its field attempt.
 * @param input Borrowed remaining payload, never retained.
 * @param type Validated full identity; representation bits are never masked for dispatch.
 * @return Raw value and examined bytes; no allocation, conversion or partial value on failure.
 */
encoding::ReadResult<std::int64_t> readScalar(std::span<const std::byte> input, core::SerialFieldType type) noexcept {
    switch (type) {
    case core::SerialFieldType::VOID_FIELD:
        return {0, 0, encoding::Status::OK};
    case core::SerialFieldType::BYTE:
        return readFixedScalar(input, sizeof(std::int8_t));
    case core::SerialFieldType::SHORT:
        return readFixedScalar(input, sizeof(std::int16_t));
    case core::SerialFieldType::INT:
        return readFixedScalar(input, sizeof(std::int32_t));
    case core::SerialFieldType::UTF_CHAR: {
        const auto value = encoding::readUtfChar(input);

        return {value.value, value.consumed, value.status};
    }
    case core::SerialFieldType::COMPACT_INT:
    case core::SerialFieldType::DECIMAL:
    case core::SerialFieldType::SHORT_STRING:
    case core::SerialFieldType::TIME_SECONDS:
    case core::SerialFieldType::SEQUENCE:
    case core::SerialFieldType::DATE:
    case core::SerialFieldType::LONG:
    case core::SerialFieldType::WIDE_DECIMAL:
    case core::SerialFieldType::TIME_MILLIS: {
        auto value = encoding::readCompactLong(input);

        if (value.status == encoding::Status::OK && type != core::SerialFieldType::LONG &&
            type != core::SerialFieldType::WIDE_DECIMAL && type != core::SerialFieldType::TIME_MILLIS) {
            value.value = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value.value));
        }

        return value;
    }
    default:
        return {0, 0, encoding::Status::INVALID_ENCODING};
    }
}

/** Charged temporary payload; errors retain work but never expose a partial value. */
struct PayloadReadResult {
    std::optional<std::u16string> units{};         ///< Exact UTFString units on success only.
    std::optional<std::vector<std::byte>> bytes{}; ///< Exact opaque body on success only.
    std::size_t consumed = 0;                      ///< Actual examined prefix/body, including an attempted UTF tail.
    std::size_t chargedBytes = 0;                  ///< Admitted body-byte work, never refunded after admission.
    std::size_t chargedUnits = 0;  ///< Admitted UTF-16 reservation work, never refunded after admission.
    std::size_t retainedBytes = 0; ///< Logical successful temporary storage, excluding allocator capacity.
    encoding::Status status = encoding::Status::OK; ///< First error or successful completion.
};

/** Check a UTF-16 append against its admitted reservation without overflowing.
 * @param reservedUnits Admitted maximum logical UTF-16 units.
 * @param currentUnits Units already held by the temporary.
 * @param additionalUnits Complete decoded point width, one or two units.
 * @return Whether the whole point fits; false also rejects an invalid current count.
 */
constexpr bool hasStringUnitRoom(std::size_t reservedUnits, std::size_t currentUnits,
                                 std::size_t additionalUnits) noexcept {
    return currentUnits <= reservedUnits && additionalUnits <= reservedUnits - currentUnits;
}

// Prove capped-reservation boundaries without constructing multi-gigabyte strings.
static_assert(hasStringUnitRoom(std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max() - 1,
                                1));
static_assert(!hasStringUnitRoom(std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max() - 1,
                                 2));
static_assert(!hasStringUnitRoom(std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max(),
                                 1));
static_assert(hasStringUnitRoom(std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max() - 1,
                                1));
static_assert(!hasStringUnitRoom(std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max() - 1,
                                 2));
static_assert(!hasStringUnitRoom(std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max(), 1));
static_assert(!hasStringUnitRoom(0, std::numeric_limits<std::size_t>::max(), 0));

/** Read a new exact payload grammar independently of whether it will be retained.
 * @param input Remaining complete message, borrowed only during this call.
 * @param type Exact byte/object/UTFString identity validated by the binding.
 * @param limits Remaining work/completed-output budgets and per-value limits.
 * @param currentUnits Existing current-record strings that coexist with the temporary.
 * @param currentBytes Existing current-record new payload storage that coexists with the temporary.
 * @return Owned temporary or first error with all examined/admitted work retained.
 */
PayloadReadResult readPayload(std::span<const std::byte> input, core::SerialFieldType type,
                              const StreamDataLimits &limits, std::size_t currentUnits,
                              std::size_t currentBytes) noexcept {
    constexpr std::int64_t NULL_LENGTH = -1;
    constexpr std::int64_t MAX_BODY_LENGTH = std::numeric_limits<std::int32_t>::max();
    constexpr std::size_t UNIT_BYTES = sizeof(char16_t);
    constexpr std::uint32_t MAX_BMP_UNIT = 0xffff;
    constexpr std::uint32_t SUPPLEMENTARY_BASE = 0x10000;
    constexpr std::uint32_t HIGH_SURROGATE = 0xd800;
    constexpr std::uint32_t LOW_SURROGATE = 0xdc00;
    constexpr unsigned int SURROGATE_BITS = 10;
    constexpr std::uint32_t SURROGATE_MASK = 0x3ff;
    PayloadReadResult result;
    const auto encodedAllowance = std::min(limits.fieldPayloadEncodedBytes, limits.payloadEncodedBytes);
    const auto bounded = input.first(std::min(input.size(), encodedAllowance));
    const auto exhausted =
        bounded.size() < input.size() ? encoding::Status::RESOURCE_LIMIT : encoding::Status::INSUFFICIENT_INPUT;
    auto prefix = encoding::readCompactLong(bounded);
    result.consumed = prefix.consumed;

    if (prefix.status != encoding::Status::OK) {
        result.status = exhausted;

        return result;
    }

    if (type == core::SerialFieldType::SERIAL_OBJECT) {
        prefix.value = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(prefix.value));
    }

    const bool text = type == core::SerialFieldType::STRING;
    const auto maximum = text ? MAX_BODY_LENGTH * 4 : MAX_BODY_LENGTH;

    if (prefix.value < NULL_LENGTH || prefix.value > maximum) {
        result.status = encoding::Status::INVALID_LENGTH;

        return result;
    }

    if (prefix.value == NULL_LENGTH) {
        return result;
    }

    // Compare before narrowing: UTFString's wire domain can exceed a 32-bit size_t.
    if (static_cast<std::uint64_t>(prefix.value) > input.size() - result.consumed) {
        result.status = encoding::Status::INVALID_LENGTH;

        return result;
    }

    const auto count = static_cast<std::size_t>(prefix.value);

    if (count > limits.fieldPayloadBytes || count > limits.payloadWorkBytes) {
        result.status = encoding::Status::RESOURCE_LIMIT;

        return result;
    }

    result.chargedBytes = count;
    const auto reservedUnits = text ? std::min(count, static_cast<std::size_t>(MAX_BODY_LENGTH)) : 0;

    if (reservedUnits > limits.fieldString.codeUnits || reservedUnits > limits.fieldUnits) {
        result.status = encoding::Status::RESOURCE_LIMIT;

        return result;
    }

    result.chargedUnits = reservedUnits;

    if (reservedUnits > limits.ownedFieldUnits - currentUnits ||
        reservedUnits > std::numeric_limits<std::size_t>::max() / UNIT_BYTES) {
        result.status = encoding::Status::RESOURCE_LIMIT;

        return result;
    }

    const auto temporaryBytes = text ? reservedUnits * UNIT_BYTES : count;

    if (temporaryBytes > limits.ownedPayloadBytes - currentBytes) {
        result.status = encoding::Status::RESOURCE_LIMIT;

        return result;
    }

    if (!text && count > bounded.size() - result.consumed) {
        result.consumed = bounded.size();
        result.status = exhausted;

        return result;
    }

    try {
        if (!text) {
            const auto body = bounded.subspan(result.consumed, count);
            std::vector<std::byte> value(body.begin(), body.end());
            result.bytes = std::move(value);
            result.consumed += count;
            result.retainedBytes = count;

            return result;
        }

        std::u16string value;
        value.reserve(reservedUnits);
        const auto bodyStart = result.consumed;

        while (result.consumed - bodyStart < count) {
            const auto character = encoding::readUtfChar(bounded.subspan(result.consumed));
            result.consumed += character.consumed;

            if (character.status != encoding::Status::OK) {
                result.status = character.status == encoding::Status::INSUFFICIENT_INPUT ? exhausted : character.status;

                return result;
            }

            if (result.consumed - bodyStart > count) {
                result.status = encoding::Status::INVALID_ENCODING;

                return result;
            }

            const auto point = static_cast<std::uint32_t>(character.value);
            const auto neededUnits = point <= MAX_BMP_UNIT ? std::size_t{1} : std::size_t{2};

            if (!hasStringUnitRoom(reservedUnits, value.size(), neededUnits)) {
                result.status = encoding::Status::RESOURCE_LIMIT;

                return result;
            }

            if (point <= MAX_BMP_UNIT) {
                value.push_back(static_cast<char16_t>(point));
            } else {
                const auto remainder = point - SUPPLEMENTARY_BASE;
                value.push_back(static_cast<char16_t>(HIGH_SURROGATE + (remainder >> SURROGATE_BITS)));
                value.push_back(static_cast<char16_t>(LOW_SURROGATE + (remainder & SURROGATE_MASK)));
            }
        }

        if (result.consumed - bodyStart != count) {
            result.status = encoding::Status::INVALID_ENCODING;

            return result;
        }

        result.retainedBytes = value.size() * UNIT_BYTES;
        result.units = std::move(value);

        return result;
    } catch (const std::bad_alloc &) {
        result.status = encoding::Status::ALLOCATION_FAILURE;
    } catch (const std::length_error &) {
        result.status = encoding::Status::ALLOCATION_FAILURE;
    }

    return result;
}

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
            std::size_t currentPayloadBytes = 0;

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
                std::optional<std::vector<std::byte>> bytes;
                std::size_t retainedPayloadBytes = 0;

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
                } else if (field.type == core::SerialFieldType::BYTE_ARRAY ||
                           field.type == core::SerialFieldType::STRING ||
                           field.type == core::SerialFieldType::CUSTOM_OBJECT ||
                           field.type == core::SerialFieldType::SERIAL_OBJECT) {
                    auto value = readPayload(payload.subspan(result.consumed), field.type, limits, currentFieldUnits,
                                             currentPayloadBytes);
                    result.consumed += value.consumed;
                    limits.payloadEncodedBytes -= value.consumed;
                    result.payloadEncodedBytes += value.consumed;
                    limits.payloadWorkBytes -= value.chargedBytes;
                    result.payloadWorkBytes += value.chargedBytes;
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
                    bytes = std::move(value.bytes);
                    retainedPayloadBytes = value.retainedBytes;
                } else {
                    const auto value = readScalar(payload.subspan(result.consumed), field.type);
                    result.consumed += value.consumed;

                    if (value.status != encoding::Status::OK) {
                        result.status = value.status == encoding::Status::INSUFFICIENT_INPUT
                                            ? StreamDataStatus::TRUNCATED_PAYLOAD
                                            : StreamDataStatus::INVALID_FIELD;

                        return result;
                    }

                    raw = value.value;
                }

                if (field.targetFieldId) {
                    if (typed) {
                        auto &target = typedValues[*field.targetFieldId];
                        // Replacement decoding must fit while the old value is still owned.
                        currentFieldUnits -= target.units ? target.units->size() : 0;
                        currentFieldUnits += units ? units->size() : 0;
                        currentPayloadBytes -= target.bytes ? target.bytes->size() : 0;

                        if (target.type == core::SerialFieldType::STRING && target.units) {
                            currentPayloadBytes -= target.units->size() * sizeof(char16_t);
                        }

                        currentPayloadBytes += retainedPayloadBytes;
                        target = {field.type, raw, std::move(units), std::move(bytes)};
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
                                          std::move(typedValues), plan->context, id.value});
                --limits.emittedRecords;
                limits.emittedValues -= plan->localFieldCount;
                limits.ownedSymbolUnits -= units;
                limits.ownedFieldUnits -= currentFieldUnits;
                result.ownedFieldUnits += currentFieldUnits;
                limits.ownedPayloadBytes -= currentPayloadBytes;
                result.ownedPayloadBytes += currentPayloadBytes;
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
