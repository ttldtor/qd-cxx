// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/protocol/tape_reader.hpp"

#include <expected>

namespace org::ttldtor::qdcxx::protocol {

/** Bounded composition failures, independent of caller I/O and Java exception classes. */
enum class WriterError {
    NONE,              ///< No failure.
    INVALID_LAYOUT,    ///< Outgoing identity, lookup or complete field coverage is invalid.
    INVALID_VALUE,     ///< Record, heartbeat or exact tagged value is invalid.
    UNSUPPORTED,       ///< Descriptor, profile, flags or full type is outside the outgoing subset.
    INVALID_STATE,     ///< Operation violates the single-use lifecycle.
    RESOURCE_LIMIT,    ///< A finite logical bound or representable encoded size was exceeded.
    ALLOCATION_FAILURE ///< Bounded owned storage allocation failed.
};

/** Single-threaded writer lifecycle; destruction does not finish a recording. */
enum class WriterState {
    READY,   ///< Configuration is owned; begin has not emitted the descriptor.
    ACTIVE,  ///< Descriptor has been emitted; explicit frame operations are permitted.
    FAILED,  ///< First terminal failure is retained; later inputs are not examined.
    FINISHED ///< Explicit finish succeeded; only idempotent finish is permitted.
};

/** Numeric diagnostic operation, without borrowed user text. */
enum class WriterOperation {
    LAYOUT,    ///< Immutable outgoing layout construction.
    CREATE,    ///< Writer configuration validation and copy.
    BEGIN,     ///< Sole initial descriptor.
    DESCRIBE,  ///< One atomic description group.
    HEARTBEAT, ///< One explicit metadata frame.
    DATA,      ///< One complete RAW_DATA batch.
    MARKER,    ///< One empty envelope.
    FINISH     ///< Explicit completion without bytes.
};

/** First failure owns numeric checkpoints and uses only static stage text. */
struct WriterDiagnostic {
    WriterError error = WriterError::NONE;               ///< Category, separate from checkpoints.
    WriterOperation operation = WriterOperation::CREATE; ///< Failed construction or frame operation.
    std::string_view stage = "none";                     ///< Static checkpoint; never an input name.
    std::optional<std::size_t> record;                   ///< Input record/group position, when applicable.
    std::optional<std::size_t> field;                    ///< Input local or wire field position at the stage.
    /** Compare complete first-error evidence.
     * @param other Diagnostic to compare.
     * @return True when all static and numeric checkpoints match.
     */
    bool operator==(const WriterDiagnostic &other) const = default;
};

/** One explicitly named target; order determines outgoing field order. */
struct OutgoingField {
    std::u16string wireName;   ///< Owned exact lookup key targeting localFieldId.
    std::int32_t serialTypeId; ///< Supported full identity, exactly equal to the local field type.
    std::int32_t localFieldId; ///< Record-local logical field position; never a physical Java slot.
};

/** Complete one-to-one outgoing identity and ordered field coverage. */
struct OutgoingRecord {
    std::int32_t senderId;             ///< Unique identity in 0..INT32_MAX-1; never an allocation index.
    std::int32_t localRecordId;        ///< Unique existing local schema record position.
    std::u16string wireName;           ///< Owned exact local record name.
    std::vector<OutgoingField> fields; ///< Every local field exactly once, in explicit outgoing order.
};

/** Required finite bounds for the complete schema and outgoing declaration copies. */
struct LayoutLimits {
    core::SchemaLimits schema;  ///< Complete local schema copy bounds.
    std::size_t records;        ///< Maximum outgoing record count.
    std::size_t fields;         ///< Maximum aggregate outgoing field count.
    std::size_t nameUnits;      ///< Maximum units in one outgoing record/field name.
    std::size_t totalNameUnits; ///< Maximum aggregate outgoing name units, including repeated names.
};

/** Immutable owned schema and explicit outgoing layouts; no inferred aliases or ID coalescing. */
class OutgoingLayouts {
    core::LocalSchema schema_;
    std::vector<OutgoingRecord> records_;
    LayoutLimits limits_;

    friend class TapeWriter;
    /** Adopt fully validated bounded declarations.
     * @param schema Owned validated schema.
     * @param records Owned outgoing declarations.
     * @param limits Bounds used for subsequent independent copies.
     */
    OutgoingLayouts(core::LocalSchema schema, std::vector<OutgoingRecord> records, LayoutLimits limits) noexcept;

    public:
    /** Transfer ownership; borrowed views into the source expire.
     * @param other Immutable owned declarations to move.
     */
    OutgoingLayouts(OutgoingLayouts &&other) noexcept = default;
    /** Copying is unavailable; use bounded construction for independent declarations.
     * @param other Source declarations.
     */
    OutgoingLayouts(const OutgoingLayouts &other) = delete;
    /** Assignment is unavailable; layouts remain immutable after construction.
     * @param other Source declarations.
     * @return No result because assignment is deleted.
     */
    OutgoingLayouts &operator=(const OutgoingLayouts &other) = delete;
    /** Assignment is unavailable; layouts remain immutable after construction.
     * @param other Source declarations.
     * @return No result because assignment is deleted.
     */
    OutgoingLayouts &operator=(OutgoingLayouts &&other) = delete;
    /** Validate complete bijective coverage before publishing owned declarations.
     * Read-only BINARY_FIELDS_V1 schemas are refused before allocation, including empty layouts.
     * @param schema Validated local schema, borrowed only for this call.
     * @param records Ordered outgoing declarations, borrowed only for this call.
     * @param limits Required finite copy bounds; zero is a real allowance.
     * @return Immutable owned copies or the first construction diagnostic; no partial publication.
     */
    [[nodiscard]] static std::expected<OutgoingLayouts, WriterDiagnostic>
    create(const core::LocalSchema &schema, std::span<const OutgoingRecord> records, LayoutLimits limits);
    /** Inspect the owned immutable schema.
     * @return Borrowed reference valid until destruction or move.
     */
    [[nodiscard]] const core::LocalSchema &schema() const noexcept;
    /** Inspect explicit outgoing order.
     * @return Const view valid until destruction or move.
     */
    [[nodiscard]] std::span<const OutgoingRecord> records() const noexcept;
    /** Find an explicit sender identity without ID-indexed allocation.
     * @param senderId Exact wire identity.
     * @return Borrowed layout until destruction/move, or null when unknown.
     */
    [[nodiscard]] const OutgoingRecord *find(std::int32_t senderId) const noexcept;
};

/** Complete represented RAW/MESSAGE descriptor, with exact optional text distinctions. */
struct OutgoingDescriptor {
    std::optional<std::u16string> version; ///< Opaque nonnull text when present; absence differs from empty.
    std::optional<std::u16string> options; ///< Absent, empty or exact hs only.
    std::u16string time;                   ///< Exact MESSAGE or message spelling.
    bool explicitRawSend = false;          ///< Emit singleton RAW_DATA send list rather than an empty send list.
};

/** Caller-owned record inputs, all borrowed only during data(). */
struct OutgoingRecordView {
    std::int32_t senderId;                    ///< Announced exact sender identity.
    std::u16string_view symbol;               ///< Nonnullable exact units; empty and isolated surrogates are valid.
    std::span<const core::FieldValue> values; ///< Exact tagged values in local logical order.
    std::int32_t eventFlags = 0;              ///< Only zero is supported; checked before symbol and identity.
};

/** Independent caller-owned whole envelope; finish returns an empty byte vector. */
struct WriterChunk {
    std::vector<std::byte> bytes; ///< Whole successfully committed operation; no partial failed chunk escapes.
};

/** Required finite work, output and peak private storage allowances. */
struct WriterLimits {
    std::size_t frameBytes;                ///< Maximum body bytes, including the compact message ID.
    std::size_t outputBytes;               ///< Cumulative committed envelope bytes, including length prefixes.
    std::size_t frames;                    ///< Cumulative committed envelopes, including markers.
    std::size_t records;                   ///< Cumulative attempted data records and description IDs.
    std::size_t fields;                    ///< Cumulative attempted data/description fields.
    encoding::UtfStringLimits text;        ///< Per descriptor/name UTFString body bytes and input units.
    std::size_t textUnits;                 ///< Cumulative descriptor/name unit visits, including repeated passes.
    encoding::CharArrayLimits fieldString; ///< One type-10 prefix/body encoded bytes and units.
    std::size_t fieldBytes;                ///< Cumulative type-10 canonical byte work across passes.
    std::size_t fieldUnits;                ///< Cumulative type-10 unit visits across passes.
    std::size_t symbolUnits;               ///< Maximum units in one symbol.
    std::size_t totalSymbolUnits;          ///< Cumulative symbol unit visits, including equality/copy/encoding.
    std::size_t scratchBytes;              ///< Peak simultaneously owned private envelope byte storage.
    std::size_t scratchUnits;              ///< Peak private temporary/previous-symbol UTF-16 units.
    std::size_t workUnits;                 ///< Cumulative element/unit examination, copying, sizing and encoding.
};

/** Logical accounting; failed attempts retain work while output commits atomically. */
struct WriterCounters {
    std::size_t outputBytes = 0;      ///< Whole encoded bytes committed to successful chunks.
    std::size_t frames = 0;           ///< Successful envelopes, including markers.
    std::size_t records = 0;          ///< Attempted input data records and description IDs.
    std::size_t fields = 0;           ///< Attempted input data/description fields.
    std::size_t textUnits = 0;        ///< Charged descriptor/name visits.
    std::size_t fieldBytes = 0;       ///< Charged canonical type-10 byte work.
    std::size_t fieldUnits = 0;       ///< Charged type-10 unit visits.
    std::size_t symbolUnits = 0;      ///< Charged symbol visits, including comparison and copy.
    std::size_t workUnits = 0;        ///< Charged logical work, never refunded.
    std::size_t peakScratchBytes = 0; ///< Largest owned private envelope before transfer.
    std::size_t peakScratchUnits = 0; ///< Largest retained private message-local symbol.
};

/** I/O-free single-use RAW/MESSAGE composer; single-threaded and non-reentrant. */
class TapeWriter {
    struct Operation;
    OutgoingLayouts layouts_;
    OutgoingDescriptor descriptor_;
    WriterLimits limits_;
    std::vector<bool> announced_;
    WriterState state_ = WriterState::READY;
    WriterDiagnostic diagnostic_;
    WriterCounters counters_;

    /** Adopt a completely validated and independently owned configuration.
     * @param layouts Owned declarations.
     * @param descriptor Owned represented descriptor.
     * @param limits Required finite operation bounds.
     * @param counters Construction work already charged.
     */
    TapeWriter(OutgoingLayouts layouts, OutgoingDescriptor descriptor, WriterLimits limits, WriterCounters counters);
    /** Execute one complete operation and retain the first terminal failure.
     * @param operation Numeric action checkpoint.
     * @param ids Borrowed description IDs; unused for other operations.
     * @param records Borrowed records; unused for other operations.
     * @param heartbeat Borrowed fixed-size heartbeat; null for other operations.
     * @return Whole owned chunk or identical first diagnostic on later failures.
     */
    std::expected<WriterChunk, WriterDiagnostic> execute(WriterOperation operation, std::span<const std::int32_t> ids,
                                                         std::span<const OutgoingRecordView> records,
                                                         const TapeHeartbeat *heartbeat);

    public:
    /** Transfer ownership; borrowed access into the source expires.
     * @param other Writer to move; no bytes or completion are synthesized.
     */
    TapeWriter(TapeWriter &&other) noexcept;
    /** Copying state is unavailable; create independent bounded instances instead.
     * @param other Source writer.
     */
    TapeWriter(const TapeWriter &other) = delete;
    /** Assignment is unavailable for the single-use lifecycle.
     * @param other Source writer.
     * @return No result because assignment is deleted.
     */
    TapeWriter &operator=(const TapeWriter &other) = delete;
    /** Assignment is unavailable for the single-use lifecycle.
     * @param other Source writer.
     * @return No result because assignment is deleted.
     */
    TapeWriter &operator=(TapeWriter &&other) = delete;
    /** Own complete configuration before returning READY, without emitting bytes.
     * @param layouts Immutable declarations borrowed only for this call and independently copied.
     * @param descriptor Complete represented metadata borrowed only for this call.
     * @param limits Mandatory finite output/work/scratch allowances.
     * @return Owned READY writer or first construction diagnostic; allocation exceptions are translated.
     */
    [[nodiscard]] static std::expected<TapeWriter, WriterDiagnostic>
    create(const OutgoingLayouts &layouts, const OutgoingDescriptor &descriptor, WriterLimits limits);
    /** READY to ACTIVE, emitting the sole initial type-1 descriptor.
     * @return Whole envelope or first terminal diagnostic.
     */
    [[nodiscard]] std::expected<WriterChunk, WriterDiagnostic> begin();
    /** Announce one complete group only after whole-operation success.
     * @param senderIds Ordered distinct previously unannounced IDs; borrowed only for the call; empty is valid.
     * @return Whole type-2 envelope or first terminal diagnostic, without partial announcements.
     */
    [[nodiscard]] std::expected<WriterChunk, WriterDiagnostic> describe(std::span<const std::int32_t> senderIds);
    /** Emit one supported explicit type-zero heartbeat without synthesizing data time.
     * @param value Absent content, zero content or content 1 with one exact signed timestamp.
     * @return Whole envelope or first terminal diagnostic.
     */
    [[nodiscard]] std::expected<WriterChunk, WriterDiagnostic> heartbeat(const TapeHeartbeat &value);
    /** Emit one RAW_DATA batch with fresh message-local raw symbol state.
     * @param records Ordered views with local-order values; borrowed only during this call; empty is valid.
     * @return Whole envelope or first terminal diagnostic; failed calls expose no bytes.
     */
    [[nodiscard]] std::expected<WriterChunk, WriterDiagnostic> data(std::span<const OutgoingRecordView> records);
    /** Emit an empty envelope; a marker does not authenticate EOF.
     * @return Owned one-byte marker or first terminal diagnostic.
     */
    [[nodiscard]] std::expected<WriterChunk, WriterDiagnostic> marker();
    /** ACTIVE to FINISHED without bytes; FINISHED finish is idempotent and uncharged.
     * @return Empty owned chunk or first terminal diagnostic.
     */
    [[nodiscard]] std::expected<WriterChunk, WriterDiagnostic> finish();
    /** Inspect lifecycle without examining caller input.
     * @return Current READY/ACTIVE/FAILED/FINISHED state.
     */
    [[nodiscard]] WriterState state() const noexcept;
    /** Inspect the first terminal diagnostic.
     * @return Borrowed immutable evidence until destruction or move.
     */
    [[nodiscard]] const WriterDiagnostic &diagnostic() const noexcept;
    /** Inspect committed output and nonrefunded logical work.
     * @return Borrowed counters until destruction or move; values change during operations.
     */
    [[nodiscard]] const WriterCounters &counters() const noexcept;
};

} // namespace org::ttldtor::qdcxx::protocol
