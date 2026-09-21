#pragma once

#include "qd/core/local_schema.hpp"
#include "qd/protocol/describe_records.hpp"

#include <optional>

namespace qd::protocol {

/** Planner failures, separate from a successfully described unknown record. */
enum class BindingError {
    INVALID_DESCRIPTION, ///< Invalid sender ID, empty record/field name or out-of-range wire type ID.
    UNSUPPORTED_PROFILE, ///< A structurally valid full type is not exactly COMPACT_INT.
    RESOURCE_LIMIT,      ///< Explicit logical-element budget exceeded.
    ALLOCATION_FAILURE   ///< No plan published because allocation failed.
};

/** Mandatory finite planning budgets; zero is valid. */
struct BindingLimits {
    std::size_t fields;         ///< Maximum wire field actions.
    std::size_t nameUnits;      ///< Maximum units in each record/field name.
    std::size_t totalNameUnits; ///< Aggregate record/field UTF-16 units, including repeats.
};

/** One ordered COMPACT_INT action; it contains no schema reference or decoder. */
struct FieldBinding {
    std::optional<std::size_t> targetFieldId; ///< Assign this logical local field, or consume/skip when absent.
};

/** Owned independent description plan, not an incoming-ID table or delivered event. */
struct RecordBinding {
    std::int32_t senderId;                     ///< Original sender identity; never an allocation index.
    std::optional<std::int32_t> localRecordId; ///< Resolved local identity, absent for a described unknown name.
    std::vector<FieldBinding> fields;          ///< Exactly one action per wire field in wire order, preserving repeats.
};

/** Plan one raw description without retaining either input or installing session state.
 * @param description Borrowed complete raw description, unchanged during the call.
 * @param schema Borrowed validated immutable local subset.
 * @param limits Mandatory finite logical-element budgets.
 * @return Complete owned plan or first failure with no partial plan; allocation exceptions are translated.
 */
[[nodiscard]] std::expected<RecordBinding, BindingError>
bindRecord(const WireRecordDescription &description, const core::LocalSchema &schema, BindingLimits limits);

} // namespace qd::protocol
