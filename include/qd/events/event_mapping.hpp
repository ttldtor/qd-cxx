// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/core/local_schema.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::events {

namespace detail {
/** Inline source business value paired with its WideDecimal projection, without source references. */
struct BusinessNumericSnapshot {
    std::int64_t wideRaw = 0; ///< Captured wide bits; a getter uses directValue only while these still match.
    double directValue = std::numeric_limits<double>::quiet_NaN(); ///< Actual source business-double semantics.
};
} // namespace detail

/** Failures of bounded preparation or complete local-value validation. */
enum class MappingError {
    INVALID_INPUT,     ///< Invalid record ID, request syntax or unsupported type alternative.
    MISSING_REQUIRED,  ///< No explicit lookup key selects a required field.
    AMBIGUOUS_FIELD,   ///< Accepted keys select distinct local fields.
    INCOMPATIBLE_TYPE, ///< Present local field has no accepted exact full type.
    WRONG_CONTEXT,     ///< Values have no context or belong to a different schema/record.
    INVALID_VALUES,    ///< Complete local count, type tags or canonical value shapes do not match.
    RESOURCE_LIMIT,    ///< Finite construction, work or event ownership allowance exceeded.
    ALLOCATION_FAILURE ///< Required owned storage could not be allocated.
};

/** Borrowed explicit property rule; event semantics and defaults belong to the adapter. */
struct FieldRequest {
    std::span<const std::u16string_view> keys;    ///< Accepted exact keys; aliases may share one target.
    std::span<const core::SerialFieldType> types; ///< Supported exact full type alternatives, never masks.
    bool required;                                ///< False permits local absence, represented by an empty selection.
};

/** Required finite logical bounds for preparation; zero is a real limit. */
struct MappingLimits {
    std::size_t fields;           ///< Maximum selected-record shape retained by the result.
    std::size_t lookups;          ///< Maximum source lookup entries inspected in the selected record.
    std::size_t selections;       ///< Maximum requested property selections retained by the result.
    std::size_t keys;             ///< Maximum aggregate request key count, including repeats.
    std::size_t typeAlternatives; ///< Maximum aggregate full-type alternative count, including repeats.
    std::size_t nameUnits;        ///< Maximum UTF-16 units in one source lookup or request key.
    std::size_t totalNameUnits;   ///< Maximum aggregate source lookup and request key units, including repeats.
    std::size_t comparisonWork;   ///< Key comparisons cost max(lengths)+1; present-field type comparisons cost one.
};

/** Mandatory logical UTF-16 ownership bounds for one complete event. */
struct EventValueLimits {
    std::size_t symbolUnits; ///< Maximum copied symbol units.
    std::size_t stringUnits; ///< Maximum units in each copied nullable field string.
    std::size_t totalUnits;  ///< Maximum combined symbol and selected field string units.
};

/** Borrowed complete local tagged values carrying explicit immutable provenance. */
struct RecordValueView {
    core::RecordContext context;              ///< Owned source schema/record identity, without its declarations.
    std::span<const core::FieldValue> values; ///< Complete local field order, borrowed for validation/projection.
};

/** Complete local field views; copying this object does not retain borrowed payload storage. */
struct BorrowedRecordValueView {
    core::RecordContext context;                  ///< Owned immutable source schema/record identity.
    std::span<const core::FieldValueView> values; ///< Complete local order; fields and payloads must outlive use.
};

/** Immutable local-to-event selections, independent of protocol and preparation input lifetime. */
class EventMapping {
    friend class PreparedCandle;
    friend class PreparedGreeks;
    friend class PreparedUnderlying;
    friend class PreparedTheoPrice;
    friend class PreparedTimeAndSale;
    friend class PreparedOptionSale;
    friend class PreparedMarketMaker;
    friend class PreparedDerivedOrder;
    /** Validate an actual indexed removal using the mapper's retained schema layout.
     * @param record Complete owning values; no payload is copied.
     * @param layout Actual independently retained source-time mapping.
     * @param rawTime Full signed removal identity, not public business index.
     * @return Complete validation, permitting exact UTF_CHAR pair words only in this explicit context.
     */
    [[nodiscard]] std::expected<void, MappingError> validateIndexedRemoval(RecordValueView record,
                                                                           const core::IndexedTimeLayout &layout,
                                                                           std::int64_t rawTime) const noexcept;
    /** Validate borrowed indexed removal fields without normalizing their raw words.
     * @param record Complete callback-borrowed values.
     * @param layout Actual retained source-time mapping.
     * @param rawTime Full signed removal identity.
     * @return Complete shape/context/canonical-or-index-word validation without allocation.
     */
    [[nodiscard]] std::expected<void, MappingError> validateIndexedRemoval(BorrowedRecordValueView record,
                                                                           const core::IndexedTimeLayout &layout,
                                                                           std::int64_t rawTime) const noexcept;
    core::RecordContext context_;
    std::vector<core::SerialFieldType> shape_;
    std::vector<std::optional<std::size_t>> selections_;

    /** Adopt fully bounded owned state.
     * @param context Valid source schema/record provenance.
     * @param shape Complete exact local type shape.
     * @param selections Ordered property positions; absent optional fields retain empty selections.
     */
    EventMapping(core::RecordContext context, std::vector<core::SerialFieldType> shape,
                 std::vector<std::optional<std::size_t>> selections) noexcept;

    public:
    /** Resolve explicit property rules once without retaining source names or containers.
     * @param schema Validated immutable local schema, borrowed only during construction.
     * @param localId Explicit selected local record ID; no event-name inference is performed.
     * @param requests Borrowed ordered property rules with exact keys and types.
     * @param limits Mandatory finite preparation storage and comparison bounds.
     * @return Owned immutable plan or first structured error; no partial mapping escapes.
     */
    [[nodiscard]] static std::expected<EventMapping, MappingError> prepare(const core::LocalSchema &schema,
                                                                           std::int32_t localId,
                                                                           std::span<const FieldRequest> requests,
                                                                           MappingLimits limits);

    /** Inspect owned provenance without retaining the original schema.
     * @return Immutable context valid until this mapping is moved from, assigned or destroyed.
     */
    [[nodiscard]] const core::RecordContext &context() const noexcept;

    /** Inspect ordered selected positions without permitting plan mutation.
     * @return Borrowed selections valid until this mapping is moved from, assigned or destroyed.
     */
    [[nodiscard]] std::span<const std::optional<std::size_t>> selections() const noexcept;

    /** Validate provenance first, then the complete shape including unselected extra fields.
     * @param record Borrowed complete values and explicit schema/record context.
     * @return Success or context/value error; no allocation, copying or retained values.
     */
    [[nodiscard]] std::expected<void, MappingError> validate(RecordValueView record) const noexcept;

    /** Validate borrowed field provenance first, then every exact tag and canonical payload shape.
     * @param record Complete field views and context; storage remains valid throughout this call.
     * @return Success or context/value error; no allocation, payload copying or retained views.
     */
    [[nodiscard]] std::expected<void, MappingError> validateView(BorrowedRecordValueView record) const noexcept;
};

} // namespace org::ttldtor::qdcxx::events
