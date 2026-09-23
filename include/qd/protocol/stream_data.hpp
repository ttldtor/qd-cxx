// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/encoding/char_array.hpp"
#include "qd/encoding/symbol_reader.hpp"
#include "qd/protocol/static_binding.hpp"

namespace org::ttldtor::qdcxx::protocol {
/** Finite bounds for building a static snapshot; no caller memory is retained. */
struct ContextLimits {
    core::SchemaLimits schema; ///< Bounds inspected local schema, including names and lookup storage.
    std::size_t descriptions;  ///< Maximum unique sender descriptions.
    BindingLimits bindings;    ///< Aggregate actions/names and per-name bound across all descriptions.
};

/** Owned validated plan with the output shape needed after schema destruction. */
struct StreamRecordPlan {
    RecordBinding binding;         ///< Owned ordered actions derived only by bindRecord.
    std::size_t localFieldCount;   ///< Number of local logical values, zero for unknown records.
    core::RecordContext context{}; ///< Owned local provenance; invalid for an unknown record.
};

/** Immutable static decoding snapshot; not a dynamic session table. */
class StreamDataContext {
    std::vector<StreamRecordPlan> plans_;
    std::vector<std::vector<core::FieldValue>> defaults_;
    core::LocalProfile profile_;
    /** Adopt validated plans.
     * @param plans Fully validated owned plans.
     * @param defaults One bounded tagged shape per local record; empty for the legacy profile.
     * @param profile Validated exact-type policy.
     */
    explicit StreamDataContext(std::vector<StreamRecordPlan> plans, std::vector<std::vector<core::FieldValue>> defaults,
                               core::LocalProfile profile) noexcept;

    public:
    /** Validate and own a complete snapshot; failure publishes no partial state.
     * @param schema Validated local subset, borrowed during construction only.
     * @param descriptions Complete raw descriptions; duplicate sender IDs are rejected.
     * @param limits Mandatory schema and aggregate plan budgets.
     * @return Immutable snapshot or binding/resource/allocation error.
     */
    [[nodiscard]] static std::expected<StreamDataContext, BindingError>
    create(const core::LocalSchema &schema, std::span<const WireRecordDescription> descriptions, ContextLimits limits);
    /** Find a sender plan without ID-indexed allocation.
     * @param senderId Wire identity, distinct from local identity.
     * @return Borrowed immutable plan until context mutation/destruction, or null if undescribed.
     */
    [[nodiscard]] const StreamRecordPlan *find(std::int32_t senderId) const noexcept;

    /** Inspect the exact-type policy owned by this snapshot.
     * @return Immutable profile selected by the validated schema.
     */
    [[nodiscard]] core::LocalProfile profile() const noexcept {
        return profile_;
    }

    /** Inspect raw-zero defaults in local logical order.
     * @param localId Local record identity from a plan returned by this context.
     * @return Borrowed shape until context destruction/move; empty for legacy or unknown IDs.
     */
    [[nodiscard]] std::span<const core::FieldValue> defaults(std::int32_t localId) const noexcept;
};

/** Complete-payload decoding outcomes; no recovery or continuation is provided. */
enum class StreamDataStatus {
    OK,                  ///< Entire payload decoded.
    TRUNCATED_PAYLOAD,   ///< Symbol, ID or field ends inside a primitive.
    INVALID_SYMBOL,      ///< Symbol grammar or length is invalid.
    INVALID_ID,          ///< Negative signed sender ID.
    UNDESCRIBED_ID,      ///< Sender ID has no static description.
    UNSUPPORTED_PROFILE, ///< Nonzero raw event flags.
    RESOURCE_LIMIT,      ///< Mandatory logical budget exceeded.
    ALLOCATION_FAILURE,  ///< Allocation failed; completed prefix retained.
    INVALID_FIELD        ///< Character or array primitive has invalid encoding or length.
};
/** Checkpoint identifying failure location or successful completion. */
enum class StreamDataStage {
    PAYLOAD,   ///< Initial payload budget.
    RECORD,    ///< Processed-record budget, including skips.
    SYMBOL,    ///< Message-local symbol primitive.
    FLAGS,     ///< Raw flag profile validation.
    RECORD_ID, ///< Compact ID primitive and lookup.
    FIELD,     ///< Wire-field budget or exact selected primitive.
    OUTPUT,    ///< Output budgets, initialization or publication.
    END        ///< Successful completion.
};

/** Mandatory finite per-message budgets; zero is valid. */
struct StreamDataLimits {
    std::size_t payloadBytes;                ///< Maximum complete payload size.
    std::size_t processedRecords;            ///< Maximum records, including skipped and zero-field records.
    std::size_t wireFields;                  ///< Maximum fields read, including skips and repeated targets.
    std::size_t emittedRecords;              ///< Maximum completed output records.
    std::size_t emittedValues;               ///< Aggregate logical local values, including absent fields.
    std::size_t ownedSymbolUnits;            ///< Aggregate units in output symbols, including repeat copies.
    encoding::SymbolLimits symbol;           ///< Remaining-span, per-symbol and retained-state limits.
    encoding::CharArrayLimits fieldString{}; ///< Maximum encoded bytes and units in one string, including skips.
    std::size_t fieldBytes = 0;              ///< Aggregate examined string bytes, including prefixes and failed reads.
    std::size_t fieldUnits = 0;      ///< Aggregate admitted string units, without refunds for skips or replacements.
    std::size_t ownedFieldUnits = 0; ///< Completed plus current and temporary string units; logical storage only.
};

/** Owned local record; missing fields contain measured fresh-storage raw zero with their type. */
struct StreamDataRecord {
    std::int32_t localId;                      ///< Local schema ID, never sender identity.
    encoding::SymbolValue symbol;              ///< Exact owned UTF-16 and raw encoding metadata.
    std::vector<std::int32_t> values;          ///< Legacy profile only, local order; empty for the extended profile.
    std::vector<core::FieldValue> typedValues; ///< Extended profile only, exact local tags/raw values.
    core::RecordContext context{}; ///< Source schema/record identity; old aggregate initializers remain untagged.
};

/** Owned completed prefix with explicit first-error diagnostics. */
struct StreamDataResult {
    std::size_t processedRecords = 0; ///< Charged record attempts, including skips and subsequent failures.
    std::size_t wireFields = 0; ///< Charged field attempts, including skips, repeated targets and subsequent failures.
    StreamDataStatus status = StreamDataStatus::OK;   ///< Completion or first failure.
    StreamDataStage stage = StreamDataStage::PAYLOAD; ///< Active checkpoint or END.
    std::size_t consumed = 0;                         ///< Committed examination position relative to payload start.
    std::size_t errorOffset = 0; ///< Primitive start or validation checkpoint; payload size on success.
    encoding::SymbolStatus symbolStatus = encoding::SymbolStatus::OK; ///< Symbol cause when applicable.
    std::vector<StreamDataRecord> records; ///< Completed records only; retained on later failure.
    std::size_t fieldBytes = 0;            ///< Charged string bytes examined, including failed reads.
    std::size_t fieldUnits = 0;      ///< Charged admitted string units, including skips, replacements and failures.
    std::size_t ownedFieldUnits = 0; ///< String units retained by completed records only.
};

/** Decode one complete type-15 payload with fresh message-local symbol state.
 * @param payload Borrowed payload excluding length/type, unchanged during this call.
 * @param context Validated static snapshot, borrowed during this call.
 * @param limits Required finite per-message budgets.
 * @return Owned completed prefix and status/positions; no partial current record or retained input.
 */
[[nodiscard]] StreamDataResult decodeStreamData(std::span<const std::byte> payload, const StreamDataContext &context,
                                                StreamDataLimits limits);
/** Decode one complete type-5 payload under RAW-TAPE's exact fields and zero-flags policy.
 * This is one wire-record projection, without Java ticker/history callback fan-out or timing state.
 * @param payload Borrowed payload excluding length/type, unchanged during this call.
 * @param context Validated static snapshot, borrowed during this call.
 * @param limits Required finite per-message budgets.
 * @return Owned completed prefix and first-error positions, with fresh message-local symbols.
 */
[[nodiscard]] StreamDataResult decodeRawData(std::span<const std::byte> payload, const StreamDataContext &context,
                                             StreamDataLimits limits);
} // namespace org::ttldtor::qdcxx::protocol
