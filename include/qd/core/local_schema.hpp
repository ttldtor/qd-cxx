#pragma once

#include "qd/core/serial_field_type.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace qd::core {

/** Validation failures for the bounded resolved local input. */
enum class SchemaError {
    INVALID_SCHEMA,      ///< Invalid identity, name, duplicate key or dangling target.
    UNSUPPORTED_PROFILE, ///< Time or non-COMPACT_INT shape.
    RESOURCE_LIMIT,      ///< Caller logical-element budget exceeded.
    ALLOCATION_FAILURE   ///< Owned copy could not be allocated.
};

/** One ordinary logical field; its position is its record-local identity. */
struct LocalField {
    std::int32_t serialTypeId; ///< Full type, accepted only when exactly COMPACT_INT (8).
};

/** Explicit record-scoped key, already resolved by the caller. */
struct FieldLookup {
    std::u16string name; ///< Owned key; no heuristic aliases are manufactured.
    std::size_t fieldId; ///< Logical field position in the containing record.
};

/** Loader-independent input subset; not a serialized DataScheme. */
struct LocalRecord {
    std::int32_t id;                  ///< Local identity, required to equal the record position.
    std::u16string name;              ///< Exact nonempty well-formed UTF-16 record name.
    bool hasTime;                     ///< True is explicitly unsupported by this ordinary-field subset.
    std::vector<LocalField> fields;   ///< Ordered logical fields, with no synthetic slots.
    std::vector<FieldLookup> lookups; ///< Unique record keys with valid targets covering every field.
};

/** Mandatory finite logical budgets for an owned schema. */
struct SchemaLimits {
    std::size_t records;        ///< Maximum record count.
    std::size_t fields;         ///< Maximum total logical field count.
    std::size_t lookups;        ///< Maximum total explicit key count.
    std::size_t nameUnits;      ///< Maximum UTF-16 units in one record/key name.
    std::size_t totalNameUnits; ///< Maximum aggregate record/key units, counting repeats across scopes.
};

/** Validated owned immutable subset; no protocol or loading dependency. */
class LocalSchema {
    std::vector<LocalRecord> records_;

    /** Copy a fully validated input.
     * @param records Borrowed validated records, copied into this object.
     */
    explicit LocalSchema(std::span<const LocalRecord> records);

    public:
    /** Validate all input before publishing an owned schema.
     * @param records Borrowed input, unchanged during the call; may be empty.
     * @param limits Required logical-element bounds, including zero.
     * @return Owned immutable schema or first error; allocation exceptions are translated.
     */
    [[nodiscard]] static std::expected<LocalSchema, SchemaError> create(std::span<const LocalRecord> records,
                                                                        SchemaLimits limits);

    /** Inspect validated records without permitting mutation.
     * @return View valid until this schema is destroyed, moved from or assigned.
     */
    [[nodiscard]] std::span<const LocalRecord> records() const noexcept;
};

} // namespace qd::core
