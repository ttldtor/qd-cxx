// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/protocol/describe_records.hpp"
#include "qd/encoding/utf_char.hpp"
#include "session_budget.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::protocol {
namespace {
/** Allocation-free UTFString measurement using the same compact/UTFChar primitives as the codec. */
struct MeasuredName {
    std::size_t units = 0;                          ///< Complete decoded UTF-16 units before an error.
    std::size_t consumed = 0;                       ///< Exact primitive examination including an invalid full tail.
    encoding::Status status = encoding::Status::OK; ///< Codec failure or complete success.
};

/** Count name units before their owning decode, preserving the codec's length/error grammar.
 * @param input Borrowed remaining payload.
 * @param limits Per-name and remaining aggregate bounds.
 * @return Allocation-free measurement; no string or input view escapes.
 */
MeasuredName measureName(std::span<const std::byte> input, encoding::UtfStringLimits limits) noexcept {
    constexpr std::int64_t MAX_UTF_BODY_BYTES = 8589934588LL;
    constexpr std::int64_t NULL_LENGTH = -1;
    constexpr std::uint32_t SUPPLEMENTARY_MIN = 0x10000;
    constexpr std::size_t SURROGATE_PAIR_UNITS = 2;
    const auto prefix = encoding::readCompactLong(input);
    MeasuredName result{0, prefix.consumed, prefix.status};

    if (prefix.status != encoding::Status::OK) {
        return result;
    }

    if (prefix.value < NULL_LENGTH || prefix.value > MAX_UTF_BODY_BYTES) {
        result.status = encoding::Status::INVALID_LENGTH;
    } else if (prefix.value != NULL_LENGTH && static_cast<std::uint64_t>(prefix.value) > limits.bodyBytes) {
        result.status = encoding::Status::RESOURCE_LIMIT;
    } else if (prefix.value != NULL_LENGTH) {
        auto remaining = prefix.value;

        while (remaining > 0) {
            const auto character = encoding::readUtfChar(input.subspan(result.consumed));
            result.consumed += character.consumed;
            result.status = character.status;

            if (character.status != encoding::Status::OK) {
                return result;
            }

            const std::size_t units =
                static_cast<std::uint32_t>(character.value) >= SUPPLEMENTARY_MIN ? SURROGATE_PAIR_UNITS : 1;

            if (units > limits.codeUnits - result.units) {
                result.status = encoding::Status::RESOURCE_LIMIT;

                return result;
            }

            result.units += units;
            remaining -= static_cast<std::int64_t>(character.consumed);
        }

        if (remaining < 0) {
            result.status = encoding::Status::INVALID_ENCODING;
        }
    }

    return result;
}

/** Call-local bounded walk, publishing only completed descriptions into its owning result. */
class DescriptionReader {
    std::span<const std::byte> payload_;
    DescriptionLimits limits_;
    DescriptionResult result_{{}, DescriptionStatus::OK, DescriptionStage::PAYLOAD, 0, 0, encoding::Status::OK};
    std::size_t fields_{};
    std::size_t nameUnits_{};
    detail::SessionBudget *budget_ = nullptr; ///< Optional internal admission, never a user callback.

    /** Set a checkpoint before the next operation, clearing any prior primitive cause.
     * @param stage Next operation or validation checkpoint.
     */
    void checkpoint(DescriptionStage stage) noexcept {
        result_.stage = stage;
        result_.errorOffset = result_.consumed;
        result_.primitiveStatus = encoding::Status::OK;
    }

    /** Record a failure without changing the completed prefix or measured positions.
     * @param status First failing category.
     * @return Always false, allowing immediate propagation from parsing helpers.
     */
    bool fail(DescriptionStatus status) noexcept {
        result_.status = status;

        return false;
    }

    /** Read one bounded compact int with the inherited low-32-bit semantics.
     * @param value Receives a value only on success.
     * @param stage Primitive identity for diagnostics.
     * @return True on complete read, false with TRUNCATED_PAYLOAD otherwise.
     */
    bool integer(std::int32_t &value, DescriptionStage stage) {
        checkpoint(stage);
        const auto decoded = encoding::readCompactInt(payload_.subspan(result_.consumed));
        result_.consumed += decoded.consumed;
        result_.primitiveStatus = decoded.status;

        if (decoded.status != encoding::Status::OK) {
            return fail(DescriptionStatus::TRUNCATED_PAYLOAD);
        }

        value = decoded.value;

        return true;
    }

    /** Read a name, accounting units before subsequent structural validation.
     * @param value Receives owned units or null only on success.
     * @param stage Name identity for diagnostics.
     * @return True on primitive success; false on bounded primitive error.
     */
    bool name(std::optional<std::u16string> &value, DescriptionStage stage) {
        checkpoint(stage);
        const auto units = std::min(limits_.name.codeUnits, limits_.totalNameUnits - nameUnits_);

        if (budget_ != nullptr) {
            const auto measured = measureName(payload_.subspan(result_.consumed), {limits_.name.bodyBytes, units});

            if (measured.status != encoding::Status::OK) {
                result_.consumed += measured.consumed;
                result_.primitiveStatus = measured.status;

                return fail(measured.status == encoding::Status::INSUFFICIENT_INPUT
                                ? DescriptionStatus::TRUNCATED_PAYLOAD
                            : measured.status == encoding::Status::RESOURCE_LIMIT ? DescriptionStatus::RESOURCE_LIMIT
                                                                                  : DescriptionStatus::INVALID_STRING);
            }

            if (!budget_->admit(measured.units)) {
                return fail(DescriptionStatus::RESOURCE_LIMIT);
            }
        }

        auto decoded = encoding::readUtfString(payload_.subspan(result_.consumed), {limits_.name.bodyBytes, units});
        result_.consumed += decoded.consumed;
        result_.primitiveStatus = decoded.status;

        switch (decoded.status) {
        case encoding::Status::OK:

            if (decoded.value) {
                nameUnits_ += decoded.value->size();
            }

            value = std::move(decoded.value);

            return true;
        case encoding::Status::INSUFFICIENT_INPUT:
            return fail(DescriptionStatus::TRUNCATED_PAYLOAD);
        case encoding::Status::RESOURCE_LIMIT:
            return fail(DescriptionStatus::RESOURCE_LIMIT);
        default:
            return fail(DescriptionStatus::INVALID_STRING);
        }
    }

    /** Read one record, holding its pending fields separately until complete.
     * @return True only after successful publication; false preserves prior records.
     */
    bool record() {
        checkpoint(DescriptionStage::DESCRIPTION);

        if (result_.descriptions.size() == limits_.descriptions || (budget_ != nullptr && !budget_->admit(1))) {
            return fail(DescriptionStatus::RESOURCE_LIMIT);
        }

        std::int32_t id{}, count{};
        std::optional<std::u16string> recordName;

        if (!integer(id, DescriptionStage::SENDER_ID) || !name(recordName, DescriptionStage::RECORD_NAME) ||
            !integer(count, DescriptionStage::FIELD_COUNT)) {
            return false;
        }

        checkpoint(DescriptionStage::RECORD_HEADER);

        if (id < 0 || !recordName || recordName->empty() || count < 0) {
            return fail(DescriptionStatus::INVALID_RECORD);
        }

        const auto fieldCount = static_cast<std::size_t>(count);

        if (fieldCount > limits_.fieldsPerDescription || fieldCount > limits_.totalFields - fields_) {
            return fail(DescriptionStatus::RESOURCE_LIMIT);
        }

        fields_ += fieldCount;

        if (budget_ != nullptr && !budget_->admit(fieldCount)) {
            return fail(DescriptionStatus::RESOURCE_LIMIT);
        }

        WireRecordDescription pending{id, std::move(*recordName), {}};

        for (std::size_t i = 0; i < fieldCount; ++i) {
            std::optional<std::u16string> fieldName;
            std::int32_t type{};

            if (!name(fieldName, DescriptionStage::FIELD_NAME) || !integer(type, DescriptionStage::FIELD_TYPE)) {
                return false;
            }

            checkpoint(DescriptionStage::FIELD_HEADER);

            if (!fieldName || fieldName->empty() || type < 0 || type > MAX_WIRE_SERIAL_TYPE_ID) {
                return fail(DescriptionStatus::INVALID_FIELD);
            }

            pending.fields.push_back({std::move(*fieldName), type});
        }

        checkpoint(DescriptionStage::DESCRIPTION);
        result_.descriptions.push_back(std::move(pending));

        return true;
    }

    public:
    /** Store call-local views and budgets without allocating.
     * @param payload Borrowed complete payload.
     * @param limits Logical element budgets.
     * @param budget Optional internal peak/work ledger; absent preserves ordinary parser behavior.
     */
    DescriptionReader(std::span<const std::byte> payload, DescriptionLimits limits,
                      detail::SessionBudget *budget = nullptr)
        : payload_(payload), limits_(limits), budget_(budget) {
    }

    /** Execute once, catching allocation errors before returning the owned completed prefix.
     * @return Result with exact committed examination positions and first error.
     */
    DescriptionResult read() {
        try {
            if (payload_.size() > limits_.payloadBytes) {
                fail(DescriptionStatus::RESOURCE_LIMIT);

                return std::move(result_);
            }

            while (result_.consumed < payload_.size()) {
                if (!record()) {
                    return std::move(result_);
                }
            }

            checkpoint(DescriptionStage::END);
        } catch (const std::bad_alloc &) {
            fail(DescriptionStatus::ALLOCATION_FAILURE);
        } catch (const std::length_error &) {
            fail(DescriptionStatus::ALLOCATION_FAILURE);
        }

        return std::move(result_);
    }
};
} // namespace

DescriptionResult parseDescribeRecords(std::span<const std::byte> payload, DescriptionLimits limits) {
    return DescriptionReader(payload, limits).read();
}

DescriptionResult detail::parseSessionDescriptions(std::span<const std::byte> payload, DescriptionLimits limits,
                                                   SessionBudget &budget) {
    return DescriptionReader(payload, limits, &budget).read();
}

} // namespace org::ttldtor::qdcxx::protocol
