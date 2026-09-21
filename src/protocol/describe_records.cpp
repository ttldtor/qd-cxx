#include "qd/protocol/describe_records.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>

namespace qd::protocol {
namespace {
/** Call-local bounded walk, publishing only completed descriptions into its owning result. */
class DescriptionReader {
    std::span<const std::byte> payload_;
    DescriptionLimits limits_;
    DescriptionResult result_{{}, DescriptionStatus::OK, DescriptionStage::PAYLOAD, 0, 0, encoding::Status::OK};
    std::size_t fields_{};
    std::size_t nameUnits_{};

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

        if (result_.descriptions.size() == limits_.descriptions) {
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
     */
    DescriptionReader(std::span<const std::byte> payload, DescriptionLimits limits)
        : payload_(payload), limits_(limits) {
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

} // namespace qd::protocol
