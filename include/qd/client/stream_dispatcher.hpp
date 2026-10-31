// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/collectors/stream.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

namespace org::ttldtor::qdcxx::client {
namespace detail {
struct Port;
struct Completion;
struct Batch;
struct Budget;
enum class CommandKind;
} // namespace detail

/** Local dispatcher/port failures; no peer acknowledgement or socket outcome is implied. */
enum class ClientError {
    INVALID_STATE,         ///< Moved-from owner/handle or unavailable operation.
    INVALID_CONFIGURATION, ///< Invalid capacities, missing callback or arithmetic overflow at creation.
    REENTRANT,             ///< Owner operation attempted recursively from an executing owner action/callback.
    INVALID_EVENT,         ///< Moved-from event or data family other than admitted STREAM_DATA.
    INVALID_CONTEXT,       ///< Admitted local record belongs to another immutable schema context.
    INVALID_EPOCH,         ///< Event belongs to a different connection epoch; no implicit reset occurs.
    BUSY,                  ///< The sole accepted event still needs owner staging/release.
    CLOSING,               ///< Ordinary event/command admission has closed.
    COMMAND_LIMIT,         ///< No ordinary copied-command slot is available.
    RESULT_LIMIT,          ///< All finite completion slots remain pending or retained by callers.
    RESOURCE_LIMIT,        ///< Client/collector/batch capacity exhausted; data overflow is terminal.
    WORK_LIMIT,            ///< Caller turn cannot admit the next bounded owner/submit operation.
    ALLOCATION_FAILURE,    ///< Factory or detached-batch allocation failed before publication.
    IDENTITY_EXHAUSTED,    ///< Ordinary command identity cannot advance without reuse.
    CALLBACK_FAILURE,      ///< Consumer threw; no callback side effect is retried.
    COLLECTOR_FAILURE,     ///< Existing collector returned a non-work/non-resource input failure.
    ABORTED                ///< Explicit cancellation/gap category; never a primary non-cancellation failure.
};

/** Reserved control request; abort may escalate an earlier graceful request. */
enum class StopMode {
    GRACEFUL, ///< Drain the defined still-eligible dispatcher-admitted prefix after the owner cutoff.
    ABORT     ///< Discard pending local data; may escalate graceful stop without ordinary queue admission.
};
/** Dispatcher-local lifecycle, independent of external session/socket progress. */
enum class DispatchState {
    OPEN,    ///< Local event/ordinary command admission is available; no peer readiness is implied.
    CLOSING, ///< Owner has applied stop/failure; callbacks or bounded cleanup may still need progress.
    TERMINAL ///< Owned callback/command/staging domain is quiescent; external driver state is separate.
};
/** Local command completion; acceptance and wire emission are separate barriers. */
enum class CommandStatus {
    APPLIED,          ///< Existing desired-state mutation committed, including an explicit no-op.
    REJECTED,         ///< Existing desired-state error preserved the previous map/revision.
    CANCELLED_CLOSING ///< Accepted command was not applied before the owner stop cutoff.
};
/** Truthful local terminal outcome; external transport completion is not certified. */
enum class DispatchOutcome {
    DRAINED, ///< Defined still-eligible local prefix completed, without certifying the external feed lifetime.
    ABORTED, ///< Explicit abort/last-handle retirement occurred without an earlier owner failure.
    FAILED   ///< First non-cancellation owner failure remains visible after cleanup.
};
/** At most one bounded owner action performed by progress(). */
enum class ProgressKind {
    IDLE,     ///< No local bounded action is immediately required.
    STAGED,   ///< Accepted whole data event entered the real collector.
    RELEASED, ///< Control/zero-selection event or retired source prefix was released without callback.
    COMMAND,  ///< One accepted ordinary command applied, rejected or cancelled.
    CALLBACK, ///< One complete immutable batch invocation returned or threw.
    CLOSING,  ///< Stop/failure barrier or bounded abort discard advanced.
    TERMINAL  ///< Local owner quiescence was published, or had already been reached.
};

/** Explicit fixed-port and detached-batch limits; zero never means unlimited. */
struct ClientLimits {
    std::size_t commandSlots;       ///< Copied ordinary commands, including an owner-executing slot.
    std::size_t resultSlots;        ///< Independently retained completion allocations reserved at creation.
    std::size_t commandEntries;     ///< Maximum borrowed entries per add/remove/replace command.
    std::size_t commandSymbolUnits; ///< UTF-16 backing units reserved in EACH command slot.
    std::size_t batchRecords;       ///< Positive maximum records in one immutable callback batch.
    std::size_t batchFields;        ///< Aggregate typed and legacy scalar slots permitted in one batch.
    std::size_t batchSymbolUnits;   ///< Aggregate copied symbol UTF-16 units per batch.
    std::size_t batchFieldUnits;    ///< Aggregate copied nullable field-text units per batch.
    std::size_t batchByteUnits;     ///< Aggregate copied nullable opaque field bytes per batch.
    std::size_t batchArenaBytes;    ///< Complete fixed arena requested for each detached batch candidate.
    std::size_t batchOwners;        ///< Maximum simultaneous distinct candidate/executing/caller-retained batches.
    std::size_t batchResidentBytes; ///< Complete Batch object plus arena reservations for all live batch owners.
    std::size_t residentBytes;      ///< Fixed client reservation plus all live batch reservations; excludes existing
                                    ///< desired/collector/session scopes.
};

/** Producer-local finite admission credits, never shared implicitly across threads. */
struct SubmitWork {
    std::size_t operations; ///< Remaining command submission attempts.
    std::size_t visits;     ///< Remaining input, slot search, initialization and disposal visits.
    std::size_t textUnits;  ///< Remaining exact examined/copied UTF-16 units.
};

/** Owner-local turn credits, reused across progress calls without refunds. */
struct DispatchWork {
    std::size_t operations;         ///< Remaining public progress attempts.
    std::size_t commands;           ///< Remaining ordinary application/cancellation actions.
    std::size_t callbacks;          ///< Remaining consumer invocations, including throwing invocations.
    std::size_t records;            ///< Remaining context, offer, copy and disposal metadata visits.
    std::size_t fields;             ///< Remaining typed/legacy field visits.
    std::size_t textUnits;          ///< Remaining examined/copied symbol and field UTF-16 units.
    std::size_t bytes;              ///< Remaining fixed initialization, arena copy and conservative release byte work.
    core::SubscriptionWork desired; ///< Existing atomic desired-state mutation credits.
    collectors::StreamWork collector; ///< Existing collector staging/offer/ack/release credits.
};

/** Immutable numeric result published once for an accepted command. */
struct CommandResult {
    std::uint64_t id = 0; ///< Non-reused identity in this port, never a wire sequence.
    CommandStatus status = CommandStatus::CANCELLED_CLOSING; ///< Applied/rejected/closing cancellation.
    std::uint64_t revision = 0;                   ///< Desired revision at application/rejection/cancellation.
    bool changed = false;                         ///< Effective desired mutation only, never composition or delivery.
    std::optional<core::SubscriptionError> error; ///< Existing rejection category, absent otherwise.
};

/** Thread-safe completion lease retaining no endpoint, callable, event or operation. */
class CommandTicket {
    detail::Completion *state_ = nullptr;
    friend class ClientHandle;
    /** Adopt one already retained fixed completion slot.
     * @param state Independently referenced slot.
     */
    explicit CommandTicket(detail::Completion *state) noexcept;

    public:
    /** Construct an empty ticket. */
    CommandTicket() noexcept = default;
    /** Retain an independent thread-safe result lease without allocation.
     * @param other Existing ticket whose object lifetime is externally protected during copying.
     */
    CommandTicket(const CommandTicket &other) noexcept;
    /** Transfer an existing result lease.
     * @param other Source ticket, made empty.
     */
    CommandTicket(CommandTicket &&other) noexcept;
    /** Retain another result, releasing the previous pure result on this thread.
     * @param other Existing source ticket.
     * @return This ticket.
     */
    CommandTicket &operator=(const CommandTicket &other) noexcept;
    /** Transfer another result lease.
     * @param other Source ticket, made empty unless self-assigned.
     * @return This ticket.
     */
    CommandTicket &operator=(CommandTicket &&other) noexcept;
    /** Release only detached completion/budget metadata; permitted on any thread. */
    ~CommandTicket();
    /** Query completion without waiting or owner progress.
     * @return Immutable result after application/rejection/cancellation, otherwise empty.
     */
    [[nodiscard]] std::optional<CommandResult> result() const noexcept;
    /** Inspect the accepted command identity without implying completion.
     * @return Nonzero identity, or zero for an empty ticket.
     */
    [[nodiscard]] std::uint64_t id() const noexcept;
};

/** Detached opaque run/epoch identity; no protocol intrusive lease can escape through it. */
class ClientIdentity {
    detail::Budget *budget_ = nullptr;
    bool epoch_ = false;
    friend class OwnedBatch;
    /** Retain detached identity/accounting metadata.
     * @param budget Fixed thread-safe metadata without callable/operation ownership.
     * @param epoch Select epoch identity, distinct from the registration/run identity.
     */
    ClientIdentity(detail::Budget *budget, bool epoch) noexcept;

    public:
    /** Construct an invalid token. */
    ClientIdentity() noexcept = default;
    /** Retain this detached identity on any thread.
     * @param other Source token.
     */
    ClientIdentity(const ClientIdentity &other) noexcept;
    /** Transfer this detached identity.
     * @param other Source token, made empty.
     */
    ClientIdentity(ClientIdentity &&other) noexcept;
    /** Retain another detached identity.
     * @param other Source token.
     * @return This token.
     */
    ClientIdentity &operator=(const ClientIdentity &other) noexcept;
    /** Transfer another detached identity.
     * @param other Source token, made empty unless self-assigned.
     * @return This token.
     */
    ClientIdentity &operator=(ClientIdentity &&other) noexcept;
    /** Release fixed detached metadata only; permitted on any thread. */
    ~ClientIdentity();
    /** Compare opaque domain and scope; invalid identities never match.
     * @param other Token to compare.
     * @return True only for the same valid run or epoch identity, never between scopes.
     */
    [[nodiscard]] bool matches(const ClientIdentity &other) const noexcept;
};

/** Immutable event-independent record; every span is owned by its detached batch. */
struct BatchRecord {
    std::int32_t localId;                    ///< Exact local immutable record identity.
    std::int32_t senderId;                   ///< Original peer sender ID, never membership identity.
    core::RecordContext context;             ///< Owning immutable core provenance, safe to copy independently.
    core::SubscriptionMembership membership; ///< Owning admission activation, without desired-entry storage.
    std::uint64_t revision;                  ///< Desired revision captured by collector admission.
    std::uint64_t bindingGeneration;      ///< Original peer generation, scoped by the batch's detached epoch identity.
    std::u16string_view symbol;           ///< Exact owned UTF-16 units, including non-null empty text.
    std::int32_t cipher;                  ///< Original raw symbol metadata.
    std::int32_t eventFlags;              ///< Original admitted flags, without business reinterpretation.
    std::span<const std::int32_t> values; ///< Exact optional legacy compact-only fields.
    std::span<const protocol::LiveFieldValueView>
        typedValues;                                   ///< Exact copied generic fields; views borrow batch arena only.
    std::optional<std::int64_t> removalTime;           ///< Exact optional raw removal time.
    std::optional<protocol::FieldEventTime> fieldTime; ///< Exact optional virtual-prefix metadata.
};

/** Thread-safe immutable batch lease; no callable, port, event, snapshot or live operation is retained. */
class OwnedBatch {
    detail::Batch *state_ = nullptr;
    friend class BorrowedBatch;
    friend class StreamDispatcher;
    /** Adopt one already retained immutable batch.
     * @param state Fully constructed detached backing.
     */
    explicit OwnedBatch(detail::Batch *state) noexcept;

    public:
    /** Construct an empty batch. */
    OwnedBatch() noexcept = default;
    /** Retain immutable backing without allocation.
     * @param other Source batch whose object lifetime is protected during copying.
     */
    OwnedBatch(const OwnedBatch &other) noexcept;
    /** Transfer immutable backing.
     * @param other Source batch, made empty.
     */
    OwnedBatch(OwnedBatch &&other) noexcept;
    /** Retain another batch, releasing prior detached storage on this thread if last.
     * @param other Source batch.
     * @return This owner.
     */
    OwnedBatch &operator=(const OwnedBatch &other) noexcept;
    /** Transfer another batch.
     * @param other Source batch, made empty unless self-assigned.
     * @return This owner.
     */
    OwnedBatch &operator=(OwnedBatch &&other) noexcept;
    /** Release detached payload and its atomic reservation on any thread, never a protocol owner. */
    ~OwnedBatch();
    /** Borrow exact immutable records while this lease remains alive.
     * @return Batch records, or empty for an empty/moved-from lease.
     */
    [[nodiscard]] std::span<const BatchRecord> records() const noexcept;
    /** Retain the detached registration/run identity.
     * @return Owning token, invalid for an empty batch.
     */
    [[nodiscard]] ClientIdentity runIdentity() const noexcept;
    /** Retain the detached single-epoch identity, distinct from run identity.
     * @return Owning token, invalid for an empty batch.
     */
    [[nodiscard]] ClientIdentity epochIdentity() const noexcept;
};

/** Callback-only borrowed view; retaining this view itself does not extend its lifetime. */
class BorrowedBatch {
    detail::Batch *state_;
    friend class StreamDispatcher;
    /** Borrow fully prepared callback backing.
     * @param state Protected immutable storage, valid through callback return/exception.
     */
    explicit BorrowedBatch(detail::Batch *state) noexcept;

    public:
    /** Inspect exact records during the executing callback only.
     * @return Borrowed immutable views that expire on callback return.
     */
    [[nodiscard]] std::span<const BatchRecord> records() const noexcept;
    /** Retain independent immutable backing, extending its atomic quota reservation.
     * @return Thread-safe owner permitted to outlive callback, terminal and dispatcher destruction.
     */
    [[nodiscard]] OwnedBatch retain() const noexcept;
};

/** Caller-created callback object; opaque application allocations are outside client allocation accounting. */
class Consumer {
    public:
    /** Release caller-owned callback state; the dispatcher drops its reference only in the owner domain. */
    virtual ~Consumer() = default;
    /** Consume one immutable multi-record batch outside every mutation/container lock.
     * Commands submitted here apply only after return. Exceptions retire this one consumer without retry.
     * @param batch Borrowed views valid only during this invocation; retain explicitly for longer ownership.
     */
    virtual void onBatch(const BorrowedBatch &batch) = 0;
};

/** Cumulative dispatcher-local diagnostic counts; lower collector acknowledgement is not callback completion. */
struct DispatchCounts {
    std::size_t acceptedFrames = 0;      ///< Whole events whose ownership entered the dispatcher.
    std::size_t acceptedRecords = 0;     ///< Records in accepted data events before collector filtering.
    std::size_t stagedRecords = 0;       ///< Eligible records successfully staged by the real collector.
    std::size_t acknowledgedRecords = 0; ///< Eligible collector records consumed into prepared detached batches.
    std::size_t startedBatches = 0;      ///< Callback invocations begun, including failures.
    std::size_t startedRecords = 0;      ///< Whole borrowed batches presented at callback entry.
    std::size_t completedBatches = 0;    ///< Callback invocations returning normally.
    std::size_t completedRecords = 0;    ///< Records in normally returned callbacks, not upstream acknowledgements.
    std::size_t failedBatches = 0;       ///< Callback invocations throwing an exception.
    std::size_t failedRecords = 0;       ///< Records in throwing batches; application-side partial effects are unknown.
    std::size_t withdrawnRecords = 0;    ///< Retired memberships filtered by collector acknowledgement.
    std::size_t discardedRecords = 0;    ///< Known queued eligible records discarded at local abort/failure.
    std::size_t discardedPendingRecords = 0; ///< Accepted pending records discarded before classification/staging.
    std::size_t ignoredRecords = 0;          ///< Unrequested records classified by the collector, never cached.
    std::size_t completedCommands = 0;       ///< Applied/rejected/cancelled accepted commands.
    bool saturated = false;                  ///< At least one diagnostic counter reached its representable maximum.
};

/** Reserved gap evidence, visible independently of ordinary queue capacity. */
struct DispatchGap {
    ClientError cause;          ///< Static typed cause, never callback/credential text.
    std::size_t queuedRecords;  ///< Known eligible queued records at gap observation.
    std::size_t pendingRecords; ///< Pending records not yet classified by the collector.
    bool unknownEligibility;    ///< Pending records may include eligible records; no fabricated zero-loss claim.
};

/** Detached local completion after callbacks, accepted commands and owned event queues quiesce. */
struct DispatchTerminal {
    DispatchOutcome outcome;            ///< Drained local prefix, explicit abort or preserved failure.
    std::optional<ClientError> primary; ///< First observed non-cancellation owner failure.
    std::optional<collectors::StreamError> collectorError; ///< Nested collector category when relevant.
    DispatchCounts counts;   ///< Final immutable local counts; no socket/output/partial-input counts are invented.
    std::uint64_t revision;  ///< Last caller-applied desired revision, not internal cleanup or wire emission.
    bool unknownEligibility; ///< Accepted pending input was discarded before exact classification.
};

/** Finite owner progress result; no callback/operation runs in the background. */
struct DispatchProgress {
    ProgressKind kind;   ///< Bounded action completed this call.
    DispatchState state; ///< Local state after the action, distinct from transport state.
};

/** Per-scope current reservations, never a false transitive heap/RSS total. */
struct ClientResources {
    std::size_t fixedReservationBytes; ///< Conservative fixed client/port/result/identity reservation.
    std::size_t batchOwners;           ///< Distinct candidate/executing/caller-retained batch allocations.
    std::size_t batchBytes;            ///< Whole detached batch object/arena reservations.
    std::size_t queuedCommands;        ///< Accepted commands awaiting application/cancellation.
    std::size_t resultSlotsInUse;      ///< Pending or caller-retained immutable result slots.
    std::size_t queuedRecords;         ///< Collector records awaiting acknowledgement/discard.
    std::size_t pendingRecords;        ///< Whole accepted event records awaiting selection or release.
    std::size_t
        collectorFixedBytes; ///< Collector backing excluding event arenas already in external session accounting.
    std::size_t
        eventBytes; ///< Unique staged/pending event State+arena backing, also covered by the external session ledger.
    std::size_t desiredEntries; ///< Existing separately bounded desired-state entries.
};

/** Thread-safe nonblocking command/control port, retaining no dispatcher/callback/protocol owner. */
class ClientHandle {
    detail::Port *port_ = nullptr;
    friend class StreamDispatcher;
    /** Adopt a public port reference already linearized independently of owner progress ownership.
     * @param port Pure thread-safe port storage with one acquired public/port reference.
     */
    explicit ClientHandle(detail::Port *port) noexcept;
    /** Copy one ordinary mutation into fixed producer-safe slot storage.
     * @param kind Exact add/remove/replace/clear operation.
     * @param requests Borrowed complete inputs for add/replace, empty for other operations.
     * @param keys Borrowed exact keys for remove, empty for other operations.
     * @param work Producer-local preflight/copy credits, never refunded.
     * @return Accepted independent completion lease or immediate admission rejection.
     */
    [[nodiscard]] std::expected<CommandTicket, ClientError> submit(detail::CommandKind kind,
                                                                   std::span<const core::SubscriptionRequest> requests,
                                                                   std::span<const core::SubscriptionKey> keys,
                                                                   SubmitWork &work) const;

    public:
    /** Construct an empty handle. */
    ClientHandle() noexcept = default;
    /** Retain another public reference on any thread.
     * @param other Source handle whose object lifetime is protected during copying.
     */
    ClientHandle(const ClientHandle &other) noexcept;
    /** Transfer a public reference.
     * @param other Source handle, made empty.
     */
    ClientHandle(ClientHandle &&other) noexcept;
    /** Retain another public reference; releasing this last reference requests abort, never waits.
     * @param other Source handle.
     * @return This handle.
     */
    ClientHandle &operator=(const ClientHandle &other) noexcept;
    /** Transfer another public reference.
     * @param other Source handle, made empty unless self-assigned.
     * @return This handle.
     */
    ClientHandle &operator=(ClientHandle &&other) noexcept;
    /** Last public release requests allocation-free abort; only owner progress can complete it. */
    ~ClientHandle();
    /** Copy a bounded exact add command without invoking user code.
     * @param requests Borrowed entries preserved by caller through this call only.
     * @param work Producer-local finite credits.
     * @return Accepted completion ticket or immediate rejection with no command accepted.
     */
    [[nodiscard]] std::expected<CommandTicket, ClientError> add(std::span<const core::SubscriptionRequest> requests,
                                                                SubmitWork &work) const;
    /** Copy a bounded exact removal command.
     * @param keys Borrowed exact keys.
     * @param work Producer-local finite credits.
     * @return Accepted ticket or immediate admission rejection.
     */
    [[nodiscard]] std::expected<CommandTicket, ClientError> remove(std::span<const core::SubscriptionKey> keys,
                                                                   SubmitWork &work) const;
    /** Copy a complete replacement command for later atomic owner application.
     * @param requests Borrowed desired map.
     * @param work Producer-local finite credits.
     * @return Accepted ticket or immediate admission rejection.
     */
    [[nodiscard]] std::expected<CommandTicket, ClientError> replace(std::span<const core::SubscriptionRequest> requests,
                                                                    SubmitWork &work) const;
    /** Submit bounded clear through the ordinary command port.
     * @param work Producer-local finite credits.
     * @return Accepted ticket or immediate admission rejection; reserved stop is separate.
     */
    [[nodiscard]] std::expected<CommandTicket, ClientError> clear(SubmitWork &work) const;
    /** Request nonblocking graceful stop or escalating abort, even when ordinary slots are full.
     * @param mode Explicit local stop policy.
     * @return Success/request repetition or INVALID_STATE for an empty handle; terminal remains queryable.
     */
    [[nodiscard]] std::expected<void, ClientError> requestStop(StopMode mode) const noexcept;
    /** Query immutable completion without waiting or making queue space.
     * @return Local terminal only after actual owner cleanup/callback return, otherwise empty.
     */
    [[nodiscard]] std::optional<DispatchTerminal> terminal() const noexcept;
    /** Query the reserved first local gap observation.
     * @return Static typed/numeric evidence, or empty before a local gap.
     */
    [[nodiscard]] std::optional<DispatchGap> gap() const noexcept;
};

/** Event-driven single-consumer STREAM dispatcher, independent of socket/session ownership.
 * All owner calls/moves/destruction are externally serialized; recursive mutating owner calls return
 * REENTRANT. Keep this execution/progress owner alive through every callback; destroying it from its
 * own callback is unsupported. Public Handle release is safe there and requests abort without waiting.
 * Caller must hand a newly returned LiveSession/TcpRuntime event to admit BEFORE ordinary progress,
 * and handle rejected ownership before applying later commands. Accepted pending events stage/release
 * before commands/callbacks. Local terminal never certifies external runtime/socket/output completion.
 * Destruction/move-assignment outside an owner action performs bounded uncredited abort fallback.
 * Caller-created Consumer allocations and independent deep copies are outside client allocation scope.
 */
class StreamDispatcher {
    struct Impl;
    std::unique_ptr<Impl> impl_;
    /** Adopt fully allocated owner state without invoking callbacks.
     * @param impl Complete ready local owner.
     */
    explicit StreamDispatcher(std::unique_ptr<Impl> impl) noexcept;

    public:
    /** Transfer stable owner state within its serialized domain.
     * @param other Source dispatcher, made unusable.
     */
    StreamDispatcher(StreamDispatcher &&other) noexcept;
    /** Abort/release old state and transfer another outside every active owner call.
     * @param other Source dispatcher, made unusable unless self-assigned.
     * @return This dispatcher.
     */
    StreamDispatcher &operator=(StreamDispatcher &&other) noexcept;
    /** Bounded uncredited fallback cancels pending commands and releases protocol owners only here. */
    ~StreamDispatcher();
    /** Reserve independent desired/collector/port/batch/control scopes without retaining schema declarations.
     * @param schema Immutable source borrowed for creation; core contexts survive independently.
     * @param desiredLimits Existing exact desired-state capacities.
     * @param collectorLimits Existing whole-event collector capacities.
     * @param limits Required fixed-port and detached-batch capacities.
     * @param consumer Caller-created callback owner, preserved on failure and guarded through every invocation.
     * @param work Explicit initialization and existing desired/collector work credits.
     * @return Fresh local owner or typed rejection; no callbacks/network/ambient configuration occur.
     */
    [[nodiscard]] static std::expected<StreamDispatcher, ClientError>
    create(const core::LocalSchema &schema, core::SubscriptionLimits desiredLimits,
           collectors::StreamLimits collectorLimits, ClientLimits limits, const std::shared_ptr<Consumer> &consumer,
           DispatchWork &work);
    /** Acquire a public command/control reference without allocating.
     * @return Thread-safe handle, or rejection once last-handle retirement/closing has begun.
     */
    [[nodiscard]] std::expected<ClientHandle, ClientError> handle() const noexcept;
    /** Transfer exactly one whole admitted event into the sole owner-only pending position.
     * @param event Real immutable protocol owner, consumed only on acceptance; controls are release-only.
     * @return Ownership acceptance, not staging/delivery; BUSY/closing/invalid/reentrant rejection preserves input.
     * This constant bounded call allocates nothing and invokes no callback. Epoch is pinned even for no matches.
     */
    [[nodiscard]] std::expected<void, ClientError> admit(protocol::LiveEvent &&event) noexcept;
    /** Perform at most one bounded staging/release/command/callback/close unit.
     * @param work Owner turn credits, never automatically reset or refunded.
     * @return Actual local progress, or nonterminal WORK_LIMIT/REENTRANT/INVALID_STATE. Data/callback failures
     * begin explicit local closing; keep progressing until terminal. No accepted pending event is lost on yield.
     */
    [[nodiscard]] std::expected<DispatchProgress, ClientError> progress(DispatchWork &work);
    /** Borrow current exact desired demand for the composing driver's explicit subscription output route.
     * @return Read-only state in the owner domain, or null after collector retirement/move.
     */
    [[nodiscard]] const core::SubscriptionState *desired() const noexcept;
    /** Inspect owner-local lifecycle without implying transport readiness.
     * @return Current local state; moved-from dispatchers report TERMINAL without fabricated completion.
     */
    [[nodiscard]] DispatchState state() const noexcept;
    /** Inspect immutable local completion, including retained detached batches/results.
     * @return Terminal after callback/command/event quiescence, otherwise empty.
     */
    [[nodiscard]] std::optional<DispatchTerminal> terminal() const noexcept;
    /** Inspect current owner/atomic reservations; externally retained batches remain charged.
     * @return Explicit separate scopes, not transitive allocator/process memory; zero after move.
     */
    [[nodiscard]] ClientResources resources() const noexcept;
    /** Inspect cumulative local admission/ack/callback/command/discard evidence.
     * @return Current owner-local counts; only terminal snapshots are producer-safe via Handle.
     */
    [[nodiscard]] DispatchCounts counts() const noexcept;
};

} // namespace org::ttldtor::qdcxx::client
