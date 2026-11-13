// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/statistics.hpp"
#include "qd/protocol/subscription_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace org::ttldtor::qdcxx::protocol {
namespace detail {
class TapeEventBridge;
struct LiveAuthorization;
} // namespace detail

/** Detached counters at framing and atomic DATA-commit boundaries, before projection or fan-out. */
struct LiveInputStatistics {
    core::StatisticCounter messages;    ///< Structurally accepted complete QTP frames, including rejected payloads.
    core::StatisticCounter dataRecords; ///< Validated wire occurrences, including skipped local records.
};

/** Pure live-session failures; diagnostics never retain credential or peer text. */
enum class LiveError {
    NONE,                    ///< No failure.
    INVALID_STATE,           ///< Closed, failed or moved-from session.
    INVALID_CONFIGURATION,   ///< Incompatible schema/composer, profile or finite limits.
    MALFORMED,               ///< Invalid or truncated selected wire grammar.
    UNSUPPORTED,             ///< Actual wire message, descriptor extension or full type outside the selected profile.
    NOT_READY,               ///< Required peer data/send or subscription receive-add readiness is absent.
    AUTHENTICATION_REQUIRED, ///< The selected authentication policy has not completed.
    CREDENTIALS_FORBIDDEN,   ///< Credential output lacks explicit verified-TLS eligibility.
    RESOURCE_LIMIT,          ///< Configured requested backing, owner, frame or logical bound exhausted.
    WORK_LIMIT,              ///< Explicit caller-turn credits exhausted, without refunding prior charges.
    ALLOCATION_FAILURE,      ///< Admitted backing could not be allocated.
    INVALID_CONSUMPTION,     ///< A local partial-write cursor cannot advance that far.
    IDENTITY_EXHAUSTED       ///< A binding-generation or wire-offset counter cannot advance honestly.
};

/** One session's immutable explicit credential eligibility, supplied by the owning adapter. */
enum class LiveCredentialPolicy {
    CREDENTIAL_FREE_PLAIN, ///< Plain transport may emit no credentials.
    VERIFIED_TLS_BASIC     ///< The trusted adapter has verified TLS and permits the selected Basic authorization.
};

/** Effective protocol-derived authentication state; no timer or automatic retry is implied. */
enum class LiveAuthentication {
    UNCHALLENGED,          ///< No authentication property has been received.
    REQUIRED,              ///< A nonempty peer authentication reason is effective.
    AUTHORIZATION_EMITTED, ///< Local Basic bytes were composed, without peer acknowledgement.
    ACKNOWLEDGED           ///< The effective peer authentication property is explicitly empty.
};

/** Mutable finite credits for one caller-selected turn, reused across calls without refunds. */
struct LiveWork {
    std::size_t operations; ///< Remaining public operation attempts.
    std::size_t visits;     ///< Remaining record, field, property and comparison visits.
    std::size_t textUnits;  ///< Remaining examined/copied UTF-16 units.
    std::size_t bytes;      ///< Remaining input, parsing, output and arena initialization byte work.
};

/** Explicit finite live bounds; a continuing feed has no lifetime/file quota. */
struct LiveLimits {
    FrameLimits frame;              ///< Per-frame envelope bounds and fixed framing allocation.
    TapeDescriptorLimits protocol;  ///< Complete descriptor collection and text bounds.
    DescriptionLimits descriptions; ///< One incoming description batch bounds.
    ContextLimits context;          ///< Complete retained peer-binding generation bounds.
    StreamDataLimits data;          ///< One complete data-frame logical work and value bounds.
    std::size_t residentBytes; ///< All epoch backing reservations, including retained old owners and fixed metadata.
    std::size_t owners;        ///< Simultaneous candidate/published snapshot, event and pending-output owners.
    std::size_t snapshotBytes; ///< Fixed backing for one whole effective description/protocol/binding snapshot.
    std::size_t scratchBytes;  ///< Fixed temporary control-parse arena; coexists with old and candidate owners.
    std::size_t
        eventBytes; ///< Fixed backing for one decoded data/control event, including all container reallocations.
    std::size_t outputBytes; ///< Maximum whole control envelope; subscription bounds also remain in the composer.
    LiveCredentialPolicy credentials; ///< Explicit adapter-supplied credential eligibility.
    FieldTimePolicy fieldTime;        ///< Explicit virtual-prefix policy, never inferred from peer text.
    DataEventPolicy events;           ///< Explicit zero-flags or finite binary metadata policy.
    bool historySnapshot;             ///< Whether local negotiation advertises the selected hs capability.
};

/** Static numeric first-error evidence; input credentials never enter this value. */
struct LiveDiagnostic {
    LiveError error = LiveError::NONE; ///< First terminal category.
    std::string_view stage = "none";   ///< Static checkpoint label.
    std::uint64_t frameStart = 0;      ///< Failing envelope start, independent of payload examination.
    std::uint64_t accepted = 0;        ///< Honest cumulative accepted bytes in this epoch.
    std::size_t consumed = 0;          ///< Payload-relative examination checkpoint.
    std::size_t detail = 0;            ///< Nested parser enum ordinal, when applicable.
};

/** Opaque owning epoch/generation token; equal integers never make different sessions identical.
 * Copies and destruction, like every related session operation, require serialized access.
 */
class LiveIdentity {
    void *epoch_ = nullptr;        ///< Opaque serialized epoch lease, never caller-forgeable.
    std::uint64_t generation_ = 0; ///< Non-reused effective peer-snapshot generation.
    friend class LiveSession;
    friend class LiveSnapshot;
    /** Retain an existing epoch identity.
     * @param epoch Opaque live epoch ledger.
     * @param generation Effective peer-snapshot generation.
     */
    LiveIdentity(void *epoch, std::uint64_t generation) noexcept;

    public:
    /** Construct an invalid identity without allocating. */
    LiveIdentity() noexcept = default;
    /** Retain the source epoch without allocating.
     * @param other Source identity.
     */
    LiveIdentity(const LiveIdentity &other) noexcept;
    /** Transfer the epoch lease.
     * @param other Source identity, made invalid.
     */
    LiveIdentity(LiveIdentity &&other) noexcept;
    /** Replace a lease with an independent source lease.
     * @param other Source identity.
     * @return This identity.
     */
    LiveIdentity &operator=(const LiveIdentity &other) noexcept;
    /** Release the old lease and transfer the source.
     * @param other Source identity, made invalid.
     * @return This identity.
     */
    LiveIdentity &operator=(LiveIdentity &&other) noexcept;
    /** Release only fixed opaque epoch ownership, never a session or socket. */
    ~LiveIdentity();
    /** Compare owning epoch and generation identities.
     * @param other Token to compare; invalid identities never match.
     * @return True only for the same valid epoch and generation.
     */
    [[nodiscard]] bool matches(const LiveIdentity &other) const noexcept;
    /** Compare opaque epochs independently of effective binding generations.
     * @param other Owning identity to compare; invalid identities never match.
     * @return True only when both identities retain the same epoch, including generation zero.
     */
    [[nodiscard]] bool sameEpoch(const LiveIdentity &other) const noexcept;
    /** Inspect the monotonically increasing generation within its opaque epoch.
     * @return Generation number; zero is also the initial empty generation.
     */
    [[nodiscard]] std::uint64_t generation() const noexcept;
    /** Inspect conservative work for releasing this fixed opaque epoch reference.
     * @return Fixed ledger disposal bytes for a retained epoch, or zero when empty. This is not a
     * reservation refund or a claim that other epoch/snapshot/event owners have retired.
     */
    [[nodiscard]] std::size_t releaseBytes() const noexcept;
};

/** Exact borrowed property text; Java null and non-null empty remain distinct. */
struct LivePropertyView {
    std::optional<std::u16string_view> key;   ///< Exact nullable key, without normalization.
    std::optional<std::u16string_view> value; ///< Exact nullable value; authorization values are never published.
};

/** Borrowed message descriptor; properties contain local overrides, not copied inheritance. */
struct LiveMessageView {
    std::int32_t id; ///< Exact numeric message identity; unknown declarations remain informational.
    std::optional<std::u16string_view> name; ///< Exact nullable name; only matching known ID/name enables capability.
    std::span<const LivePropertyView> properties; ///< Exact local property overrides.
};

/** Borrowed complete or delta descriptor; no wire reset/full discriminator is invented. */
struct LiveProtocolView {
    std::span<const LivePropertyView> properties; ///< Top-level exact bounded properties.
    std::span<const LiveMessageView> send;        ///< Directional messages sent by the descriptor owner.
    std::span<const LiveMessageView> receive;     ///< Directional messages received by the descriptor owner.
};

/** Borrowed immutable raw peer field declaration. */
struct LiveFieldDescriptionView {
    std::u16string_view name;  ///< Exact nonempty field name.
    std::int32_t serialTypeId; ///< Full serial identity, never masked.
};

/** Borrowed immutable raw peer record declaration. */
struct LiveRecordDescriptionView {
    std::int32_t senderId;                            ///< Directional peer ID.
    std::u16string_view name;                         ///< Exact nonempty record name.
    std::span<const LiveFieldDescriptionView> fields; ///< Exact physical wire order.
};

/** Owning immutable effective peer snapshot, independent of future frames or session destruction. */
class LiveSnapshot {
    struct State;
    State *state_ = nullptr; ///< Serialized intrusive owner; copies allocate no new storage.
    friend class LiveSession;
    friend class LiveEvent;
    friend class detail::TapeEventBridge;
    /** Adopt an already counted snapshot owner.
     * @param state Complete immutable snapshot, or null.
     */
    explicit LiveSnapshot(State *state) noexcept;

    public:
    /** Construct an empty snapshot handle without allocating. */
    LiveSnapshot() noexcept = default;
    /** Retain a complete immutable owner.
     * @param other Source handle.
     */
    LiveSnapshot(const LiveSnapshot &other) noexcept;
    /** Transfer the complete owner.
     * @param other Source handle, made empty.
     */
    LiveSnapshot(LiveSnapshot &&other) noexcept;
    /** Replace ownership while preserving independent old handles.
     * @param other Source handle.
     * @return This handle.
     */
    LiveSnapshot &operator=(const LiveSnapshot &other) noexcept;
    /** Transfer ownership and release the previous handle.
     * @param other Source handle, made empty.
     * @return This handle.
     */
    LiveSnapshot &operator=(LiveSnapshot &&other) noexcept;
    /** Release this handle and its requested-capacity reservation after the last lease. */
    ~LiveSnapshot();
    /** Borrow exact effective descriptor properties and directional declarations.
     * @return Views valid until this handle is destroyed or replaced; empty for an empty handle.
     */
    [[nodiscard]] LiveProtocolView protocol() const noexcept;
    /** Borrow raw descriptions in effective sender-table order.
     * @return Views valid until this handle is destroyed or replaced.
     */
    [[nodiscard]] std::span<const LiveRecordDescriptionView> descriptions() const noexcept;
    /** Obtain an independent opaque generation token.
     * @return Owning epoch/generation identity, invalid for an empty handle.
     */
    [[nodiscard]] LiveIdentity identity() const noexcept;
};

/** Selected full heartbeat fields; engaged zero differs from an absent field. */
struct LiveHeartbeat {
    bool hasPayload = true;                 ///< False composes type0 with no payload, distinct from mask-zero payload.
    std::optional<std::int64_t> timeMillis; ///< Raw compact-long milliseconds.
    std::optional<std::int32_t> timeMark;   ///< Raw compact-int mark.
    std::optional<std::int32_t> deltaMark;  ///< Raw compact-int delta.
    std::optional<std::int32_t> lagMark;    ///< Raw compact-int lag.
};

/** Borrowed exact generic value owned by its immutable event. */
struct LiveFieldValueView {
    core::SerialFieldType type;                      ///< Exact full serial identity.
    std::int64_t raw;                                ///< Raw scalar bits, without business conversion.
    std::optional<std::u16string_view> units;        ///< Null versus empty exact text.
    std::optional<std::span<const std::byte>> bytes; ///< Null versus empty exact opaque payload.
};

/** Borrowed admitted local record; every span/text remains owned by the containing event. */
struct LiveRecordView {
    std::int32_t localId;                            ///< Immutable local record identity.
    std::int32_t senderId;                           ///< Directional peer identity, never delivery identity.
    core::RecordContext context;                     ///< Owning local provenance.
    std::u16string_view symbol;                      ///< Exact symbol, including empty/NUL/unpaired units.
    std::int32_t cipher;                             ///< Raw Java cipher metadata.
    std::int32_t eventFlags;                         ///< Exact selected raw flags.
    std::span<const std::int32_t> values;            ///< Legacy compact-only values.
    std::span<const LiveFieldValueView> typedValues; ///< Exact generic tagged values.
    std::optional<std::int64_t> removalTime;         ///< Exact optional raw removal time.
    std::optional<FieldEventTime> fieldTime;         ///< Exact optional virtual-prefix metadata.
    std::optional<std::int64_t> tapeMessageTimeMs{}; ///< Optional tape MESSAGE cursor time, separate from FIELD.
};

/** Whole admitted envelope kind; controls are never application records. */
enum class LiveEventKind {
    EMPTY_HEARTBEAT, ///< Envelope with zero body and no message type.
    HEARTBEAT,       ///< Explicit type0, with or without selected payload fields.
    PROTOCOL,        ///< Atomically published effective descriptor.
    DESCRIPTIONS,    ///< Atomically published peer sender/binding generation.
    DATA             ///< Whole successfully decoded data frame.
};

/** Move-only immutable admitted frame owner; all access/release remains serialized. */
class LiveEvent {
    struct State;
    std::unique_ptr<State> state_; ///< Whole owned arena plus immutable generation lease.
    friend class LiveSession;
    friend class detail::TapeEventBridge;
    /** Adopt a fully admitted event.
     * @param state Complete owned event.
     */
    explicit LiveEvent(std::unique_ptr<State> state) noexcept;

    public:
    /** Transfer ownership; existing views now follow the destination owner.
     * @param other Source event, made empty.
     */
    LiveEvent(LiveEvent &&other) noexcept;
    /** Release old storage and transfer ownership.
     * @param other Source event, made empty.
     * @return This event.
     */
    LiveEvent &operator=(LiveEvent &&other) noexcept;
    /** Release requested backing and all borrowed views. */
    ~LiveEvent();
    /** Inspect the admitted control/data kind.
     * @return Event kind; moved-from events report EMPTY_HEARTBEAT.
     */
    [[nodiscard]] LiveEventKind kind() const noexcept;
    /** Inspect the original envelope message identity, independently of emitted records.
     * @return Exact type, including 10/15/20 for empty data batches; absent only for an empty envelope.
     */
    [[nodiscard]] std::optional<std::int32_t> messageType() const noexcept;
    /** Borrow only whole-frame admitted application records.
     * @return Immutable views until event destruction/replacement, empty for controls.
     */
    [[nodiscard]] std::span<const LiveRecordView> records() const noexcept;
    /** Inspect selected heartbeat metadata.
     * @return Value only for explicit type0 events; empty envelope markers have no heartbeat.
     */
    [[nodiscard]] std::optional<LiveHeartbeat> heartbeat() const noexcept;
    /** Retain the immutable effective peer generation used by this event.
     * @return Independent owning snapshot handle.
     */
    [[nodiscard]] LiveSnapshot snapshot() const noexcept;
    /** Inspect this event's complete requested State and arena backing in constant work.
     * Shared snapshot/epoch reservations remain separately bounded by the epoch ledger.
     * @return Event-specific requested bytes, including spare arena capacity; zero after move.
     */
    [[nodiscard]] std::size_t residentBytes() const noexcept;
    /** Inspect a conservative byte-work allowance before releasing this event.
     * Includes the current whole epoch reservation, shared snapshot/configuration backing and
     * possibly unrelated owners. Sample immediately before release in the serialized, nonreentrant
     * ownership domain; do not use this value as event-specific staging or physical-memory usage.
     * This does not bound native allocator latency or measure CPU instructions.
     * @return Current epoch requested bytes covering potential shared-owner destruction; zero after move.
     */
    [[nodiscard]] std::size_t releaseBytes() const noexcept;
};

/** Move-only complete output and local partial-write cursor; consumption is never peer acknowledgement. */
class LivePendingOutput {
    struct State;
    std::unique_ptr<State> state_; ///< Counted whole byte backing and serialized epoch lease.
    friend class LiveSession;
    friend struct detail::LiveAuthorization;
    /** Adopt one fully composed output.
     * @param state Counted immutable envelope and initial zero cursor.
     */
    explicit LivePendingOutput(std::unique_ptr<State> state) noexcept;

    public:
    /** Transfer bytes and local cursor.
     * @param other Source output, made empty.
     */
    LivePendingOutput(LivePendingOutput &&other) noexcept;
    /** Release old output and transfer bytes/cursor.
     * @param other Source output, made empty.
     * @return This output.
     */
    LivePendingOutput &operator=(LivePendingOutput &&other) noexcept;
    /** Release whole backing, including consumed bytes, and its epoch credits. */
    ~LivePendingOutput();
    /** Borrow the original immutable whole envelope.
     * @return Bytes valid until destruction/replacement; moving transfers their lifetime.
     */
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept;
    /** Inspect exact ADD/REMOVE wire occurrences in this immutable composed frame.
     * @return Zero for control/descriptions, otherwise actual composed subscription occurrences.
     */
    [[nodiscard]] std::uint64_t subscriptionRecords() const noexcept;
    /** Borrow the exact local unsent suffix.
     * @return Immutable suffix; consumed backing stays reserved until owner destruction.
     */
    [[nodiscard]] std::span<const std::byte> pendingBytes() const noexcept;
    /** Advance only the local partial-write cursor.
     * @param count Locally accepted suffix bytes, without peer acknowledgement.
     * @return Success or INVALID_CONSUMPTION with the cursor unchanged.
     */
    [[nodiscard]] std::expected<void, LiveError> consume(std::size_t count) noexcept;
};

/** Incremental live input state; empty feed never means EOF. */
enum class LiveStatus {
    NEED_INPUT, ///< No whole admitted envelope this call.
    EVENT,      ///< Exactly one whole admitted event.
    FAILED,     ///< First terminal input failure, with no failed-frame event.
    CLOSED      ///< Explicit clean input end.
};

/** At most one whole admitted immutable event and exact per-call input accounting. */
struct LiveFeedResult {
    std::size_t accepted = 0;                   ///< Input bytes copied this call; resubmit the unaccepted suffix.
    LiveStatus status = LiveStatus::NEED_INPUT; ///< State after this call.
    std::optional<LiveEvent> event;             ///< Present only for a whole successfully admitted frame.
};

/** Pure read-side bidirectional protocol driver; no sockets, clocks, callbacks or executor.
 * All related operations, owner access/copies/destruction and partial-output progress are serialized
 * and non-reentrant. A new instance is a fresh connection epoch; no reconnect/reset reuse is provided.
 */
class LiveSession {
    struct Impl;
    std::unique_ptr<Impl> impl_; ///< Owned configuration, framing, generation and accounting.
    friend struct detail::LiveAuthorization;
    /** Adopt fully prepared owned state without allocating.
     * @param impl Complete session implementation.
     */
    explicit LiveSession(std::unique_ptr<Impl> impl) noexcept;

    public:
    /** Inspect a detached serialized-owner cut without parsing or advancing the session.
     * @return Lifetime framing and committed DATA counters; zero for a moved-from session.
     */
    [[nodiscard]] LiveInputStatistics inputStatistics() const noexcept;
    /** Transfer this epoch without invalidating existing owning results.
     * @param other Source session, made unusable.
     */
    LiveSession(LiveSession &&other) noexcept;
    /** Release the old epoch and transfer another.
     * @param other Source session, made unusable.
     * @return This session.
     */
    LiveSession &operator=(LiveSession &&other) noexcept;
    /** Release session storage; independent results retain only their own epoch reservations. */
    ~LiveSession();
    /** Prepare all potentially failing allocations before consuming configuration ownership.
     * Requires a usable matching composer without outstanding outputs. Schema/configuration is
     * immutable for this epoch. Invalid/mismatched configuration or outstanding composer outputs
     * returns INVALID_CONFIGURATION. Failure consumes neither input; work is never refunded.
     * @param schema Validated immutable schema, transferred only on success.
     * @param composer Validated outgoing layouts/composer from the same provenance, transferred only on success.
     * @param limits Required finite backing, logical and explicit profile/security limits.
     * @param work Mutable caller-turn credits, including failures.
     * @return Fresh independent epoch or typed rejection with caller inputs unchanged.
     */
    [[nodiscard]] static std::expected<LiveSession, LiveError>
    create(core::LocalSchema &&schema, SubscriptionWriter &&composer, LiveLimits limits, LiveWork &work);
    /** Accept no more than one envelope and atomically admit its entire semantic result.
     * A failed data/description frame publishes no decoder prefix or candidate state. Previous
     * successful owners remain valid. Input must not alias any session-retained framing storage.
     * @param input Borrowed bytes, retained only by copying accepted input; empty is not EOF.
     * @param work Mutable caller-turn credits, replenished only at the caller's chosen next turn.
     * @return Exact accepted count and at most one immutable successful event.
     */
    [[nodiscard]] LiveFeedResult feed(std::span<const std::byte> input, LiveWork &work);
    /** End input explicitly; incomplete input is a terminal malformed failure.
     * @return CLOSED for a clean negotiated boundary, otherwise FAILED.
     */
    [[nodiscard]] LiveStatus finish() noexcept;
    /** Compose a bounded local descriptor delta; authorization must use authorize().
     * Only implemented message capabilities may be advertised. No local send/peer acknowledgement is implied.
     * @param descriptor Borrowed explicit properties and directional messages, unchanged during the call.
     * @param work Mutable caller-turn credits.
     * @return Whole immutable output or a nonterminal typed rejection.
     */
    [[nodiscard]] std::expected<LivePendingOutput, LiveError> composeProtocol(LiveProtocolView descriptor,
                                                                              LiveWork &work);
    /** Compose one selected heartbeat without reading a clock or scheduling a timer.
     * @param heartbeat Explicit raw field-presence/value selection.
     * @param work Mutable caller-turn credits.
     * @return Whole immutable type0 output or typed rejection.
     */
    [[nodiscard]] std::expected<LivePendingOutput, LiveError> composeHeartbeat(LiveHeartbeat heartbeat, LiveWork &work);
    /** Compose an authorization-only descriptor delta after explicit verified-TLS eligibility.
     * Token text is borrowed only during the call and retained only in successful pending bytes.
     * @param basicToken Complete nonempty Basic authorization text, including its scheme prefix.
     * @param work Mutable caller-turn credits.
     * @return Whole secret-bearing output or a static typed rejection; composition is not auth acknowledgement.
     */
    [[nodiscard]] std::expected<LivePendingOutput, LiveError> authorize(std::u16string_view basicToken, LiveWork &work);
    /** Compose exact outgoing requested descriptions using the existing composer contract.
     * @param senderIds Explicit outgoing IDs, borrowed only during the call.
     * @param work Mutable caller-turn credits.
     * @return Whole immutable type2 output or nonterminal typed rejection.
     */
    [[nodiscard]] std::expected<LivePendingOutput, LiveError> describe(std::span<const std::int32_t> senderIds,
                                                                       LiveWork &work);
    /** Compose one family only after peer receive-add readiness and selected authentication completion.
     * @param message One of the six exact supported subscription messages.
     * @param requests Exact symbols/provenance and optional raw HISTORY bounds, borrowed only for the call.
     * @param work Mutable caller-turn credits.
     * @return Whole immutable output, never a peer acknowledgement, or a nonterminal rejection.
     */
    [[nodiscard]] std::expected<LivePendingOutput, LiveError>
    subscribe(SubscriptionMessage message, std::span<const core::SubscriptionRequest> requests, LiveWork &work);
    /** Retain the current immutable effective peer generation.
     * @return Owning snapshot, empty for a moved-from session.
     */
    [[nodiscard]] LiveSnapshot peer() const noexcept;
    /** Inspect source-derived authentication state, without advancing timers or invoking callbacks.
     * @return Effective state; moved-from sessions report UNCHALLENGED.
     */
    [[nodiscard]] LiveAuthentication authentication() const noexcept;
    /** Inspect the intersection of local hs support and the peer's exact comma-token advertisement.
     * @return True only for the selected negotiated history-snapshot capability.
     */
    [[nodiscard]] bool historySnapshotEnabled() const noexcept;
    /** Inspect the first terminal input error.
     * @return Static/numeric evidence; moved-from sessions report INVALID_STATE.
     */
    [[nodiscard]] LiveDiagnostic diagnostic() const noexcept;
    /** Inspect all current requested-capacity reservations in this epoch.
     * Fixed configuration reservations persist with epoch leases, conservatively covering shared provenance.
     * @return Requested bytes, including old owners, never process RSS; zero when moved from.
     */
    [[nodiscard]] std::size_t residentBytes() const noexcept;
    /** Inspect the highest admitted simultaneous backing reservation.
     * @return Requested byte peak for this epoch, or zero when moved from.
     */
    [[nodiscard]] std::size_t peakBytes() const noexcept;
};

} // namespace org::ttldtor::qdcxx::protocol
