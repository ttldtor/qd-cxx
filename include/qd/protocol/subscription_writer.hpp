// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/core/subscription.hpp"
#include "qd/protocol/tape_writer.hpp"

namespace org::ttldtor::qdcxx::protocol {

/** Supported plain binary subscription message identities. */
enum class SubscriptionMessage : std::int32_t {
    TICKER_ADD = 11,    ///< Add exact ticker interest.
    TICKER_REMOVE = 12, ///< Remove exact ticker interest.
    STREAM_ADD = 16,    ///< Add exact stream interest.
    STREAM_REMOVE = 17, ///< Remove exact stream interest.
    HISTORY_ADD = 21,   ///< Add indexed interest with a raw signed64 bound.
    HISTORY_REMOVE = 22 ///< Remove indexed interest without a wire time.
};

/** Outgoing plain binary DATA message identities. */
enum class LiveDataMessage : std::int32_t {
    TICKER_DATA = 10, ///< Ticker data.
    STREAM_DATA = 15, ///< Stream data.
    HISTORY_DATA = 20 ///< History data.
};

/** One exact outgoing DATA occurrence, borrowed only during composition. */
struct LiveDataRecordView {
    std::int32_t senderId;                        ///< Sender ID from this writer's outgoing type-2 layout.
    std::u16string_view symbol;                   ///< Nonnull, exact UTF-16 units.
    std::span<const core::FieldValueView> values; ///< Exact tagged values in local logical order.
    std::int32_t eventFlags = 0;                  ///< Raw flags allowed by the selected option.
    std::optional<std::int64_t> removalTime{};    ///< Required only for REMOVE_EVENT, with empty values.
};

/** Rejections publish no output and do not poison the composer. */
enum class SubscriptionWriterError {
    INVALID_STATE,           ///< Moved-from composer.
    INVALID_LAYOUT,          ///< Invalid sender/local identity, name, type or incomplete field mapping.
    INVALID_CONTEXT,         ///< Key provenance has no explicit outgoing record.
    INVALID_SELECTOR,        ///< Null selector or reserved leading asterisk.
    INVALID_HISTORY,         ///< Unexpected/missing bound or absent explicit indexed-time metadata.
    INVALID_MESSAGE,         ///< Unsupported message identity.
    INVALID_CONSUMPTION,     ///< Requested suffix consumption exceeds remaining bytes.
    RESOURCE_LIMIT,          ///< Finite storage, input or wire-domain allowance exceeded.
    WORK_LIMIT,              ///< Caller turn credits exhausted; previous charges are not refunded.
    ALLOCATION_FAILURE,      ///< Owned storage allocation failed before publication.
    UNSUPPORTED_SELECTOR,    ///< Explicit profile/family does not permit All; no output is published.
    INVALID_DATA,            ///< Invalid DATA symbol, tagged values, or removal shape.
    UNSUPPORTED_DATA_FLAGS,  ///< Raw flags outside the selected outgoing option.
    UNSUPPORTED_DATA_REMOVAL ///< Outgoing layout has a leading effective Event field.
};

/** Explicit caller-turn credits; reuse across calls, replenish only at the next chosen turn. */
struct SubscriptionWriterWork {
    std::size_t operations;  ///< Remaining create/compose/describe attempts.
    std::size_t visits;      ///< Remaining metadata, key and lookup/comparison visits.
    std::size_t textUnits;   ///< Remaining examined units, including primitive passes and comparisons.
    std::size_t outputBytes; ///< Remaining size, initialization and encoding byte work.
};

/** Required logical bounds; zero means zero. Allocator and exception bookkeeping are excluded. */
struct SubscriptionWriterLimits {
    std::size_t frameBytes;         ///< Body bytes including message identity; at most Java INT_MAX.
    std::size_t operationBytes;     ///< Complete envelope bytes per successful operation.
    std::size_t operationEntries;   ///< Borrowed input tuples or description IDs, counting duplicates.
    std::size_t symbolUnits;        ///< Units in one exact non-null symbol.
    encoding::UtfStringLimits text; ///< One description name's UTFString body bytes and units.
    std::size_t residentBytes;      ///< Aggregate backing bytes of all outputs from this composer, consumed or not.
    std::size_t peakBytes;          ///< Old outstanding output backing plus the new candidate backing.
    std::size_t outputs;            ///< Maximum simultaneously retained output owners, including the candidate.
};

/** Move-only whole envelope and partial-write cursor, independent of composer/input lifetime.
 * All access to this value and its originating composer/other outputs must be serialized.
 * Consuming bytes never acknowledges peer receipt and never releases retained backing storage.
 * bytes() permits retransmission of the original envelope even after complete consumption.
 */
class PendingSubscriptionOutput {
    struct Budget;
    std::shared_ptr<Budget> budget_;
    std::unique_ptr<std::byte[]> bytes_;
    std::size_t size_ = 0;
    std::size_t consumed_ = 0;
    friend class SubscriptionWriter;
    /** Adopt one fully encoded, charged allocation.
     * @param budget Shared fixed-size credit owner, retained after composer destruction.
     * @param bytes Exactly size backing bytes.
     * @param size Whole envelope byte count.
     */
    PendingSubscriptionOutput(std::shared_ptr<Budget> budget, std::unique_ptr<std::byte[]> bytes,
                              std::size_t size) noexcept;

    public:
    /** Transfer storage and cursor; source becomes empty.
     * @param other Output to transfer; existing spans remain valid while the destination owns it.
     */
    PendingSubscriptionOutput(PendingSubscriptionOutput &&other) noexcept;
    /** Release this allocation and transfer another; previous destination spans expire.
     * @param other Output to transfer; self-move has no effect.
     * @return This owner.
     */
    PendingSubscriptionOutput &operator=(PendingSubscriptionOutput &&other) noexcept;
    /** Release backing and its shared resident credits; all borrowed spans expire. */
    ~PendingSubscriptionOutput();
    /** Inspect the immutable original envelope.
     * @return Span valid until owner destruction or replacement; move transfers its lifetime.
     */
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept;
    /** Inspect the exact unsent suffix.
     * @return Borrowed suffix; consume changes the next returned suffix, not the underlying bytes.
     */
    [[nodiscard]] std::span<const std::byte> pendingBytes() const noexcept;
    /** Advance the local partial-write cursor without I/O or acknowledgement.
     * @param count Bytes accepted by the caller's local transport operation.
     * @return Success or INVALID_CONSUMPTION with the cursor unchanged.
     */
    [[nodiscard]] std::expected<void, SubscriptionWriterError> consume(std::size_t count) noexcept;
};

/** I/O-free live composer; no descriptor negotiation, announcement state or implicit conversion.
 * Inputs are borrowed unchanged only for the call. Calls are non-reentrant; calls and output destruction are
 * serialized. Rejected calls preserve all outputs and configuration; successfully charged work is not refunded.
 * Metadata storage is bounded by LayoutLimits; all outstanding byte backing is jointly bounded
 * by residentBytes/peakBytes, including caller-retained consumed outputs. Fixed schema provenance
 * and one shared credit owner survive while referenced; allocator overhead is not measured RSS.
 */
class SubscriptionWriter {
    struct Operation;
    core::LocalSchema schema_;
    std::vector<OutgoingRecord> records_;
    SubscriptionWriterLimits limits_;
    std::shared_ptr<PendingSubscriptionOutput::Budget> budget_;
    std::size_t storageBytes_ = 0; ///< Immutable actual-capacity total, cached before the composer is published.
    core::SubscriptionSelectorPolicy selectorPolicy_ =
        core::SubscriptionSelectorPolicy::EXACT_ONLY; ///< Immutable peer assertion.
    /** Measure adopted immutable configuration once, after its construction work is charged.
     * @return Conservative backing and fixed metadata bytes, excluding pending byte arrays.
     */
    [[nodiscard]] std::size_t measureStorageBytes() const noexcept;
    /** Adopt independently owned validated metadata.
     * @param schema Bounded schema copy preserving provenance.
     * @param records Bounded complete outgoing layouts.
     * @param limits Operation/output limits.
     * @param budget Fixed shared output accounting owner.
     * @param policy Immutable controlled-peer symbol/selector assertion, not negotiated capability.
     */
    SubscriptionWriter(core::LocalSchema schema, std::vector<OutgoingRecord> records, SubscriptionWriterLimits limits,
                       std::shared_ptr<PendingSubscriptionOutput::Budget> budget,
                       core::SubscriptionSelectorPolicy policy);
    /** Compose one atomic operation after validation and exact sizing.
     * @param message Subscription identity, or absent for descriptions.
     * @param requests Borrowed ordered keys; unused for descriptions.
     * @param ids Borrowed sender IDs; unused for subscriptions.
     * @param dataMessage DATA identity, absent for descriptions and subscriptions.
     * @param data Borrowed DATA records; unused for descriptions and subscriptions.
     * @param historySnapshot Whether the caller enabled the hs option for outgoing DATA.
     * @param work Mutable caller-turn credits, never refunded.
     * @return Independent complete output or typed rejection.
     */
    std::expected<PendingSubscriptionOutput, SubscriptionWriterError>
    execute(std::optional<SubscriptionMessage> message, std::span<const core::SubscriptionRequest> requests,
            std::span<const std::int32_t> ids, std::optional<LiveDataMessage> dataMessage,
            std::span<const LiveDataRecordView> data, bool historySnapshot, SubscriptionWriterWork &work);

    public:
    /** Transfer configuration and shared credits; source rejects subsequent operations.
     * @param other Composer to transfer; existing outputs are unaffected.
     */
    SubscriptionWriter(SubscriptionWriter &&other) noexcept = default;
    /** Validate complete one-to-one names/types/field coverage and own bounded metadata.
     * Indexed BINARY_FIELDS_V1 metadata is admitted for descriptions without enabling tape writing.
     * IDs are sparse bounded declarations, never ID-indexed allocations. Sender IDs must be in
     * 0..INT32_MAX-1; this does not guarantee a peer's heap usage for large accepted IDs.
     * @param schema Immutable validated local schema borrowed only during the call.
     * @param records Explicit outgoing layouts borrowed only during the call.
     * @param layoutLimits Required complete metadata-copy bounds.
     * @param limits Required finite per-operation/aggregate byte bounds.
     * @param work Mutable explicit caller-turn credits; failures retain successful charges.
     * @param policy Immutable profile selected before activation. Canonical profiles encode Exact for all six messages;
     * only CANONICAL_STREAM_ALL_V1 permits All under STREAM16/17. Default retains every legacy FD byte.
     * @return Owned composer or typed validation/resource/allocation error.
     */
    [[nodiscard]] static std::expected<SubscriptionWriter, SubscriptionWriterError>
    create(const core::LocalSchema &schema, std::span<const OutgoingRecord> records, LayoutLimits layoutLimits,
           SubscriptionWriterLimits limits, SubscriptionWriterWork &work,
           core::SubscriptionSelectorPolicy policy = core::SubscriptionSelectorPolicy::EXACT_ONLY);
    /** Encode one message with fresh symbol state, preserving order and duplicate tuples.
     * @param message One of six supported subscription identities.
     * @param requests Explicit keys; only HISTORY_ADD requires/allows a raw bound. Both history
     * forms require explicit indexed metadata. Non-null empty/NUL/unpaired UTF-16 is preserved.
     * @param work Mutable explicit caller-turn credits.
     * @return Whole envelope with zero cursor, or rejection without partial output.
     */
    [[nodiscard]] std::expected<PendingSubscriptionOutput, SubscriptionWriterError>
    compose(SubscriptionMessage message, std::span<const core::SubscriptionRequest> requests,
            SubscriptionWriterWork &work);
    /** Encode a nonempty DATA envelope in outgoing layout order with fresh symbol and flag state.
     * The caller orders the matching descriptions before these bytes on the connection.
     * @param message One of the three supported DATA identities.
     * @param records Borrowed occurrences, including exact values or removal times.
     * @param historySnapshot Whether hs is enabled for this direction. Unsupported flags are rejected.
     * @param work Mutable caller-turn credits, including failures.
     * @return Immutable whole envelope or typed rejection without partial output.
     */
    [[nodiscard]] std::expected<PendingSubscriptionOutput, SubscriptionWriterError>
    composeData(LiveDataMessage message, std::span<const LiveDataRecordView> records, bool historySnapshot,
                SubscriptionWriterWork &work);
    /** Encode explicit descriptions, without claiming they were sent or acknowledged.
     * @param senderIds Ordered known sender IDs, borrowed only during the call; repeats are preserved.
     * @param work Mutable explicit caller-turn credits.
     * @return One type-2 envelope or typed rejection without partial output.
     */
    [[nodiscard]] std::expected<PendingSubscriptionOutput, SubscriptionWriterError>
    describe(std::span<const std::int32_t> senderIds, SubscriptionWriterWork &work);
    /** Inspect currently retained output allocations without examining caller inputs.
     * @return Aggregate backing bytes, or zero for a moved-from composer.
     */
    [[nodiscard]] std::size_t retainedBytes() const noexcept;

    /** Inspect cached configuration backing in constant time, including capacities and fixed metadata.
     * Shared schema provenance and inline string capacity may be counted more than once.
     * @return Configuration bytes excluding pending byte arrays; zero for a moved-from composer.
     */
    [[nodiscard]] std::size_t storageBytes() const noexcept;

    /** Validate immutable provenance before transferring this composer into a session.
     * @param schema Schema supplying the expected local identity.
     * @return True for a usable composer retaining this exact schema provenance.
     */
    [[nodiscard]] bool matchesSchema(const core::LocalSchema &schema) const noexcept;

    /** Inspect the finite maximum whole-envelope allocation for one operation.
     * @return Configured per-operation byte limit; zero for a moved-from composer.
     */
    [[nodiscard]] std::size_t maximumOutputBytes() const noexcept;
};

} // namespace org::ttldtor::qdcxx::protocol
