// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/core/serial_field_type.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace org::ttldtor::qdcxx::core {

/** Validation failures for the bounded resolved local input. */
enum class SchemaError {
    INVALID_SCHEMA,      ///< Invalid identity, name, duplicate key or dangling target.
    UNSUPPORTED_PROFILE, ///< Time or field/profile identity outside the explicitly selected subset.
    RESOURCE_LIMIT,      ///< Caller logical-element budget exceeded.
    ALLOCATION_FAILURE   ///< Owned copy could not be allocated.
};

/** One logical field; its position is its record-local identity, independent of source slots. */
struct LocalField {
    std::int32_t serialTypeId; ///< Full type, validated against the explicitly selected local profile.
};

/** Explicit record-scoped key, already resolved by the caller. */
struct FieldLookup {
    std::u16string name; ///< Owned key; no heuristic aliases are manufactured.
    std::size_t fieldId; ///< Logical field position in the containing record.
};

/** Two logical signed32 fields proven to occupy source integer slots zero and one. */
struct IndexedTimePairInt32 {
    std::size_t firstField;  ///< Logical high-word field ID, independent of wire order.
    std::size_t secondField; ///< Distinct logical low-word field ID.
};

/** One logical signed64 field proven to occupy both source time slots. */
struct IndexedTimeSingleInt64 {
    std::size_t field; ///< Logical field ID; no physical VOID tail is materialized.
};

/** Finite explicit source-time mapping; never inferred from a record or field name. */
using IndexedTimeLayout = std::variant<IndexedTimePairInt32, IndexedTimeSingleInt64>;

/** Loader-independent input subset; not a serialized DataScheme. */
struct LocalRecord {
    std::int32_t id;                  ///< Local identity, required to equal the record position.
    std::u16string name;              ///< Exact nonempty well-formed UTF-16 record name.
    bool hasTime;                     ///< Explicit source indexing; requires a supported layout in BINARY_FIELDS_V1.
    std::vector<LocalField> fields;   ///< Ordered logical fields, with no synthetic slots.
    std::vector<FieldLookup> lookups; ///< Unique record keys with valid targets covering every field.
    std::optional<IndexedTimeLayout> indexedTimeLayout{}; ///< Owned logical IDs; absent for ordinary records.
};

/** Mandatory finite logical budgets for an owned schema. */
struct SchemaLimits {
    std::size_t records;        ///< Maximum record count.
    std::size_t fields;         ///< Maximum total logical field count.
    std::size_t lookups;        ///< Maximum total explicit key count.
    std::size_t nameUnits;      ///< Maximum UTF-16 units in one record/key name.
    std::size_t totalNameUnits; ///< Maximum aggregate record/key units, counting repeats across scopes.
};

/** Owned opaque provenance for one immutable local record; default contexts never match. */
class RecordContext {
    struct Identity;
    std::shared_ptr<const Identity> identity_;
    std::int32_t localId_ = -1;

    friend class LocalSchema;
    /** Retain one validated schema identity and record selection.
     * @param identity Fixed-size owned identity, independent of schema declarations.
     * @param localId Validated local record ID.
     */
    RecordContext(std::shared_ptr<const Identity> identity, std::int32_t localId) noexcept;

    public:
    /** Construct an invalid context for untagged caller values. */
    RecordContext() noexcept = default;
    /** Inspect whether a source schema supplied this context.
     * @return True only for a valid owned schema/record selection.
     */
    [[nodiscard]] bool valid() const noexcept;
    /** Compare provenance without inspecting schema names or values.
     * @param other Context to compare; invalid contexts never match.
     * @return True only for the same live identity and local record ID.
     */
    [[nodiscard]] bool matches(const RecordContext &other) const noexcept;
};

/** Validated owned immutable subset; no protocol or loading dependency. */
class LocalSchema {
    std::vector<LocalRecord> records_;
    LocalProfile profile_;
    std::shared_ptr<const RecordContext::Identity> identity_;

    /** Copy a fully validated input.
     * @param records Borrowed validated records, copied into this object.
     * @param profile Validated exact-type policy retained with the schema.
     */
    explicit LocalSchema(std::span<const LocalRecord> records, LocalProfile profile);

    public:
    /** Validate all input before publishing an owned schema.
     * @param records Borrowed input, unchanged during the call; may be empty.
     * @param limits Required logical-element bounds, including zero.
     * @param profile Explicit exact-type subset; omission preserves the type-8 contract.
     * @return Owned immutable schema or first error; allocation exceptions are translated.
     */
    [[nodiscard]] static std::expected<LocalSchema, SchemaError>
    create(std::span<const LocalRecord> records, SchemaLimits limits,
           LocalProfile profile = LocalProfile::COMPACT_INT_V1);

    /** Copy within explicit storage limits while preserving immutable provenance.
     * @param limits Required logical counts and name limits for the complete copy.
     * @return Independent owned declarations with the same identity, or resource/allocation error.
     */
    [[nodiscard]] std::expected<LocalSchema, SchemaError> boundedCopy(SchemaLimits limits) const;

    /** Obtain owned provenance for manually supplied or decoded local values.
     * @param localId Explicit record position in this schema.
     * @return Owned context, or invalid context for an out-of-range ID or moved-from schema.
     */
    [[nodiscard]] RecordContext recordContext(std::int32_t localId) const noexcept;

    /** Inspect the immutable exact-type policy.
     * @return The profile selected during validation.
     */
    [[nodiscard]] LocalProfile profile() const noexcept {
        return profile_;
    }

    /** Inspect validated records without permitting mutation.
     * @return View valid until this schema is destroyed, moved from or assigned.
     */
    [[nodiscard]] std::span<const LocalRecord> records() const noexcept;
};

} // namespace org::ttldtor::qdcxx::core
