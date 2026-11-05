// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/encoding/char_array.hpp"
#include "qd/encoding/symbol_reader.hpp"
#include "qd/protocol/static_binding.hpp"

namespace org::ttldtor::qdcxx::protocol {
/** Explicit recognition of finite wire FIELD metadata, independent of descriptor time text. */
enum class FieldTimePolicy {
    DISABLED, ///< Preserve ordinary binding and the old explicit FIELD descriptor refusal.
    READ      ///< Recognize the exact leading EventTime/full56 and optional EventSequence/full72.
};

/** Owned physical FIELD words with exact Java low-word projections. */
struct FieldEventTime {
    std::int64_t rawSeconds = 0;               ///< Physical signed compact-long before low32 projection.
    std::optional<std::int64_t> rawSequence{}; ///< Absent differs from an explicit zero sequence.
    std::uint64_t sourceOrdinal = 0;           ///< Zero-based source record attempt, including unknown skips.
    /** Project low32 seconds and sequence without saturation.
     * @return Exact signed64 packed cursor bits.
     */
    [[nodiscard]] std::int64_t packed() const noexcept;
    /** Project signed32 seconds and the unsigned ten-bit millisecond subfield.
     * @return Exact milliseconds, retaining subfield values 1000 through 1023.
     */
    [[nodiscard]] std::int64_t eventTimeMs() const noexcept;
};

/** Finite bounds for building a static snapshot; no caller memory is retained. */
struct ContextLimits {
    core::SchemaLimits schema; ///< Bounds inspected local schema, including names and lookup storage.
    std::size_t descriptions;  ///< Maximum unique sender descriptions.
    BindingLimits bindings;    ///< Aggregate actions/names and per-name bound across all descriptions.
};

/** Resolved dense output positions for the signed32 high and low time words. */
struct ResolvedIndexedTimePairInt32 {
    std::size_t firstValueIndex;  ///< High-word position, never a sender ID or source slot.
    std::size_t secondValueIndex; ///< Low-word position in the same local values vector.
};

/** Resolved dense output position for a signed64 indexed value. */
struct ResolvedIndexedTimeSingleInt64 {
    std::size_t valueIndex; ///< Complete logical64 value; physical tails remain absent.
};

/** Resolved low-word output position with no logical high-word field. */
struct ResolvedIndexedTimeLowInt32 {
    std::size_t valueIndex; ///< Exact COMPACT_INT contributor; full removal identity stays in metadata.
};

/** Owned decoding positions, distinct from the logical IDs supplied to LocalRecord. */
using ResolvedIndexedTimeLayout =
    std::variant<ResolvedIndexedTimePairInt32, ResolvedIndexedTimeSingleInt64, ResolvedIndexedTimeLowInt32>;

/** Owned validated plan with the output shape needed after schema destruction. */
struct StreamRecordPlan {
    RecordBinding binding;         ///< Owned ordered exact actions, with explicit virtual-prefix precedence.
    std::size_t localFieldCount;   ///< Number of local logical values, zero for unknown records.
    core::RecordContext context{}; ///< Owned local provenance; invalid for an unknown record.
    std::optional<ResolvedIndexedTimeLayout> indexedTimeLayout{}; ///< Fixed-size owned positions for indexed records.
    std::size_t eventTimeFieldCount = 0;     ///< Exact recognized raw prefix length, zero through two.
    std::size_t removalPrefixFieldCount = 0; ///< Exclusive physical action bound before removal time.
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
    /** Build an owned snapshot with explicit virtual-prefix precedence.
     * @param schema Borrowed validated schema; READ requires BINARY_FIELDS_V1.
     * @param descriptions Complete borrowed sender declarations, never retained by reference.
     * @param limits Mandatory finite construction bounds.
     * @param policy Explicit FIELD recognition; invalid values return UNSUPPORTED_PROFILE.
     * @return Complete snapshot or an error without publication.
     */
    [[nodiscard]] static std::expected<StreamDataContext, BindingError>
    createWithFieldTimePolicy(const core::LocalSchema &schema, std::span<const WireRecordDescription> descriptions,
                              ContextLimits limits, FieldTimePolicy policy);
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
    UNSUPPORTED_PROFILE, ///< Event flags or explicit data policy outside the selected profile.
    RESOURCE_LIMIT,      ///< Mandatory logical budget exceeded.
    ALLOCATION_FAILURE,  ///< Allocation failed; completed prefix retained.
    INVALID_FIELD        ///< Character or array primitive has invalid encoding or length.
};
/** Checkpoint identifying failure location or successful completion. */
enum class StreamDataStage {
    PAYLOAD,     ///< Initial payload budget.
    RECORD,      ///< Processed-record budget, including skips.
    SYMBOL,      ///< Message-local symbol primitive.
    FLAGS,       ///< Raw flag profile validation.
    RECORD_ID,   ///< Compact ID primitive and lookup.
    FIELD,       ///< Wire-field budget or exact selected primitive.
    OUTPUT,      ///< Output budgets, initialization or publication.
    END,         ///< Successful completion.
    REMOVAL_TIME ///< REMOVE_EVENT compact-long payload, distinct from ordinary fields and FIELD metadata.
};

/** Explicit data metadata policy; field support alone never enables event semantics. */
enum class DataEventPolicy {
    ZERO_FLAGS,       ///< Existing complete-payload grammar, refusing every nonzero raw flag.
    BINARY_CATALOG_V1 ///< Six known data bits and REMOVE_EVENT/NONE, requiring BINARY_FIELDS_V1.
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
    encoding::CharArrayLimits fieldString{}; ///< Type10 encoded bound; per-value reserved units for both text tags.
    std::size_t fieldBytes = 0;              ///< Aggregate examined type10 bytes, including prefixes and failed reads.
    std::size_t fieldUnits = 0;      ///< Aggregate admitted string units, without refunds for skips or replacements.
    std::size_t ownedFieldUnits = 0; ///< Completed plus current and temporary string units; logical storage only.
    std::size_t fieldPayloadEncodedBytes = 0; ///< Examined prefix/body bytes for one byte/object/UTFString value.
    std::size_t fieldPayloadBytes = 0;        ///< Admitted declared body bytes for one new payload value.
    std::size_t payloadEncodedBytes = 0; ///< Aggregate examined new-payload bytes, including failed reads and skips.
    std::size_t payloadWorkBytes = 0;  ///< Aggregate admitted new-payload body work; no skip/replacement/error refunds.
    std::size_t ownedPayloadBytes = 0; ///< Completed plus current plus temporary new-payload logical storage bytes.
    std::size_t ownedFieldTimeRecords = 0; ///< Completed plus current fixed FIELD metadata owners; zero stays zero.
};

/** Owned local record; missing fields contain measured fresh-storage raw zero with their type. */
struct StreamDataRecord {
    std::int32_t localId;                      ///< Local schema ID, never sender identity.
    encoding::SymbolValue symbol;              ///< Exact owned UTF-16 and raw encoding metadata.
    std::vector<std::int32_t> values;          ///< Legacy profile only, local order; empty for the extended profile.
    std::vector<core::FieldValue> typedValues; ///< Every non-compact profile, with exact tags/raw and owned payloads.
    core::RecordContext context{}; ///< Source schema/record identity; old aggregate initializers remain untagged.
    std::int32_t senderId = -1;    ///< Owned exact wire ID; -1 only for caller-created records without source identity.
    std::optional<std::int64_t> removalTime{}; ///< Exact REMOVE_EVENT compact-long; engaged zero differs from absence.
    std::optional<FieldEventTime> fieldTime{}; ///< Owned raw virtual prefix; engaged zero differs from absence.
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
    std::size_t fieldBytes = 0;            ///< Charged type10 bytes examined, including failed reads.
    std::size_t fieldUnits = 0;          ///< Charged admitted string units, including skips, replacements and failures.
    std::size_t ownedFieldUnits = 0;     ///< String units retained by completed records only.
    std::size_t payloadEncodedBytes = 0; ///< Examined new-payload bytes, including prefixes and failed reads.
    std::size_t payloadWorkBytes = 0;  ///< Admitted new-payload body work, including skips, replacements and failures.
    std::size_t ownedPayloadBytes = 0; ///< Logical new-payload bytes retained by completed records only.
    std::size_t ownedFieldTimeRecords = 0; ///< Fixed FIELD metadata owners retained by completed records only.
};

/** Decode one complete type-15 payload with fresh message-local symbol state.
 * The context selects exact field support; BINARY_FIELDS_V1 adds ordinary scalar and owned
 * byte/object/UTFString identities with explicit finite payload budgets. Existing profiles and
 * type10's character grammar and byte-budget defaults retain their meanings.
 * @param payload Borrowed payload excluding length/type, unchanged during this call.
 * @param context Validated static snapshot, borrowed during this call.
 * @param limits Required finite per-message budgets.
 * @return Owned completed prefix and status/positions; no partial current record or retained input.
 */
[[nodiscard]] StreamDataResult decodeStreamData(std::span<const std::byte> payload, const StreamDataContext &context,
                                                StreamDataLimits limits);
/** Decode a complete type-15/NONE payload under an explicit event policy.
 * Broad policy preserves all subsets of raw0x5f independently of descriptor option advertisement.
 * REMOVE_EVENT reads the configured physical Event* prefix under READ, then one compact-long;
 * DISABLED omits ordinary fields; later fields keep defaults except explicitly mapped indexed words.
 * FIELD metadata is selected only by the context's explicit FieldTimePolicy. Other flags are refused.
 * Invalid policy/profile combinations fail at PAYLOAD before payload-budget or record work.
 * @param payload Borrowed complete body excluding length/type; never retained.
 * @param context Owned validated bindings/layouts borrowed only during this call.
 * @param limits Finite input/work/output allowances; omitted fields still require local value storage.
 * @param policy Explicit metadata policy; no default broadens existing overloads.
 * @return Owned completed prefix and first-error diagnostics; current incomplete record is never published.
 */
[[nodiscard]] StreamDataResult decodeStreamDataWithEventPolicy(std::span<const std::byte> payload,
                                                               const StreamDataContext &context,
                                                               StreamDataLimits limits, DataEventPolicy policy);
/** Decode one complete type-5 payload under the context's exact fields and RAW-TAPE's zero-flags policy.
 * This is one wire-record projection, without Java ticker/history callback fan-out or timing state.
 * @param payload Borrowed payload excluding length/type, unchanged during this call.
 * @param context Validated static snapshot, borrowed during this call.
 * @param limits Required finite per-message budgets.
 * @return Owned completed prefix and first-error positions, with fresh message-local symbols.
 */
[[nodiscard]] StreamDataResult decodeRawData(std::span<const std::byte> payload, const StreamDataContext &context,
                                             StreamDataLimits limits);
} // namespace org::ttldtor::qdcxx::protocol
