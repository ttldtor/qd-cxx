// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/subscription.hpp"
#include "qd/protocol/live_session.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

namespace org::ttldtor::qdcxx::collectors {
namespace detail {
struct StreamReplaceAccess;
}

/** Local staging failures, independent of connection and callback completion. */
enum class StreamError {
    INVALID_STATE,            ///< Moved-from collector, terminal overflow, or exhausted reset identity.
    INVALID_FAMILY,           ///< Desired state does not select STREAM or is moved from.
    INVALID_EVENT,            ///< Input is not a complete admitted STREAM_DATA event.
    INVALID_EPOCH,            ///< Input belongs to another epoch; reset the source before switching.
    RESOURCE_LIMIT,           ///< Terminal overflow; no incoming record is staged or silently conflated.
    WORK_LIMIT,               ///< Nonterminal exhausted turn credits; queue and input ownership are unchanged.
    ALLOCATION_FAILURE,       ///< Factory backing allocation failed before desired-state transfer.
    NO_OFFER,                 ///< pop requires a successful nonempty front after the last effective mutation.
    UNSUPPORTED_SYMBOL_DOMAIN ///< Canonical profile rejects a noncanonical admitted cipher/text pair; input unchanged.
};

/** Required finite capacities, including zero; no lifetime record quota is imposed. */
struct StreamLimits {
    std::size_t frames;        ///< Maximum retained whole admitted event owners.
    std::size_t records;       ///< Maximum selected queued record slots, including identical records.
    std::size_t stagedBytes;   ///< Maximum unique retained event State plus arena backing.
    std::size_t residentBytes; ///< Fixed collector backing plus stagedBytes, excluding separately bounded
                               ///< desired/session storage.
};

/** Caller-delimited turn credits; successful charges are never refunded. */
struct StreamWork {
    std::size_t operations; ///< Remaining factory/stage/front/pop/discard attempts.
    std::size_t visits;     ///< Remaining metadata initialization, matching, membership and disposal visits.
    std::size_t textUnits;  ///< Remaining conservatively examined UTF-16 units.
    std::size_t bytes;      ///< Remaining fixed-backing initialization and conservative shared-owner release work.
};

/** One complete successful collector admission; zero selection leaves the input owner unchanged. */
struct StreamStage {
    std::size_t selected;   ///< Records staged in original source order.
    std::size_t ignored;    ///< Unrequested records in this frame, never cached.
    std::uint64_t revision; ///< Desired revision used for the whole frame.
};

/** One committed explicit source reset; desired identities and cumulative counters survive. */
struct StreamReset {
    std::uint64_t sequence; ///< Nonzero collector-local reset identity, never reused.
    std::size_t discarded;  ///< Actual selected queued records discarded without delivery.
    bool countsSaturated;   ///< Cumulative diagnostic arithmetic has saturated.
    bool unknownLoss;       ///< Upstream continuity cannot be quantified by a local reset.
};

/** Borrowed next record; front does not acknowledge delivery or invoke application code. */
struct StreamDelivery {
    const protocol::LiveRecordView *record; ///< Event-owned value; invalidated by successful nonempty stage/pop/discard
                                            ///< or effective desired change.
    std::uint64_t revision;                 ///< Desired revision at admission, not peer acknowledgement.
    core::SubscriptionMembership membership; ///< Owning admission activation, independent of borrowed record storage.
    std::uint64_t bindingGeneration =
        0; ///< Original event snapshot generation, scoped to the collector's pinned epoch.
};

/** Atomic acknowledgement of one prepared prefix, independent of user callback execution. */
struct StreamAcknowledged {
    std::size_t records;   ///< Eligible records consumed from the prepared offer.
    std::size_t withdrawn; ///< Retired records discarded from the same source prefix.
};

/** Cumulative local observations; saturated counters are explicitly marked. */
struct StreamCounts {
    std::size_t staged = 0;    ///< Selected admitted records, including later withdrawals/discards.
    std::size_t delivered = 0; ///< Records explicitly acknowledged by pop.
    std::size_t withdrawn = 0; ///< Queued records discarded because their activation was retired.
    std::size_t discarded = 0; ///< Queued records explicitly discarded without delivery.
    std::size_t ignored = 0;   ///< Unrequested input records, never retained.
    bool saturated = false;    ///< At least one cumulative counter reached its representable maximum.
};

/** Reserved inline evidence of the first terminal staging overflow; upstream loss is not quantified. */
struct StreamDiagnostic {
    std::optional<StreamError> error; ///< RESOURCE_LIMIT after overflow; absent for a healthy collector.
    std::size_t pending = 0;  ///< Known queued records at terminal overflow, still retained until discard/destruction.
    std::size_t incoming = 0; ///< Known eligible records rejected atomically from the overflowing frame.
};

/** Bounded pure STREAM queue owning desired state and complete admitted event owners.
 * All operations, views, moves and destruction require serialized, non-reentrant access, including
 * related LiveSession/LiveIdentity owners. No background work or user callbacks exist. First selected
 * admission pins one epoch through drain; newer bindings within it are accepted without reinterpreting
 * queued records. A new epoch requires explicit resetSource or a new collector. Staging counts unique event backing;
 * desired state and shared snapshots/epoch reservations remain in their separate existing bounds. An upper owner must
 * aggregate those scopes. Logical bounds exclude allocator overhead and process RSS. Overflow retires delivery visibly
 * and retains its bounded queue for work-bounded discard or destruction. Destruction and move-assignment cleanup
 * release bounded owners without caller credits; it never dispatches or reports delivery.
 */
class StreamCollector {
    friend struct detail::StreamReplaceAccess;
    struct Impl;
    std::unique_ptr<Impl> impl_; ///< Unique mutable owner; empty after move.
    /** Adopt fully allocated storage after all fallible preparation.
     * @param impl Complete prepared owner.
     */
    explicit StreamCollector(std::unique_ptr<Impl> impl) noexcept;

    public:
    /** Transfer ownership and invalidate borrowed delivery views.
     * @param other Source collector, made unusable.
     */
    StreamCollector(StreamCollector &&other) noexcept;
    /** Release old ownership and transfer another collector without dispatching.
     * @param other Source collector, made unusable unless self-assigned.
     * @return This collector.
     */
    StreamCollector &operator=(StreamCollector &&other) noexcept;
    /** Release fixed backing, staged frames, desired state and the pinned epoch. */
    ~StreamCollector();
    /** Allocate fixed ring slots and validate desired state before ownership transfer.
     * @param desired Usable STREAM state, consumed only on success; no-op validation preserves its revision/views.
     * @param limits Explicit immutable finite capacities.
     * @param work Mutable turn credits, including failed preparation and initialization.
     * @return Empty collector or typed failure leaving desired ownership unchanged.
     */
    [[nodiscard]] static std::expected<StreamCollector, StreamError> create(core::SubscriptionState &&desired,
                                                                            StreamLimits limits, StreamWork &work);
    /** Forward an atomic desired add; only effective change invalidates the outstanding offer.
     * @param requests Borrowed exact keys, copied by the existing desired-state owner on success.
     * @param work Existing desired-state command credits.
     * @return Existing revision/change result, or typed rejection; terminal collectors refuse additions.
     */
    [[nodiscard]] std::expected<core::SubscriptionChange, core::SubscriptionError>
    add(std::span<const core::SubscriptionRequest> requests, core::SubscriptionWork &work);
    /** Forward removal, including after terminal overflow; old queued activations become withdrawn.
     * @param keys Exact borrowed keys.
     * @param work Existing desired-state command credits.
     * @return Existing revision/change result or typed rejection, preserving offers on failure/no-op.
     */
    [[nodiscard]] std::expected<core::SubscriptionChange, core::SubscriptionError>
    remove(std::span<const core::SubscriptionKey> keys, core::SubscriptionWork &work);
    /** Forward atomic replacement; only effective change invalidates the outstanding offer.
     * @param requests Complete desired exact map.
     * @param work Existing desired-state command credits.
     * @return Existing result or rejection; terminal collectors refuse replacement.
     */
    [[nodiscard]] std::expected<core::SubscriptionChange, core::SubscriptionError>
    replace(std::span<const core::SubscriptionRequest> requests, core::SubscriptionWork &work);
    /** Retire all desired entries, including after terminal overflow.
     * @param work Existing desired-state command credits.
     * @return Existing revision/change result or rejection; queued owners release through front/discard.
     */
    [[nodiscard]] std::expected<core::SubscriptionChange, core::SubscriptionError> clear(core::SubscriptionWork &work);
    /** Atomically stage every currently eligible record from one whole admitted STREAM_DATA frame.
     * @param event Complete source owner, transferred only for a successful nonzero selection.
     * A canonical-profile domain refusal preserves the complete input, queued records and outstanding offer.
     * @param work Mutable turn credits; full matching/publication work is reserved before queue mutation.
     * @return Selection/ignore counts or failure. A zero selection preserves the prior offer. RESOURCE_LIMIT retires
     * delivery, preserves the queue and input owner, and records known pending/incoming counts. Other failures preserve
     * the existing offer.
     */
    [[nodiscard]] std::expected<StreamStage, StreamError> stage(protocol::LiveEvent &&event, StreamWork &work);
    /** Withdraw retired activations and offer the first eligible record in source order.
     * @param work Mutable turn credits, including conservative event/snapshot release work.
     * @return Borrowed optional delivery or failure; failed preflight preserves queue and prior offer.
     * Successful withdrawal invalidates previous views. Repeated front alone does not acknowledge data.
     */
    [[nodiscard]] std::expected<std::optional<StreamDelivery>, StreamError> front(StreamWork &work);
    /** Prepare a source-ordered eligible batch without consuming its queued prefix.
     * @param storage Caller-owned fixed metadata slots, preserved until acknowledgement or offer invalidation.
     * @param work Mutable credits for the complete eligibility/metadata preflight; no per-call allocation occurs.
     * @return Borrowed selected prefix metadata, or failure preserving queue and previous offer. A successful
     * call replaces a single/batch offer. Empty storage offers prefix zero; positive storage can offer zero
     * records with a wholly withdrawn prefix. Effective mutation, nonempty stage, successful front/pop/discard,
     * another preparation or move/destruction invalidates the span. Caller storage must not alias collector state.
     */
    [[nodiscard]] std::expected<std::span<const StreamDelivery>, StreamError>
    prepareBatch(std::span<StreamDelivery> storage, StreamWork &work);
    /** Atomically acknowledge only the most recent valid prepared batch, never a single front offer.
     * @param work Mutable credits, including conservative shared-owner release work before any consumption.
     * @return Consumed eligible/withdrawn counts, or failure preserving queue/offer. No user code runs;
     * acknowledgement is not callback start/completion. Success invalidates every prepared span.
     */
    [[nodiscard]] std::expected<StreamAcknowledged, StreamError> acknowledgeBatch(StreamWork &work);
    /** Acknowledge exactly the currently offered record, without invoking user code.
     * @param work Mutable turn credits; last-record owner release is charged before mutation.
     * @return Success or failure preserving queue and offer. Terminal overflow prohibits delivery.
     */
    [[nodiscard]] std::expected<void, StreamError> pop(StreamWork &work);
    /** Discard all queued records atomically, including after overflow, without claiming delivery.
     * @param work Mutable credits for all item and shared-owner release work before mutation.
     * @return Known discarded count or failure preserving the queue. Epoch pin and terminal cause persist.
     */
    [[nodiscard]] std::expected<std::size_t, StreamError> discard(StreamWork &work);
    /** Discard a healthy source and unpin its epoch while preserving desired state and fixed backing.
     * Preflight failure preserves queued records, offers, desired identities, epoch and reset sequence;
     * attempted work may be consumed. Successful reset invalidates offers and permits a new epoch.
     * Ordinary discard alone still preserves its epoch. Terminal overflow cannot be revived by reset.
     * @param work Complete item/frame visits and conservative frame/epoch disposal credits.
     * @return Committed actual discard and source-reset identity, or a typed refusal before mutation.
     */
    [[nodiscard]] std::expected<StreamReset, StreamError> resetSource(StreamWork &work);
    /** Borrow immutable desired demand for outgoing composition.
     * @return State valid until move/destruction; null for a moved-from collector.
     */
    [[nodiscard]] const core::SubscriptionState *desired() const noexcept;
    /** Inspect queued selected records, including retired entries not yet examined by front.
     * @return Bounded item count, or zero after move.
     */
    [[nodiscard]] std::size_t queuedRecords() const noexcept;
    /** Inspect fixed collector plus unique staged-event backing.
     * @return Requested byte scope excluding shared snapshot/epoch and desired storage; zero after move.
     */
    [[nodiscard]] std::size_t residentBytes() const noexcept;
    /** Inspect just unique whole-event backing retained in staging.
     * @return Requested State plus arena bytes, including unrequested records sharing retained frames.
     */
    [[nodiscard]] std::size_t stagedBytes() const noexcept;
    /** Inspect local cumulative observations without changing state.
     * @return Counts with explicit saturation status; zero after move.
     */
    [[nodiscard]] StreamCounts counts() const noexcept;
    /** Inspect reserved first-overflow evidence without allocation.
     * @return Terminal cause/counts, or empty evidence for healthy/moved-from collectors.
     */
    [[nodiscard]] StreamDiagnostic diagnostic() const noexcept;
};

} // namespace org::ttldtor::qdcxx::collectors
