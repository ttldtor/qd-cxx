// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/stream_data.hpp"
#include <memory_resource>
#include <utility>

namespace org::ttldtor::qdcxx::protocol::detail {

/** Explicit-resource field description, sharing the ordinary wire grammar. */
struct PmrWireFieldDescription {
    std::pmr::u16string name;  ///< Exact owned field name.
    std::int32_t serialTypeId; ///< Full serial identity, never masked.
};

/** Explicit-resource record description; nested allocations retain the supplied resource. */
struct PmrWireRecordDescription {
    std::int32_t senderId;                            ///< Peer directional sender ID.
    std::pmr::u16string name;                         ///< Exact owned record name.
    std::pmr::vector<PmrWireFieldDescription> fields; ///< Physical wire field order.

    /** Adopt the record name and select the field-container resource.
     * @param id Peer sender identity.
     * @param recordName Owned name, transferred without allocation.
     * @param resource Resource outliving this description.
     */
    PmrWireRecordDescription(std::int32_t id, std::pmr::u16string recordName, std::pmr::memory_resource &resource)
        : senderId(id), name(std::move(recordName)), fields(&resource) {
    }
};

/** Explicit-resource description parser result, retaining ordinary diagnostic fields. */
struct PmrDescriptionResult : DescriptionResult {
    std::pmr::vector<PmrWireRecordDescription> descriptions; ///< Owned complete prefix, independent of input.

    /** Initialize the ordinary diagnostics and explicit output allocator.
     * @param resource Resource outliving every returned description.
     */
    explicit PmrDescriptionResult(std::pmr::memory_resource &resource)
        : DescriptionResult{{}, DescriptionStatus::OK, DescriptionStage::PAYLOAD, 0, 0, encoding::Status::OK},
          descriptions(&resource) {
    }
};

/** Explicit-resource ordered binding without a local schema owner. */
struct PmrRecordBinding {
    std::int32_t senderId;                     ///< Directional peer identity.
    std::optional<std::int32_t> localRecordId; ///< Resolved immutable local identity, absent for an unknown name.
    std::pmr::vector<FieldBinding> fields;     ///< Ordered exact actions.

    /** Select allocator before binding any fields.
     * @param id Peer sender identity.
     * @param resource Resource outliving this binding.
     */
    PmrRecordBinding(std::int32_t id, std::pmr::memory_resource &resource) : senderId(id), fields(&resource) {
    }
};

/** Explicit-resource tagged value, preserving exact null versus empty payloads. */
struct PmrFieldValue {
    core::SerialFieldType type;                         ///< Exact full type.
    std::int64_t raw;                                   ///< Exact scalar bits.
    std::optional<std::pmr::u16string> units{};         ///< Nullable owned text for text tags.
    std::optional<std::pmr::vector<std::byte>> bytes{}; ///< Nullable owned opaque payload.

    /** Adopt a complete scalar or payload value.
     * @param fieldType Exact full type.
     * @param value Raw scalar bits.
     * @param text Exact optional text, transferred without allocation.
     * @param payload Exact optional bytes, transferred without allocation.
     */
    PmrFieldValue(core::SerialFieldType fieldType, std::int64_t value, std::optional<std::pmr::u16string> text = {},
                  std::optional<std::pmr::vector<std::byte>> payload = {})
        : type(fieldType), raw(value), units(std::move(text)), bytes(std::move(payload)) {
    }

    /** Preserve explicit allocator identity when copying a value.
     * @param other Complete source value.
     */
    PmrFieldValue(const PmrFieldValue &other) : type(other.type), raw(other.raw) {
        if (other.units) {
            units.emplace(*other.units, other.units->get_allocator());
        }

        if (other.bytes) {
            bytes.emplace(*other.bytes, other.bytes->get_allocator());
        }
    }

    /** Transfer owned payloads without allocation.
     * @param other Source value.
     */
    PmrFieldValue(PmrFieldValue &&other) noexcept = default;

    /** Copy through the source's allocation resource.
     * @param other Source value.
     * @return This value.
     */
    PmrFieldValue &operator=(const PmrFieldValue &other) {
        if (this != &other) {
            PmrFieldValue next(other);
            *this = std::move(next);
        }

        return *this;
    }

    /** Replace payload ownership without retaining an old allocator.
     * @param other Source value.
     * @return This value.
     */
    PmrFieldValue &operator=(PmrFieldValue &&other) noexcept {
        if (this != &other) {
            type = other.type;
            raw = other.raw;
            units.reset();
            bytes.reset();

            if (other.units) {
                units.emplace(std::move(*other.units));
            }

            if (other.bytes) {
                bytes.emplace(std::move(*other.bytes));
            }
        }

        return *this;
    }
};

/** One explicit-resource immutable stream record plan. */
struct PmrStreamRecordPlan {
    PmrRecordBinding binding;                                     ///< Exact owned field actions.
    std::size_t localFieldCount = 0;                              ///< Logical local output value count.
    core::RecordContext context{};                                ///< Immutable local provenance.
    std::optional<ResolvedIndexedTimeLayout> indexedTimeLayout{}; ///< Explicit raw-time projection positions.
    std::size_t eventTimeFieldCount = 0;                          ///< Recognized physical virtual-prefix count.
    std::size_t removalPrefixFieldCount = 0;                      ///< Physical prefix before REMOVE_EVENT time.
};

/** One local default shape, with an explicitly selected nested allocator. */
struct PmrDefaultShape {
    std::pmr::vector<PmrFieldValue> values; ///< Raw fresh-storage defaults in local field order.

    /** Select the nested value resource.
     * @param resource Resource outliving this shape.
     */
    explicit PmrDefaultShape(std::pmr::memory_resource &resource) : values(&resource) {
    }
};

/** Explicit-resource static data context for an immutable live binding generation. */
struct PmrStreamDataContext {
    std::pmr::vector<PmrStreamRecordPlan> plans; ///< Immutable peer sender plans.
    std::pmr::vector<PmrDefaultShape> shapes;    ///< Local typed defaults.
    core::LocalProfile localProfile;             ///< Validated exact full-type policy.

    /** Select immutable profile and explicit allocation.
     * @param profile Validated local field profile.
     * @param resource Resource outliving this context.
     */
    PmrStreamDataContext(core::LocalProfile profile, std::pmr::memory_resource &resource)
        : plans(&resource), shapes(&resource), localProfile(profile) {
    }

    /** Look up a sparse peer sender identity.
     * @param id Peer sender identity.
     * @return Borrowed immutable plan or null; no ID-indexed allocation occurs.
     */
    [[nodiscard]] const PmrStreamRecordPlan *find(std::int32_t id) const noexcept {
        for (const auto &plan : plans) {
            if (plan.binding.senderId == id) {
                return &plan;
            }
        }

        return nullptr;
    }

    /** Inspect the immutable exact-type policy.
     * @return Validated local profile.
     */
    [[nodiscard]] core::LocalProfile profile() const noexcept {
        return localProfile;
    }

    /** Borrow one raw typed default shape.
     * @param id Validated local record position.
     * @return Immutable defaults or an empty span for unknown/legacy output.
     */
    [[nodiscard]] std::span<const PmrFieldValue> defaults(std::int32_t id) const noexcept {
        if (id < 0 || static_cast<std::size_t>(id) >= shapes.size()) {
            return {};
        }

        return shapes[static_cast<std::size_t>(id)].values;
    }
};

/** Owned explicit-resource decoded record, matching the ordinary record's semantic fields. */
struct PmrStreamDataRecord {
    std::int32_t localId;                        ///< Immutable local record identity.
    encoding::PmrSymbolValue symbol;             ///< Exact symbol and raw flags.
    std::pmr::vector<std::int32_t> values;       ///< Legacy compact-only values.
    std::pmr::vector<PmrFieldValue> typedValues; ///< Exact tagged values.
    core::RecordContext context{};               ///< Owning schema provenance.
    std::int32_t senderId = -1;                  ///< Peer directional identity.
    std::optional<std::int64_t> removalTime{};   ///< Exact optional REMOVE_EVENT payload.
    std::optional<FieldEventTime> fieldTime{};   ///< Exact optional virtual-prefix metadata.
};

/** Explicit-resource output retaining ordinary complete-payload diagnostics and counters. */
struct PmrStreamDataResult : StreamDataResult {
    std::pmr::vector<PmrStreamDataRecord> records; ///< Complete prefix allocated only through the explicit resource.

    /** Select the output allocator before decoding.
     * @param resource Resource outliving this result.
     */
    explicit PmrStreamDataResult(std::pmr::memory_resource &resource) : records(&resource) {
    }
};

/** Parse through the ordinary shared description grammar using explicit allocation.
 * @param payload Complete borrowed description body.
 * @param limits Required finite grammar/work allowances.
 * @param resource Resource outliving the returned descriptions.
 * @return Complete prefix and ordinary diagnostics; custom resource exceptions propagate.
 */
[[nodiscard]] PmrDescriptionResult parsePmrDescriptions(std::span<const std::byte> payload, DescriptionLimits limits,
                                                        std::pmr::memory_resource &resource);
/** Bind through the ordinary shared exact-type planning algorithm using explicit allocation.
 * @param description Complete borrowed description.
 * @param schema Immutable local schema.
 * @param limits Required finite planning bounds.
 * @param prefixFields Explicit virtual prefix length.
 * @param resource Resource outliving returned actions.
 * @return Complete plan or ordinary binding error; custom resource exceptions propagate.
 */
[[nodiscard]] std::expected<PmrRecordBinding, BindingError>
bindPmrRecord(const PmrWireRecordDescription &description, const core::LocalSchema &schema, BindingLimits limits,
              std::size_t prefixFields, std::pmr::memory_resource &resource);
/** Decode through the ordinary shared record algorithm using explicit allocation.
 * @param payload Complete borrowed data body.
 * @param context Immutable allocator-aware binding snapshot.
 * @param limits Required finite per-message allowances.
 * @param policy Explicit event metadata policy.
 * @param resource Resource outliving the decoded output.
 * @return Complete prefix and ordinary diagnostics; custom resource exceptions propagate.
 */
[[nodiscard]] PmrStreamDataResult decodePmrStreamData(std::span<const std::byte> payload,
                                                      const PmrStreamDataContext &context, StreamDataLimits limits,
                                                      DataEventPolicy policy, std::pmr::memory_resource &resource);

} // namespace org::ttldtor::qdcxx::protocol::detail
