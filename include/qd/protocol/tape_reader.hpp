// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/stream_data.hpp"
#include <string_view>

namespace org::ttldtor::qdcxx::protocol {
/** Explicit container policy, independent of the caller's local field profile. */
enum class TapeMode {
    LEGACY,           ///< Restricted untimed STREAM_DATA behavior.
    RAW_MESSAGE_V1,   ///< Bounded RAW_DATA with MESSAGE heartbeats; see RAW-TAPE.
    BINARY_CATALOG_V1 ///< Explicit finite binary families, descriptor deltas and self-described headerless input.
};

/** Explicit broader discovery, independent from the unchanged ordinary constructor. */
enum class TapeDiscovery { BINARY_CATALOG_V1 };

/** Fixed-size owning timing state shared by all records in one data result. */
struct TapeTiming {
    std::optional<std::int64_t> timestampMs; ///< Exact signed wire milliseconds; absent before a timestamp.
    std::int64_t eventTimeSequence = 0;      ///< Java-compatible saturated seconds and millisecond packing.
    /** Recover the delivered event time, which can differ from a saturated wire timestamp.
     * @return Signed milliseconds represented by the packed cursor sequence.
     */
    [[nodiscard]] std::int64_t eventTimeMs() const noexcept;
};

/** Accepted explicit type-zero heartbeat, distinct from an empty envelope marker. */
struct TapeHeartbeat {
    std::optional<std::int32_t> content;     ///< Absent for zero payload bytes, otherwise exact supported bits 0 or 1.
    std::optional<std::int64_t> timestampMs; ///< Present only for content bit 1; exact wire milliseconds.
};

/** Required finite descriptor text and collection bounds. */
struct TapeDescriptorLimits {
    std::size_t properties;         ///< Global and message property pairs combined.
    std::size_t messages;           ///< Send and receive entries combined.
    encoding::UtfStringLimits text; ///< One encoded/decoded string.
    std::size_t textUnits;          ///< All successfully decoded strings combined.
};

/** Required finite tape storage and cumulative work bounds. */
struct TapeLimits {
    FrameLimits frame;               ///< FrameReader storage configuration.
    std::size_t inputBytes;          ///< Total accepted bytes.
    std::size_t frames;              ///< All envelopes, including empty markers.
    TapeDescriptorLimits descriptor; ///< Initial descriptor bounds.
    ContextLimits context;           ///< Schema and retained descriptions/actions/names.
    std::size_t constructionUnits; ///< Cumulative construction units; discovery also charges raw payload/context work.
    StreamDataLimits data;         ///< Aggregate file work/output bounds and per-symbol bounds.
    std::size_t timingEvents = 0; ///< RAW/broad metadata attempts: explicit heartbeats and data frames; zero is finite.
    std::size_t snapshotUnits =
        0; ///< Broad schema/description/context/descriptor/temporary peak units; zero is finite.
};
/** Terminal categories, independent of Java exception classes. */
enum class TapeError {
    NONE,              ///< No failure.
    MALFORMED,         ///< Invalid/truncated wire input, duplicate or undescribed ID.
    UNSUPPORTED,       ///< Explicit profile or sequencing policy.
    RESOURCE_LIMIT,    ///< Finite logical bound exceeded.
    ALLOCATION_FAILURE ///< Owned storage allocation failed.
};
/** Incremental result state. */
enum class TapeStatus {
    NEED_INPUT, ///< No complete event yet; empty feed is not EOF.
    EVENT,      ///< One complete envelope was dispatched.
    FAILED,     ///< First terminal error; no further input is accepted.
    FINISHED    ///< Clean explicit EOF with an accepted header.
};

/** Bounded profile refusal detail, independent of numeric diagnostics and offsets. */
enum class TapeRejection {
    NONE,          ///< No additional profile reason.
    TIME,          ///< Unsupported tape timing.
    OPTIONS,       ///< Protocol options or raw flags violate the selected bounded mode.
    MESSAGE,       ///< Unsupported message or message-set policy.
    TYPE,          ///< Unsupported full serial identity or excluded field conversion.
    LOCAL_PROFILE, ///< Unsupported caller local schema profile.
    DESCRIPTOR,    ///< Unsupported descriptor property or extension.
    SEQUENCE       ///< Initial/repeated header or other sequencing violation.
};

/** Owning diagnostic header text; missing version is distinct from empty. */
struct TapeDescriptor {
    std::optional<std::u16string> version; ///< Opaque text, never a support test.
    bool explicitTimeNone = false;         ///< Whether time=NONE was present.
    bool explicitStreamSend = false;       ///< Whether the send set explicitly contained STREAM_DATA.
    std::optional<std::u16string> options; ///< Exact accepted option text; absent differs from empty.
    std::optional<std::u16string> time;    ///< Exact accepted timing text, without case normalization.
    bool explicitRawSend = false;          ///< Whether the send set explicitly contained RAW_DATA.
    bool explicitTickerSend = false;       ///< Effective known TICKER_DATA declaration.
    bool explicitHistorySend = false;      ///< Effective known HISTORY_DATA declaration.
};

/** First error and exact separate framing/payload positions. */
struct TapeDiagnostic {
    TapeError error = TapeError::NONE;         ///< Terminal category.
    std::string_view stage = "none";           ///< Static checkpoint label, no borrowed input.
    std::size_t detail = 0;                    ///< Nested parser enum ordinal when applicable.
    std::uint64_t frameStart = 0;              ///< Actual start of failing envelope, or EOF position.
    std::uint64_t accepted = 0;                ///< Total accepted file bytes, not payload examination.
    std::optional<std::uint64_t> payloadStart; ///< Actual payload start for complete messages.
    std::optional<std::size_t> consumed;       ///< Payload-relative committed examination.
    std::optional<std::size_t> errorOffset;    ///< Payload-relative primitive/checkpoint position.
    TapeRejection rejection =
        TapeRejection::NONE; ///< Additive bounded reason; legacy serialized fields stay unchanged.
};

/** At most one owning event and its completed data prefix. */
struct TapeFeedResult {
    std::size_t accepted = 0;                   ///< Bytes copied from this feed only.
    TapeStatus status = TapeStatus::NEED_INPUT; ///< Reader state after this call.
    std::optional<Frame> frame;                 ///< Owning envelope, also present on payload failure.
    std::vector<StreamDataRecord> records;      ///< Complete records, never a partial current record.
    std::optional<TapeTiming> timing;           ///< Owned RAW data timing, including a completed prefix on failure.
    std::optional<TapeHeartbeat> heartbeat;     ///< Accepted explicit heartbeat, even with no subsequent data.
};

/** Bounded single-threaded reader; active replacement is explicit-mode only, with no reset or recovery API. */
class TapeReader {
    TapeLimits limits_;
    TapeMode mode_;
    core::LocalSchema schema_;
    bool discoverSchema_ = false; ///< Explicit ordinary wire-schema policy, never inferred from input.
    FrameReader framer_;
    std::vector<WireRecordDescription> descriptions_;
    std::optional<StreamDataContext> context_;
    std::optional<TapeDescriptor> descriptor_;
    TapeStatus status_ = TapeStatus::NEED_INPUT;
    TapeDiagnostic diagnostic_;
    std::size_t frames_ = 0;
    std::size_t markers_ = 0;
    std::size_t construction_ = 0;
    std::size_t snapshotUnits_ =
        0; ///< Current new-mode metadata, excluding caller-owned copies and frame/data storage.
    std::size_t records_ = 0;
    std::size_t fields_ = 0;
    std::size_t emitted_ = 0;
    std::size_t values_ = 0;
    std::size_t symbols_ = 0;
    std::size_t fieldBytes_ = 0;
    std::size_t fieldUnits_ = 0;
    std::size_t ownedFieldUnits_ = 0;
    std::size_t payloadEncodedBytes_ = 0;
    std::size_t payloadWorkBytes_ = 0;
    std::size_t ownedPayloadBytes_ = 0;
    TapeTiming timing_;
    std::size_t timingEvents_ = 0;
    /** Set the first failure.
     * @param error Category.
     * @param stage Static checkpoint.
     * @param detail Nested enum ordinal.
     */
    void fail(TapeError error, std::string_view stage, std::size_t detail = 0) noexcept;
    /** Dispatch one complete envelope.
     * @param result Owns the frame and receives completed data.
     */
    void dispatch(TapeFeedResult &result);
    /** Prepare and publish one complete discovery batch; failure retains the old snapshot.
     * @param incoming Owned parsed batch, moved only after bounded admission.
     */
    void discoverDescriptions(std::vector<WireRecordDescription> &incoming);
    /** Validate and publish one new-mode active-description generation.
     * @param payload Complete borrowed description bytes; no publication occurs on any failure.
     */
    void replaceDescriptions(std::span<const std::byte> payload);

    public:
    /** Copy a validated local schema within bounds and allocate framing storage.
     * @param schema Borrowed validated schema, copied before returning.
     * @param limits Mandatory finite bounds; invalid configuration throws invalid_argument.
     * @param mode Explicit container policy; default preserves the original untimed behavior.
     * BINARY_CATALOG_V1 requires BINARY_FIELDS_V1 and finite snapshotUnits; it selects generic
     * RAW/TICKER/STREAM/HISTORY, known descriptor deltas, self-described headerless input,
     * NONE/MESSAGE timing, known flags/removal time and immutable active replacements.
     * Indexed metadata stays caller-proven; FIELD and unknown descriptor/control features are refused.
     * Indexed local schemas are refused by both older modes.
     * Allocation failure before input may throw bad_alloc.
     */
    TapeReader(const core::LocalSchema &schema, TapeLimits limits, TapeMode mode = TapeMode::LEGACY);
    /** Explicitly discover ordinary exact fields from accepted descriptions; see WIRE-INVENTORY.
     * Starts with an empty EXTENDED_FIELDS_V1 schema. Invalid configuration throws
     * invalid_argument; allocation before input may throw bad_alloc. Single-threaded.
     * @param limits Mandatory finite parsing, final schema and cumulative construction bounds.
     * @param mode Explicit container policy, independent of schema discovery.
     * BINARY_CATALOG_V1 is refused; broader discovery is not selected through this constructor.
     */
    explicit TapeReader(TapeLimits limits, TapeMode mode = TapeMode::LEGACY);
    /** Discover exact binary fields without inferring indexed metadata from wire names.
     * @param limits Explicit finite parsing, payload, snapshot and construction bounds; zero stays zero.
     * @param discovery Explicit binary policy; invalid values throw invalid_argument before input.
     * Pre-input allocation can throw bad_alloc. Every discovered record hasTime=false.
     */
    explicit TapeReader(TapeLimits limits, TapeDiscovery discovery);
    /** Accept up to one envelope; caller resubmits any unaccepted suffix.
     * @param input Borrowed bytes, not retained by reference.
     * @return Owned event/prefix and exact accepted count; terminal failures accept zero later.
     */
    [[nodiscard]] TapeFeedResult feed(std::span<const std::byte> input);
    /** Declare actual EOF; idempotent, no recovery.
     * @return FINISHED only for clean EOF after an accepted header or, in broad mode, a committed
     * nonempty description batch. Empty batches, markers and heartbeats alone do not authenticate EOF.
     */
    TapeStatus finish() noexcept;

    /** Inspect first failure.
     * @return Borrowed diagnostic valid until the reader is destroyed.
     */
    [[nodiscard]] const TapeDiagnostic &diagnostic() const noexcept {
        return diagnostic_;
    }

    /** Inspect accepted descriptor.
     * @return Borrowed owning header storage or absent before acceptance. Descriptor/text views
     * expire on successful descriptor publication, move, assignment or destruction; failed updates
     * preserve addresses. Descriptor-only updates do not invalidate schema/context views.
     */
    [[nodiscard]] const std::optional<TapeDescriptor> &descriptor() const noexcept {
        return descriptor_;
    }

    /** Inspect the last fully accepted timestamp.
     * @return Fixed-size owning state, unchanged after a failed heartbeat.
     */
    [[nodiscard]] const TapeTiming &timing() const noexcept {
        return timing_;
    }

    /** Inspect cumulative metadata work.
     * @return Charged RAW data/explicit-heartbeat attempts, including later payload failures.
     */
    [[nodiscard]] std::size_t timingEvents() const noexcept {
        return timingEvents_;
    }

    /** Inspect the current immutable snapshot.
     * @return Borrowed context until next successful description update or destruction.
     */
    [[nodiscard]] const StreamDataContext *context() const noexcept {
        return context_ ? &*context_ : nullptr;
    }

    /** Inspect owned immutable local declarations, including an empty initial schema.
     * @return Borrowed schema; views expire on successful discovery/catalog updates, destruction, move or assignment.
     * Failed updates retain declarations and provenance. boundedCopy explicitly retains an old snapshot.
     */
    [[nodiscard]] const core::LocalSchema &schema() const noexcept {
        return schema_;
    }

    /** Inspect exact accepted source declarations without reparsing or copying.
     * @return Borrowed append order in old modes; active first-seen sender order in BINARY_CATALOG_V1.
     * Expires on successful description updates, destruction, move or assignment.
     * A rejected batch leaves this storage intact. Sender IDs are sparse identities, never local positions.
     */
    [[nodiscard]] std::span<const WireRecordDescription> descriptions() const noexcept {
        return descriptions_;
    }

    /** Inspect accepted bytes.
     * @return Exact cumulative file position.
     */
    [[nodiscard]] std::uint64_t acceptedBytes() const noexcept {
        return framer_.totalAccepted();
    }

    /** Inspect envelope charges.
     * @return Frames including markers and complete failing frames.
     */
    [[nodiscard]] std::size_t frameCount() const noexcept {
        return frames_;
    }

    /** Inspect marker presence independently of parsing completion.
     * @return Empty envelopes successfully accepted after the header.
     */
    [[nodiscard]] std::size_t markerCount() const noexcept {
        return markers_;
    }

    /** Inspect record work.
     * @return Charged attempts including skips and later failures.
     */
    [[nodiscard]] std::size_t processedRecords() const noexcept {
        return records_;
    }

    /** Inspect field work.
     * @return Charged attempts including skips and repeated targets.
     */
    [[nodiscard]] std::size_t wireFields() const noexcept {
        return fields_;
    }

    /** Inspect cumulative type10 byte work.
     * @return Examined character-array bytes including prefixes, skips, replacements and failed reads.
     */
    [[nodiscard]] std::size_t fieldBytes() const noexcept {
        return fieldBytes_;
    }

    /** Inspect cumulative string-unit work.
     * @return Admitted units, never refunded for skipped, replaced or failed values.
     */
    [[nodiscard]] std::size_t fieldUnits() const noexcept {
        return fieldUnits_;
    }

    /** Inspect cumulative published string storage.
     * @return Logical UTF-16 units in all completed output records, including earlier returned events.
     */
    [[nodiscard]] std::size_t ownedFieldUnits() const noexcept {
        return ownedFieldUnits_;
    }

    /** Inspect cumulative new-payload encoded work.
     * @return Actual examined prefix/body bytes, including skips and the terminal failing frame.
     */
    [[nodiscard]] std::size_t payloadEncodedBytes() const noexcept {
        return payloadEncodedBytes_;
    }

    /** Inspect cumulative new-payload body work.
     * @return Admitted declared body bytes, never refunded for skips, replacements or failures.
     */
    [[nodiscard]] std::size_t payloadWorkBytes() const noexcept {
        return payloadWorkBytes_;
    }

    /** Inspect cumulative published new-payload storage.
     * @return Logical payload bytes in all completed records, including previously returned outputs.
     */
    [[nodiscard]] std::size_t ownedPayloadBytes() const noexcept {
        return ownedPayloadBytes_;
    }

    /** Inspect reconstruction work.
     * @return Admitted logical elements, including discovery raw payloads, temporary copies and context creation.
     */
    [[nodiscard]] std::size_t constructionUnits() const noexcept {
        return construction_;
    }
};
} // namespace org::ttldtor::qdcxx::protocol
