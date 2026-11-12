// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/statistics.hpp"

#include "qd/collectors/history.hpp"
#include "qd/collectors/stream.hpp"
#include "qd/collectors/ticker.hpp"
#include "qd/mux/interest.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace org::ttldtor::qdcxx::mux {
namespace detail {
/** Detached implementation type; no execution-owner access is exposed. */
struct QueuePort;
/** Detached implementation type; no execution-owner access is exposed. */
struct QueueCompletion;
/** Detached implementation type; no execution-owner access is exposed. */
struct QueueBatch;
/** Detached implementation type; no execution-owner access is exposed. */
struct QueueOrigin;
} // namespace detail

/** Local delivery errors; no value implies socket, wire or peer completion. */
enum class QueueError {
    INVALID_STATE,         ///< Empty owner or incompatible local lifecycle.
    INVALID_CONFIGURATION, ///< Invalid capacities or unrepresentable requested backing.
    INVALID_CLIENT,        ///< Foreign, closed or reused registration.
    INVALID_EVENT,         ///< Not a complete admitted compatible source event.
    INVALID_CONTEXT,       ///< Independent local provenance or invalid exact key.
    INVALID_EPOCH,         ///< Foreign source epoch requires explicit reset before new admission.
    REENTRANT,             ///< Recursive owner mutation/progress from a callback.
    BUSY,                  ///< Pending event or mandatory owner phase prevents new source admission.
    CLOSING,               ///< Ordinary command/data admission has closed.
    COMMAND_LIMIT,         ///< No ordinary copied command slot.
    RESULT_LIMIT,          ///< All finite completion slots remain pending or caller-retained.
    RESOURCE_LIMIT,        ///< Client or aggregate requested reservation exceeded.
    WORK_LIMIT,            ///< Retry with a new explicit turn; spent work is not refunded.
    ALLOCATION_FAILURE,    ///< Allocation failed before the relevant transaction published.
    IDENTITY_EXHAUSTED,    ///< Non-reused scalar identity cannot advance.
    CALLBACK_FAILURE,      ///< Consumer threw; side effects are never retried.
    SOURCE_FAILURE         ///< Actual interest/collector component failed.
};
/** Exact nested pure-component cause. */
using QueueSourceError = std::variant<MuxError, core::SubscriptionError, collectors::StreamError,
                                      collectors::TickerError, collectors::HistoryError>;

/** Typed local failure, optionally preserving an actual component error. */
struct QueueFailure {
    QueueError error;                       ///< Local category.
    std::optional<QueueSourceError> source; ///< Actual nested error, never an invented exception message.
};
/** Result of a passive exact TICKER observation. */
enum class TickerLatestObservation {
    UNMAPPED, ///< The registration has no applied mapping for the key.
    ABSENT,   ///< The applied mapping has no current committed value.
    VISITED,  ///< The visitor received the current committed value.
};

/** Context-local lifecycle; external runtime completion is separate. */
enum class QueueState {
    OPEN,     ///< Ordinary admission remains open.
    CLOSING,  ///< Only accepted local cleanup/drain can progress.
    TERMINAL, ///< All local callback and command operations are quiescent.
};
/** Registration phase, with pause intent retained across synchronization/reset. */
enum class QueueClientState {
    SYNC_PENDING,  ///< Applied authoritative demand awaits actual source synchronization.
    RESET_PENDING, ///< Global reset or exact client reset notice acknowledgment is pending.
    ACTIVE,        ///< The registration can receive a future callback.
    PAUSED,        ///< Future callbacks are gated; bounded source intake may continue.
    CLOSING,       ///< The finite cutoff is draining or aborting.
    TERMINAL,      ///< No future callback can start for this registration.
};
/** Reserved stop request; abort can escalate graceful. */
enum class StopMode {
    GRACEFUL, ///< Drain the finite accepted cutoff or report an explicit non-drained outcome.
    ABORT,    ///< Exclude pending delivery without waiting for consumer space.
};
/** Callback return acknowledges its whole batch; PAUSE gates only later callbacks. */
enum class BatchAction {
    CONTINUE, ///< Acknowledge this whole batch and permit a later callback.
    PAUSE,    ///< Acknowledge this whole batch and gate later callbacks until resume.
};
/** Reserved control acceptance, not owner application/completion. */
enum class ControlAdmission {
    RECORDED,  ///< The reserved control changed.
    COALESCED, ///< The same or stronger reserved control is already recorded.
    STALE,     ///< The exact notice sequence is no longer current.
    CLOSED,    ///< The port cannot accept this control.
};
/** Immutable ordinary desired-command outcome. */
enum class CommandStatus {
    APPLIED,           ///< Authoritative desired mutation completed; source synchronization is separate.
    REJECTED,          ///< The authoritative component rejected the entire command.
    CANCELLED_CLOSING, ///< Accepted command was cancelled by the finite closing barrier.
};
/** Truthful local termination, independent of transport. */
enum class TerminalOutcome {
    DRAINED, ///< The local finite cutoff completed without forced loss.
    ABORTED, ///< The cutoff excluded delivery or could not truthfully drain.
    FAILED,  ///< A preserved non-cancellation failure ended this registration/context.
};
/** Accepted event phase; controls are never consumer records. */
enum class AdmissionKind {
    DATA,         ///< A whole matching-family event owner was accepted for later staging.
    RELEASE_ONLY, ///< A source control owner was accepted only for serialized disposal.
};
/** One finite owner action. */
enum class QueueProgressKind {
    IDLE,         ///< One debited fair phase/slot made no externally visible action.
    STAGED,       ///< The actual collector admitted the whole accepted source owner.
    RELEASED,     ///< A pending owner was released in the serialized domain.
    ROUTED,       ///< One family-specific atomic routing unit committed.
    COMMAND,      ///< One ordinary command result was published.
    SYNCHRONIZED, ///< A derived desired/view mapping synchronized.
    CALLBACK,     ///< One callback invocation returned or failed.
    CONTROL,      ///< One bounded reserved lifecycle/reset phase advanced.
    CLEANUP,      ///< One finite retirement or discard unit advanced.
    TERMINAL,     ///< The context reached local quiescence.
};
/** Reset is a continuing owner operation, not a synchronous rollback transaction. */
enum class ResetPhase {
    NONE,                       ///< No source reset owner operation is pending.
    FREEZE_OLD,                 ///< Freeze old callbacks and source intake.
    DISCARD_OLD_PENDING,        ///< Exclude the old accepted whole source owner before reset.
    DISCARD_OLD_QUEUES,         ///< Discard old detached delivery in finite quanta.
    SOURCE_RESET_PENDING,       ///< Retry the actual source reset with caller credits.
    NOTICE_PUBLICATION_PENDING, ///< Publish every reserved client notice after actual source commit.
    GLOBAL_ACK_PENDING,         ///< Retry actual collector-global acknowledgment without repeating reset.
    FAILED,                     ///< Preserve phased failure evidence while closing.
};
/** A reset request may coalesce while its source interval remains frozen. */
enum class ResetAdmissionKind {
    ACCEPTED,  ///< A new finite reset owner operation was accepted.
    COALESCED, ///< A request joined the already accepted reset interval.
};

/** Fixed opaque origin plus non-reused scalar epoch; contains no payload or callable. */
class QueueIdentity {
    std::shared_ptr<const detail::QueueOrigin> origin_; ///< Fixed detached opaque scope.
    std::uint64_t generation_ = 0;                      ///< Zero is invalid; a live epoch scalar is never reused.
    friend class MuxQueue;

    public:
    /** Construct an invalid identity without allocation. */
    QueueIdentity() noexcept = default;
    /** Compare complete valid scope.
     * @param other Detached token.
     * @return Same origin and generation; invalid tokens never match.
     */
    [[nodiscard]] bool matches(const QueueIdentity &other) const noexcept;
};

/** Explicit aggregate reservations; zero never means unlimited. */
struct QueueLimits {
    std::size_t clients;        ///< Active plus retained retired registration generations.
    std::size_t sourceRecords;  ///< Maximum records in one accepted whole event.
    std::size_t routeRecords;   ///< Positive maximum STREAM source chunk.
    std::size_t batchRecords;   ///< Positive maximum callback batch.
    std::size_t cleanupRecords; ///< Positive unlink/ref-release quantum.
    std::size_t dataItems;      ///< Aggregate reserved distinct detached-record slots.
    std::size_t recordBytes;    ///< Aggregate reserved record control/object requests plus full arena capacities.
    std::size_t batchOwners;    ///< Aggregate reserved composite allocations.
    std::size_t batchBytes;     ///< Aggregate reserved composite control/object and roots/view array requests.
    std::size_t commandSlots;   ///< Aggregate reserved ordinary command slots.
    std::size_t resultSlots;    ///< Aggregate reserved pending/retained completion slots.
    std::size_t controlSlots;   ///< Two separate slots per activated generation.
    std::size_t residentBytes;  ///< All queue-owned requested blocks including STL controls; component scopes separate.
};

/** One registration's immutable maxima, reserved before activation. */
struct ClientLimits {
    std::size_t dataItems;         ///< Distinct queued/executing/caller-retained records.
    std::size_t recordBytes;       ///< Actual record control/object allocation requests plus complete arena capacities.
    std::size_t batchOwners;       ///< Candidate/executing/caller-retained composites.
    std::size_t batchBytes;        ///< Actual composite control/object and roots/view array allocation requests.
    std::size_t batchRecords;      ///< Positive per-callback record maximum.
    std::size_t recordFields;      ///< Legacy scalar plus generic tagged field slots per record.
    std::size_t recordSymbolUnits; ///< Exact symbol UTF16 units per record.
    std::size_t recordFieldUnits;  ///< Aggregate nullable field UTF16 units per record.
    std::size_t recordByteUnits;   ///< Aggregate nullable opaque bytes per record.
    std::size_t recordArenaBytes;  ///< Whole requested immutable arena capacity per record.
    std::size_t commandSlots;      ///< Ordinary command slots, including an executing command.
    std::size_t resultSlots;       ///< Independently retained completion slots.
    std::size_t commandEntries;    ///< Borrowed input entries including duplicates per submission.
    std::size_t commandSymbolUnits; ///< Fixed UTF16 capacity in each command slot.
};

/** Producer-local finite submit credits. */
struct QueueSubmitWork {
    std::size_t operations; ///< Submit attempts.
    std::size_t visits;     ///< Input, lookup, slot and metadata visits.
    std::size_t textUnits;  ///< Examined/copied exact UTF16.
    std::size_t bytes;      ///< Fixed metadata rewrite/copy/release allowance.
};

/** Owner-turn credits; actual component credits retain their own meanings. */
struct QueueWork {
    std::size_t operations; ///< Owner attempts.
    std::size_t visits; ///< Complete metadata-slot/fixed-object visits, including initialization and prepaid disposal.
    std::size_t fields; ///< Legacy/generic field visits.
    std::size_t textUnits; ///< Examined/copied UTF16 units.
    std::size_t bytes;     ///< Actual fixed/payload request allowance and prepaid detached disposal; batch preparation
    ///< conservatively debits twice the client batch-byte ceiling before allocation.
    std::size_t commands;            ///< Ordinary command application/cancellation actions.
    std::size_t callbacks;           ///< Consumer invocations including throwing invocations.
    MuxWork interest;                ///< Actual authoritative interest work.
    core::SubscriptionWork desired;  ///< Actual derived desired-state work.
    collectors::StreamWork stream;   ///< Actual STREAM work, never silently increased.
    collectors::TickerWork ticker;   ///< Actual TICKER work including global reset ACK.
    collectors::HistoryWork history; ///< Actual HISTORY work; its ACK has no work argument.
};

/** Independent desired result; APPLIED is distinct from derived synchronization and wire emission. */
struct MuxCommandResult {
    std::uint64_t id = 0;                                    ///< Non-reused port-local identity.
    CommandStatus status = CommandStatus::CANCELLED_CLOSING; ///< Applied, rejected or closing cancellation.
    std::uint64_t revision = 0;                              ///< Actual global desired revision.
    bool changed = false;                                    ///< Effective desired mutation only.
    std::optional<MuxError> error;                           ///< Exact authoritative rejection, if any.
};

/** Detached completion; no queue, callable, source event or operation is retained. */
class MuxCommandTicket {
    std::shared_ptr<detail::QueueCompletion> state_; ///< Detached finite immutable completion lease.
    friend class MuxHandle;

    public:
    /** Construct an empty completion lease. */
    MuxCommandTicket() noexcept = default;
    /** Read completion safely from any thread.
     * @return Immutable result once completed; empty while pending/invalid.
     */
    [[nodiscard]] std::optional<MuxCommandResult> result() const;
    /** Inspect accepted identity.
     * @return Nonzero identity or zero for empty.
     */
    [[nodiscard]] std::uint64_t id() const noexcept;
};

/** Exact copied generic fields; spans borrow only immutable detached record backing. */
using MuxFieldValueView = core::FieldValueView;

/** True source scalar provenance, absent for synthesized projections. */
struct RawSourceMetadata {
    std::int32_t senderId;                             ///< Actual directional source ID.
    std::int32_t cipher;                               ///< Actual source symbol encoding metadata.
    std::int32_t flags;                                ///< Original admitted flags, not produced projection flags.
    std::optional<std::int64_t> removalTime;           ///< Actual optional raw removal time.
    std::optional<protocol::FieldEventTime> fieldTime; ///< Actual optional virtual metadata.
    std::uint64_t bindingGeneration;                   ///< Original snapshot generation within source epoch.
    std::optional<std::int64_t> tapeMessageTimeMs{};   ///< Tape MESSAGE cursor time when the source supplied one.
};

/** Immutable detached projection; not an admitted protocol record. */
struct MuxRecordView {
    core::SubscriptionFamily family;                          ///< Immutable registration family.
    core::RecordContext context;                              ///< Owning core provenance only.
    std::u16string_view symbol;                               ///< Exact batch-owned non-null symbol.
    std::optional<std::int64_t> rawTime;                      ///< Full signed indexed identity only where supplied.
    std::int32_t eventFlags;                                  ///< Produced family delivery flags.
    std::span<const std::int32_t> values;                     ///< Actual legacy compact scalars, when supplied.
    std::span<const MuxFieldValueView> typedValues;           ///< Exact copied generic fields.
    MuxMembership membership;                                 ///< Authoritative admission activation.
    std::uint64_t revision;                                   ///< Authoritative desired revision at queue admission.
    std::uint64_t admissionRevision;                          ///< Actual collector admission revision.
    QueueIdentity epoch;                                      ///< Detached local source epoch.
    std::optional<RawSourceMetadata> source;                  ///< Actual source provenance, never fabricated.
    std::optional<collectors::HistorySourceMetadata> trigger; ///< Actual HISTORY trigger, when present.
    bool versionDefault =
        false; ///< Explicit TICKER zero/null cursor projection; source remains original raw provenance.
    core::CapturedDeliveryProjections<MuxMembership>
        projections{}; ///< Original business choices, detached from desired text.
};

/** Thread-safe immutable composite lease; final release reaches no protocol/callable owner. */
class OwnedMuxBatch {
    std::shared_ptr<const detail::QueueBatch> state_; ///< Pure immutable backing and atomic reservation ledger.
    friend class BorrowedMuxBatch;

    public:
    /** Construct an empty owning value. */
    OwnedMuxBatch() noexcept = default;
    /** Inspect immutable records.
     * @return Span until the last owning lease dies; empty for invalid.
     */
    [[nodiscard]] std::span<const MuxRecordView> records() const noexcept;
    /** Copy detached epoch.
     * @return Origin/epoch token or invalid for empty.
     */
    [[nodiscard]] QueueIdentity epoch() const noexcept;
    /** Read the facade command identity captured at successful mapping commit, when attached.
     * @return Applied logical revision, or nullopt for an unobserved lower-level registration.
     */
    [[nodiscard]] std::optional<std::uint64_t> appliedDesiredRevision() const noexcept;
};

/** Callback-only borrow; copying this view does not extend lifetime. */
class BorrowedMuxBatch {
    std::shared_ptr<const detail::QueueBatch> state_; ///< Pure immutable backing and atomic reservation ledger.
    friend class MuxQueue;

    public:
    /** Inspect records during this callback only.
     * @return Exact immutable span, expiring on return/exception.
     */
    [[nodiscard]] std::span<const MuxRecordView> records() const noexcept;
    /** @return Actual applied facade revision for this immutable selection, or nullopt without observation. */
    [[nodiscard]] std::optional<std::uint64_t> appliedDesiredRevision() const noexcept;
    /** Retain already constructed detached backing without allocation.
     * @return Independent cross-thread owning lease.
     */
    [[nodiscard]] OwnedMuxBatch retain() const noexcept;
};

/** Caller-created callback; arbitrary application capture memory is outside queue byte accounting. */
class MuxConsumer {
    friend class MuxQueue;
    std::shared_ptr<core::detail::DeliveryStatisticsState> statistics_; ///< Optional pure facade accounting owner.

    protected:
    /** Retain optional delivery accounting without retaining an operational owner.
     * @param statistics Internal shared accounting state; empty for ordinary lower-level consumers.
     */
    explicit MuxConsumer(std::shared_ptr<core::detail::DeliveryStatisticsState> statistics = {}) noexcept
        : statistics_(std::move(statistics)) {
    }

    /** Inspect the internal accounting attachment, without callbacks or allocation.
     * @return Pure detached accounting owner, or empty when not requested.
     */
    [[nodiscard]] const std::shared_ptr<core::detail::DeliveryStatisticsState> &deliveryStatistics() const noexcept {
        return statistics_;
    }

    public:
    /** Release application callback storage; library-held references retire in the owner domain. */
    virtual ~MuxConsumer() = default;
    /** Observe one whole batch outside every port/container lock.
     * @param batch Borrowed immutable data; explicitly retain for longer lifetime.
     * @return CONTINUE or pause future callbacks after acknowledging this batch.
     */
    virtual BatchAction onBatch(const BorrowedMuxBatch &batch) = 0;
};

/** Saturating diagnostic evidence; not an upstream exactly-once guarantee. */
struct QueueCounts {
    std::size_t acceptedFrames = 0;        ///< Whole actual event owners accepted by this context.
    std::size_t acceptedRecords = 0;       ///< Actual records in accepted whole events, before client projection.
    std::size_t collectorAcknowledged = 0; ///< Actual pure collector records/projections explicitly acknowledged.
    std::size_t queueAdmitted = 0;         ///< Per-registration detached records committed to a delivery FIFO.
    std::size_t withdrawn = 0;             ///< Known retired per-registration delivery projections.
    std::size_t discarded = 0; ///< Known materialized per-registration delivery projections explicitly discarded.
    std::size_t sourceRecordsDiscarded = 0; ///< Accepted UNSTAGED or unrouted STREAM source records; not client copies.
    std::size_t callbackStartedBatches = 0; ///< Callback invocations started, including throwing invocations.
    std::size_t callbackStartedRecords = 0; ///< Records irrevocably included in started borrowed batches.
    std::size_t callbackCompletedBatches = 0; ///< Callback invocations that returned, including PAUSE.
    std::size_t callbackCompletedRecords = 0; ///< Records acknowledged by normally returning callbacks.
    std::size_t callbackFailedBatches = 0;    ///< Throwing invocations, never retried.
    std::size_t callbackFailedRecords =
        0;                             ///< Records in throwing invocations; application side effects are not retried.
    std::size_t commandsApplied = 0;   ///< Ordinary authoritative application results.
    std::size_t commandsRejected = 0;  ///< Whole ordinary authoritative rejections.
    std::size_t commandsCancelled = 0; ///< Accepted ordinary commands cancelled by closing.
    bool saturated = false;            ///< Diagnostic accumulation overflowed.
};

/** Detached continuing reset snapshot, retaining no live operation. */
struct ResetStatus {
    ResetPhase phase = ResetPhase::NONE;               ///< Current owner phase.
    std::uint64_t requestSequence = 0;                 ///< Latest coalesced request identity.
    std::optional<std::uint64_t> actualSourceSequence; ///< Real collector notice sequence after commit.
    bool sourceCommitted = false;    ///< The actual source reset returned success; rollback is not implied.
    bool globalAcknowledged = false; ///< Source barrier completed by TICKER/HISTORY ACK or synchronous STREAM reset.
    std::size_t discardedPendingFrames = 0; ///< Whole old accepted owner frames discarded before source reset.
    std::size_t discardedPendingRecords =
        0; ///< Actual old accepted source records discarded, without per-client classification.
    std::size_t publishedClients = 0;  ///< Reserved notices published during this reset owner operation.
    std::size_t coalescedRequests = 0; ///< Additional requests joining this same owner operation.
    bool unknownEligibility = false;   ///< Discarded source records were not fully classified.
    std::size_t discardedSourceProjections =
        0; ///< Actual source notice interval materialized view projections, excluding cache points.
    bool sourceCountsSaturated = false;  ///< Actual source interval diagnostic saturated.
    bool unknownSourceLoss = true;       ///< Actual source interval cannot quantify upstream continuity.
    std::optional<QueueFailure> failure; ///< Preserved actual phase failure.
};

/** Reserved coalesced interval; available even with a full data queue. */
struct QueueResetNotice {
    std::uint64_t sequence = 0;                             ///< Exact latest client ACK identity.
    QueueIdentity fromEpoch;                                ///< Earliest affected epoch.
    std::optional<QueueIdentity> toEpoch;                   ///< New epoch only after real source commit.
    std::size_t knownDiscardedQueueRecords = 0;             ///< Known materialized queue discard.
    std::optional<std::size_t> knownDiscardedSourceRecords; ///< Absent for unknown per-client classification.
    std::size_t skippedResets = 0;                          ///< Coalesced reset requests.
    std::optional<QueueFailure> firstCause;                 ///< Bounded typed causes.
    std::optional<QueueFailure> secondaryCause;             ///< Bounded typed causes.
    bool unknownEligibility = false; ///< Lost source eligibility was not classified into exact client copies.
    bool unknownUpstreamLoss = true; ///< Upstream/application continuity is unknown; no lost count is invented.
    bool countsSaturated = false;    ///< Integrity flags.
    bool sourceCommitted = false;    ///< The actual source reset returned success; rollback is not implied.
    bool globalAckPending = false;   ///< Actual collector-global ACK remains outstanding.
};

/** Local terminal after callback/command/source-view cleanup quiescence. */
struct QueueTerminal {
    TerminalOutcome outcome = TerminalOutcome::ABORTED; ///< Never clean drained after forced loss.
    std::optional<QueueFailure> primary;     ///< First non-cancellation failure, preserved through stop and retirement.
    std::optional<QueueFailure> secondary;   ///< Later bounded failure; never replaces the primary cause.
    QueueCounts counts;                      ///< Immutable local observations.
    std::uint64_t authoritativeRevision = 0; ///< Final application revision, not wire state.
    QueueIdentity epoch;                     ///< Final local source scope.
    bool hadGap = false;              ///< Continuity is incomplete because of known or unclassified excluded delivery.
    bool unknownEligibility = false;  ///< Lost source eligibility was not classified into exact client copies.
    bool unknownUpstreamLoss = false; ///< Upstream/application continuity is unknown; no lost count is invented.
    std::size_t failedRegistrationCount = 0; ///< Context aggregate; zero for an individual result.
    ResetStatus reset;                       ///< Preserved phased reset evidence.
};

/** Per-client actual and reserved requested backing; not transitive application heap. */
struct ClientResources {
    QueueClientState state = QueueClientState::TERMINAL; ///< Invalid handles have no terminal result.
    bool paused = false;                                 ///< A reserved pause gates later callbacks.
    bool resetPending = false;                           ///< A source reset or exact client notice ACK remains pending.
    std::size_t fixedReservationBytes = 0; ///< Complete fixed logical backing reserved for this generation.
    std::size_t queuedRecords = 0;         ///< Current detached delivery FIFO length.
    std::size_t recordOwners = 0;          ///< Distinct charged immutable record allocations.
    std::size_t recordBytes = 0;           ///< Complete charged immutable record object/arena bytes.
    std::size_t batchOwners = 0;           ///< Distinct charged immutable composite allocations.
    std::size_t batchBytes = 0;            ///< Complete charged composite object/reference/view capacity bytes.
    std::size_t queuedCommands = 0;        ///< Ordinary accepted commands still awaiting application/cancellation.
    std::size_t resultSlotsInUse = 0;      ///< Pending or externally retained finite completion slots.
    std::size_t controlSlots = 0;          ///< Reserved reset and terminal control slots.
};

/** Immutable independently enforced capacities; byte subtotals exclude the separate desired-count scopes.
 * Event/session storage may overlap component scopes. These are finite logical ceilings, not process RSS.
 */
struct QueueCapacity {
    std::size_t queueBytes = 0;     ///< Queue ledger ceiling, including future caller-retained generations.
    std::size_t muxBytes = 0;       ///< Independent interest-state byte ceiling.
    std::size_t collectorBytes = 0; ///< Collector byte ceiling, excluding its separate desired/session scopes.
    std::size_t desiredOwners = 0;  ///< Maximum simultaneous desired owners: one STREAM owner or source view slots.
    core::SubscriptionLimits desiredPerOwner{}; ///< Immutable count ceilings for each source desired owner.
};

/** Context known scopes; source events overlap the separately enforced session ledger. */
struct QueueResources {
    std::size_t fixedBytes = 0;    ///< Known fixed queue logical backing.
    std::size_t residentBytes = 0; ///< Known charged current queue backing, excluding separately reported components.
    std::size_t reservedBytes = 0; ///< Fixed queue backing plus active/retired generation maxima.
    std::size_t generations = 0;   ///< Active or caller-retained retired generation reservations.
    std::size_t dataItems = 0;     ///< Distinct candidate/queued/executing/caller-held record allocations.
    std::size_t recordBytes = 0;   ///< Complete charged immutable record object/arena bytes.
    std::size_t batchOwners = 0;   ///< Distinct charged immutable composite allocations.
    std::size_t batchBytes = 0;    ///< Complete charged composite object/reference/view capacity bytes.
    std::size_t reservedItems = 0; ///< Sum of immutable per-generation record slot maxima.
    std::size_t reservedRecordBytes = 0;  ///< Sum of immutable per-generation record byte maxima.
    std::size_t reservedBatchOwners = 0;  ///< Sum of immutable per-generation composite owner maxima.
    std::size_t reservedBatchBytes = 0;   ///< Sum of immutable per-generation composite byte maxima.
    std::size_t reservedCommandSlots = 0; ///< Sum of finite generation command slot maxima.
    std::size_t reservedResultSlots = 0;  ///< Sum of finite generation result slot maxima.
    std::size_t reservedControlSlots = 0; ///< Two reserved control slots per charged generation.
    std::size_t muxBytes = 0;             ///< Actual independent MUX interest logical backing.
    std::size_t collectorBytes = 0;       ///< Actual independent collector logical backing.
    std::size_t eventBytes = 0;           ///< Source event backing, overlapping separately enforced session storage.
    std::size_t pendingRecords = 0; ///< Actual accepted source owner/prefix records still awaiting release/routing.
    ResetPhase resetPhase = ResetPhase::NONE; ///< Actual continuing reset.
};

/** Detached scoped HISTORY evidence; old source facts are not current-request completeness. */
struct QueueRangeStatus {
    std::optional<collectors::HistoryRangeStatus> source; ///< Actual optional raw AS-OF source diagnostics.
    QueueIdentity epoch;                                  ///< Current local scope.
    std::uint64_t authoritativeRevision =
        0; ///< Actual global authoritative revision, independent of wire/delivery barriers.
    std::uint64_t clientDesiredRevision = 0; ///< Latest applied desired revision of this registration.
    std::uint64_t mappingDesiredRevision =
        0;                             ///< Desired revision represented by the synchronized actual source mapping.
    MuxMembership membership;          ///< Current exact authoritative activation.
    std::int64_t requestedFrom = 0;    ///< Current authoritative signed64 bound.
    bool mappingCurrent = false;       ///< Exact activation, revision, epoch and bound mapping is current.
    bool acceptedUnstagedData = false; ///< A whole accepted data owner has not yet staged.
    bool paused = false;               ///< A reserved pause gates later callbacks.
    bool resetPending = false;         ///< A source reset or exact client notice ACK remains pending.
    bool hadGap = false;               ///< Continuity is incomplete because of known or unclassified excluded delivery.
    std::size_t queuedForKey = 0;      ///< Detached not-yet-observed delivery records for this exact key.
    std::size_t executingForKey =
        0; ///< Records executing in the non-reentrant owner; owner query rejects recursive callbacks.
    bool completeDeliveredRange = false; ///< Only scoped synchronized covered clean delivered state.
};

/** Owning HISTORY key and its exact owner-local observation; contains no data payload or callable. */
struct QueueRangeObservation {
    core::RecordContext context; ///< Immutable schema/record provenance, independent of a live queue.
    std::u16string symbol;       ///< Exact copied physical symbol; source-specific records remain distinct contexts.
    core::SubscriptionSelectorKind selector; ///< Actual authoritative selector domain.
    QueueRangeStatus status;                 ///< Actual source/mapping/delivery evidence for this key.
};

/** Accepted owner reset request; not source/reset/ACK completion. */
struct ResetAdmission {
    ResetAdmissionKind kind;       ///< New acceptance or joining an existing finite operation.
    std::uint64_t requestSequence; ///< Exact non-reused latest request identity.
};

/** Successful whole-event ownership handoff. */
struct QueueAdmission {
    AdmissionKind kind;  ///< Data staging or serialized control release.
    std::size_t records; ///< Actual complete source records accepted.
    QueueIdentity epoch; ///< Detached current local source scope.
};

/** Finite progress evidence, separate from transport completion. */
struct QueueProgress {
    QueueProgressKind kind; ///< Actual bounded owner action.
    QueueState state;       ///< Resulting local context lifecycle.
};

class MuxQueue;

/** Thread-safe finite command/control handle; no callable/protocol/owner is retained. */
class MuxHandle {
    std::shared_ptr<detail::QueuePort> port_; ///< Finite producer domain with no callable/protocol owner.
    friend class MuxQueue;
    /** Adopt the first public reference.
     * @param port Preallocated immutable registration port.
     */
    explicit MuxHandle(std::shared_ptr<detail::QueuePort> port) noexcept;

    public:
    /** Construct an invalid inert handle. */
    MuxHandle() noexcept = default;
    /** Retain an existing public handle, including while closing.
     * @param other Source object, externally protected while copying.
     */
    MuxHandle(const MuxHandle &other) noexcept;
    /** Transfer public ownership.
     * @param other Source made empty.
     */
    MuxHandle(MuxHandle &&other) noexcept;
    /** Replace public ownership.
     * @param other Existing source.
     * @return This handle.
     */
    MuxHandle &operator=(const MuxHandle &other) noexcept;
    /** Transfer public ownership.
     * @param other Source made empty unless self-assigned.
     * @return This handle.
     */
    MuxHandle &operator=(MuxHandle &&other) noexcept;
    /** Last public release records abort without waiting; finite detached port disposal was prepaid at open. */
    ~MuxHandle();
    /** Copy a bounded desired add.
     * @param inputs Borrowed requests, copied before return.
     * @param work Producer-local attempt/copy credits.
     * @return Accepted immutable ticket or immediate typed refusal.
     */
    [[nodiscard]] std::expected<MuxCommandTicket, QueueError> add(std::span<const core::SubscriptionRequest> inputs,
                                                                  QueueSubmitWork &work);
    /** Copy a bounded desired removal.
     * @param inputs Borrowed exact keys.
     * @param work Producer-local credits.
     * @return Accepted ticket or refusal, never partial acceptance.
     */
    [[nodiscard]] std::expected<MuxCommandTicket, QueueError> remove(std::span<const core::SubscriptionKey> inputs,
                                                                     QueueSubmitWork &work);
    /** Copy a whole desired replacement.
     * @param inputs Borrowed complete candidate map.
     * @param work Producer-local credits.
     * @return Accepted ticket or refusal.
     */
    [[nodiscard]] std::expected<MuxCommandTicket, QueueError> replace(std::span<const core::SubscriptionRequest> inputs,
                                                                      QueueSubmitWork &work);
    /** Queue desired clear.
     * @param work Producer-local credits.
     * @return Accepted ticket or refusal.
     */
    [[nodiscard]] std::expected<MuxCommandTicket, QueueError> clear(QueueSubmitWork &work);
    /** Record monotonic stop independently of data/ordinary capacity.
     * @param mode Graceful or escalating abort.
     * @return Request acceptance only.
     */
    ControlAdmission requestStop(StopMode mode) noexcept;
    /** Coalesce a resume; cannot undo stop/reset gates.
     * @return Recorded/coalesced/closed.
     */
    ControlAdmission requestResume() noexcept;
    /** Request exact advertised reset ACK; owner rechecks the sequence.
     * @param sequence Current client notice identity.
     * @return Recorded/coalesced/stale/closed, not application completion.
     */
    ControlAdmission acknowledgeReset(std::uint64_t sequence) noexcept;
    /** Copy reserved notice safely from any thread.
     * @return Pending interval or empty.
     */
    [[nodiscard]] std::optional<QueueResetNotice> resetNotice() const;
    /** Query terminal safely without pumping/waiting.
     * @return Result only after actual local quiescence; empty for invalid/pending.
     */
    [[nodiscard]] std::optional<QueueTerminal> terminal() const;
    /** Inspect local phase without implying completed terminal.
     * @return Inert TERMINAL sentinel for invalid handles, whose terminal result remains empty.
     */
    [[nodiscard]] QueueClientState state() const;
};

/** One serialized context/family with actual shared collectors and isolated detached queues.
 * Owner methods/moves/destruction are externally serialized, not factory-thread affine. No callbacks under
 * locks; execution owner must outlive its callback. Final callable retirement also rejects recursive owner mutation.
 * Only handles/tickets/immutable leases are producer-safe.
 * No background execution, decoder, I/O, clocks or transport completion. Destructor fallback is finite
 * without caller-work/latency guarantees. Opaque application Consumer captures are separately owned.
 * Byte scopes include queue-owned STL control/object/vector/arena requests, but exclude physical allocator
 * and C++ runtime exception-transport infrastructure. Measured request profiles qualify the supported STL shape.
 */
class MuxQueue {
    /** Serialized concrete composition, defined out of line. */
    struct State;

    /** Empty owner deleter preserves pure accounting through actual State deallocation. */
    struct StateDelete {
        /** Dispose serialized State and then its requested backing.
         * @param state Existing owner, or null.
         */
        void operator()(State *state) const noexcept;
    };

    using StateOwner = std::unique_ptr<State, StateDelete>; ///< Sole serialized owner, with accounted backing.
    StateOwner state_;                                      ///< Sole externally serialized execution state.
    /** Adopt fully prepared ownership.
     * @param state Reserved owner state.
     */
    explicit MuxQueue(StateOwner state) noexcept;

    /** Share one finite scheduler with explicit callback-origin eligibility.
     * @param work Actual caller credits, never replenished.
     * @param retainedOnly Admit only newly projected cached INITIAL records to callbacks.
     * @return Existing fair progress result or unchanged typed error.
     */
    std::expected<QueueProgress, QueueFailure> progressImpl(QueueWork &work, bool retainedOnly);

    public:
    /** Transfer serialized ownership.
     * @param other Source made empty.
     */
    MuxQueue(MuxQueue &&other) noexcept;
    /** Release old ownership and transfer another owner.
     * @param other Source made empty unless self-assigned.
     * @return This owner.
     */
    MuxQueue &operator=(MuxQueue &&other) noexcept;
    /** Finite library abort traversal; never starts a delivery callback. Final callable destruction
     * remains owner-serialized, outside locks and inside the reentrancy guard; its application work is excluded.
     */
    ~MuxQueue();
    /** Create one STREAM union collector and actual interest state.
     * @param schema Immutable provenance.
     * @param interests Actual interest capacities.
     * @param desired Actual union desired capacities.
     * @param source Actual STREAM capacities.
     * @param limits Explicit queue aggregate maxima.
     * @param work Owner initialization/cleanup credits.
     * @param policy Immutable controlled-peer selector profile, chosen before any registration.
     * @return Fresh owner or typed failure before publication.
     */
    static std::expected<MuxQueue, QueueFailure>
    createStream(const core::LocalSchema &schema, MuxLimits interests, core::SubscriptionLimits desired,
                 collectors::StreamLimits source, QueueLimits limits, QueueWork &work,
                 core::SubscriptionSelectorPolicy policy = core::SubscriptionSelectorPolicy::EXACT_ONLY);
    /** Create one real shared TICKER collector.
     * @param schema Immutable provenance.
     * @param interests Actual interest capacities.
     * @param source Actual cache/view capacities.
     * @param options Explicit source retention/quantum profile.
     * @param limits Queue aggregate maxima.
     * @param work Initialization credits.
     * @return Fresh owner or typed failure.
     */
    static std::expected<MuxQueue, QueueFailure> createTicker(const core::LocalSchema &schema, MuxLimits interests,
                                                              collectors::TickerLimits source,
                                                              collectors::TickerOptions options, QueueLimits limits,
                                                              QueueWork &work);
    /** Create one real shared HISTORY collector.
     * @param schema Immutable indexed provenance.
     * @param interests Actual interest capacities.
     * @param source Actual history/view capacities.
     * @param options Explicit retention/quantum profile.
     * @param limits Queue aggregate maxima.
     * @param work Initialization credits.
     * @return Fresh owner or typed failure.
     */
    static std::expected<MuxQueue, QueueFailure> createHistory(const core::LocalSchema &schema, MuxLimits interests,
                                                               collectors::HistoryLimits source,
                                                               collectors::HistoryOptions options, QueueLimits limits,
                                                               QueueWork &work);
    /** Validate an unbound source owner before transferring it into a composing runtime.
     * Existing registrations and desired commands are permitted; prior source admission/reset is not.
     * No callback, allocation, ownership transfer or state mutation occurs. Owner access is serialized.
     * @param schema Required exact immutable provenance, including an empty catalog.
     * @param family Required fixed source family.
     * @param policy Required immutable selector policy.
     * @param work Fixed inspection credits; a failed call may consume attempted work.
     * @return Success or typed state/context/profile/work refusal, leaving the queue usable.
     */
    [[nodiscard]] std::expected<void, QueueFailure> validateBinding(const core::LocalSchema &schema,
                                                                    core::SubscriptionFamily family,
                                                                    core::SubscriptionSelectorPolicy policy,
                                                                    QueueWork &work) const;
    /** Validate immutable receiver provenance without requiring a pristine source epoch.
     * Prior admission, staging and resets do not invalidate the immutable binding. This serialized
     * owner query performs no allocation, callback, ownership transfer or state mutation.
     * @param schema Required exact immutable provenance, including an empty catalog.
     * @param family Required fixed source family.
     * @param policy Required immutable selector policy.
     * @param work Fixed inspection credits; attempted work is not refunded on refusal.
     * @return Success or typed state/context/profile/work refusal, leaving the queue usable.
     */
    [[nodiscard]] std::expected<void, QueueFailure> validateReceiverBinding(const core::LocalSchema &schema,
                                                                            core::SubscriptionFamily family,
                                                                            core::SubscriptionSelectorPolicy policy,
                                                                            QueueWork &work) const;
    /** Reserve a registration before exposing its port.
     * @param limits Complete individual reservations.
     * @param consumer Caller-created callback, preserved on failure.
     * @param work Full fixed initialization, rollback and eventual detached-port disposal credits.
     * @return First public handle or typed refusal, preserving existing clients.
     */
    std::expected<MuxHandle, QueueFailure> open(ClientLimits limits, const std::shared_ptr<MuxConsumer> &consumer,
                                                QueueWork &work);
    /** Accept one whole compatible source event; stage precedes later commands.
     * @param event Owner transferred only on success.
     * @param work Admission credits.
     * @return Ownership barrier or typed refusal leaving input usable.
     */
    std::expected<QueueAdmission, QueueFailure> admit(protocol::LiveEvent &&event, QueueWork &work);
    /** Execute one finite fair owner action; no self-wait or nested dispatch.
     * @param work Remaining turn credits, never implicitly raised.
     * @return Actual progress or nonterminal typed rejection.
     */
    std::expected<QueueProgress, QueueFailure> progress(QueueWork &work);
    /** Progress retained cache while the composing owner fences fresh source input.
     * Only actual cached INITIAL projections constructed by this mode can start callbacks. Previously queued
     * source/live/initial records are discarded with genuine work/gap accounting, never relabelled as replay.
     * Caller-retained batches and source identity remain unchanged. An already executing callback may finish.
     * Serialized owner use; no wait, thread, wire flag or business EventSource is introduced.
     * @param work Remaining actual turn credits; exhaustion preserves the unconsumed queue prefix.
     * @return Existing fair progress result or typed refusal; no budget or lease charge is refunded.
     */
    std::expected<QueueProgress, QueueFailure> progressRetainedCache(QueueWork &work);
    /** Request context-wide stop without iterating/cleaning inline.
     * Graceful stop attempts to drain admitted projections, including unfinished staging/routing.
     * The terminal outcome records actual drain, abort or failure.
     * Caller-retained batches remain valid and charged after terminal publication.
     * Repeated requests can escalate graceful stop to abort before completion.
     * Requests after terminal publication leave the terminal state and evidence unchanged.
     * @param mode Graceful or escalating abort; allowed from its callback.
     */
    void requestStop(StopMode mode) noexcept;
    /** Request funded disposal of the current uncommitted source offer while preserving committed cache.
     * Serialized owner use, including its callback; no allocation, callback, disposal or epoch reset occurs inline.
     * Later progress releases pending input before staging/callbacks and rejects new admission until complete.
     * Already committed projections, registrations, source identity and caller-owned leases remain unchanged.
     * Exhausted progress work keeps this request pending; no quota/work is refunded. Empty/terminal is a no-op.
     */
    void
    requestDiscardPendingSource() noexcept; /** Accept/coalesce a phased source reset while retaining registration and
                                             * desired identities. STREAM explicitly unpins its old source in place;
                                             * TICKER/HISTORY retain actual reset/ACK behavior. Acceptance cuts off
                                             * later old-source admission and callback dispatch before returning.
                                             * @param work Bounded request-publication credits.
                                             * @return Accepted request, never synchronous reset/ACK completion.
                                             */
    std::expected<ResetAdmission, QueueFailure> resetSource(QueueWork &work);
    /** Copy current reset phase without advancing it.
     * @return Detached phase/commit/ACK evidence.
     */
    [[nodiscard]] ResetStatus resetStatus() const noexcept;
    /** Query exact current HISTORY request plus scoped raw source diagnostics.
     * @param client Current registration.
     * @param key Exact authoritative key.
     * @param work Lookup/matching/queue visit credits.
     * @return Detached guarded status; no current-complete claim from old mapping/input.
     */
    std::expected<QueueRangeStatus, QueueFailure> rangeStatus(const MuxHandle &client, core::SubscriptionKey key,
                                                              QueueWork &work) const;
    /** Copy observations for the exact current applied HISTORY interests of one registration.
     * Owner-serialized and nonreentrant; no progress, callback or reset acknowledgement occurs.
     * @param client Current registration in this queue's exact owner scope.
     * @param work One finite observation seed; attempts, copies and every per-key query consume it.
     * @return Owning pure key/status rows or a refusal; no partial vector or delivery/cache mutation escapes.
     */
    [[nodiscard]] std::expected<std::vector<QueueRangeObservation>, QueueFailure> rangeSnapshot(const MuxHandle &client,
                                                                                                QueueWork &work) const;
    /** Visit one current TICKER value for an applied exact registration.
     * The owner invokes the nonthrowing visitor synchronously. During the call, keep the queue alive and
     * stationary, do not retain the borrowed value, and do not reenter the queue.
     * @param client Exact registration in this queue.
     * @param key Borrowed exact source key.
     * @param work Finite queue and TICKER lookup credits.
     * @param context Caller-owned state borrowed for the invocation.
     * @param visitor Non-null bounded callback which cannot throw.
     * @return Exact mapping/value disposition, or typed refusal.
     */
    [[nodiscard]] std::expected<TickerLatestObservation, QueueFailure>
    visitTickerLatest(const MuxHandle &client, core::SubscriptionKey key, QueueWork &work, void *context,
                      void (*visitor)(void *, const collectors::TickerValue &) noexcept) const;
    /** Inspect queue and separate source scopes.
     * @return Requested logical counters, not RSS. Concurrent detached release may expose adjacent instants
     * across independently sampled counters; quiescent values are exact.
     */
    [[nodiscard]] QueueResources resources() const noexcept;
    /** Inspect future enforced capacities without allocating or changing the owner.
     * Desired entry/UTF16 peaks remain separate count axes; do not label the byte subtotal a whole-run byte bound.
     * @return Immutable configured component ceilings, or zeroed capacities after move.
     */
    [[nodiscard]] QueueCapacity capacity() const noexcept;
    /** Inspect one retained current registration.
     * @param client Exact port from this owner.
     * @return Actual/reserved counts or typed invalid-client.
     */
    std::expected<ClientResources, QueueFailure> resources(const MuxHandle &client) const noexcept;
    /** Borrow actual upstream desired union.
     * @return Span until effective mutation/retirement/move/destruction.
     */
    [[nodiscard]] std::span<const core::SubscriptionRequest> aggregateEntries() const noexcept;
    /** Inspect actual desired revision.
     * @return Global revision, zero after move.
     */
    [[nodiscard]] std::uint64_t revision() const noexcept;
    /** Inspect local context lifecycle.
     * @return TERMINAL after move; terminal result remains empty there.
     */
    [[nodiscard]] QueueState state() const noexcept;
    /** Inspect actual global local quiescence.
     * @return Result after all owned callbacks/commands/source cleanup retire.
     */
    [[nodiscard]] std::optional<QueueTerminal> terminal() const noexcept;
};

} // namespace org::ttldtor::qdcxx::mux
