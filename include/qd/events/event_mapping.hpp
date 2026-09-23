// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/core/local_schema.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::events {

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

/** Borrowed complete local tagged values carrying explicit immutable provenance. */
struct RecordValueView {
    core::RecordContext context;              ///< Owned source schema/record identity, without its declarations.
    std::span<const core::FieldValue> values; ///< Complete local field order, borrowed for validation/projection.
};

/** Immutable local-to-event selections, independent of protocol and preparation input lifetime. */
class EventMapping {
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
};

} // namespace org::ttldtor::qdcxx::events
