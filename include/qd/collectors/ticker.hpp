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

/** Typed pure collector failures; no result implies wire or callback completion. */
enum class TickerError {
    INVALID_STATE,         ///< Moved-from collector.
    INVALID_CONFIGURATION, ///< Invalid finite capacities or zero retrieval quantum.
    INVALID_VIEW,          ///< Default, foreign, closed or reused view token.
    INVALID_CONTEXT,       ///< Input belongs to another immutable local schema.
    INVALID_EVENT,         ///< Input is not a complete TICKER_DATA event.
    INVALID_EPOCH,         ///< Another source epoch requires an explicit reset barrier.
    RESOURCE_LIMIT,        ///< Finite capacity exceeded; only stage rejection publishes shared STALE.
    WORK_LIMIT,            ///< Retryable turn exhaustion; published state and input are unchanged.
    ALLOCATION_FAILURE,    ///< Fixed backing or a new desired-state owner could not be allocated.
    STALE,                 ///< Shared cache failure prevents fresh delivery until reset.
    RESET_PENDING,         ///< The owner must acknowledge the exact reserved reset notice.
    NO_OFFER,              ///< Pop requires the current successful nonempty front.
    IDENTITY_EXHAUSTED     ///< A natural identity counter cannot advance without reuse.
};

/** Current collector freshness, independent of external session/socket state. */
enum class TickerState {
    EMPTY,        ///< No source epoch has been admitted in this local generation.
    CURRENT,      ///< Cache belongs to the pinned source epoch.
    STALE,        ///< Shared admission failed; retained values are stale evidence only.
    RESET_PENDING ///< Old data is gone and the exact owner reset notice awaits acknowledgment.
};

/** First value for a membership versus an update after its initial acknowledgment. */
enum class TickerDeliveryKind {
    INITIAL, ///< First acknowledged value for this membership activation.
    LIVE     ///< Changed latest value after initial acknowledgment.
};

/** Opaque fixed token backing; contains no collector pointer or protocol owner. */
struct TickerIdentity;

/** Detached owning identity for a view or local epoch generation.
 * Copies/releases of distinct token objects are thread-safe and retain only fixed scalar backing.
 * Access to the collector and every related protocol owner remains serialized and nonreentrant.
 * Tokens never keep a collector, cache frame, desired state or callable alive.
 * A reference count at the representable size_t maximum terminates before overflow; no counter wraps.
 */
class TickerToken {
    TickerIdentity *origin_ = nullptr; ///< Fixed atomic-reference identity, independent of collector state.
    std::size_t slot_ = 0;             ///< View slot, or the reserved epoch discriminator.
    std::uint64_t generation_ = 0;     ///< Non-reused view activation or local epoch generation.
    friend class TickerCollector;
    /** Retain one fixed origin with a validated selection.
     * @param origin Origin marker, or null for an invalid token.
     * @param slot View slot or epoch discriminator.
     * @param generation Nonzero local generation.
     */
    TickerToken(TickerIdentity *origin, std::size_t slot, std::uint64_t generation) noexcept;

    public:
    /** Construct an invalid token without allocation. */
    TickerToken() noexcept = default;
    /** Retain only fixed scalar identity.
     * @param other Source token; its object must not be concurrently mutated.
     */
    TickerToken(const TickerToken &other) noexcept;
    /** Transfer identity without allocation.
     * @param other Source, made invalid.
     */
    TickerToken(TickerToken &&other) noexcept;
    /** Replace this identity with a retained source.
     * @param other Source token.
     * @return This token.
     */
    TickerToken &operator=(const TickerToken &other) noexcept;
    /** Release this identity and transfer the source.
     * @param other Source token; self-move preserves it.
     * @return This token.
     */
    TickerToken &operator=(TickerToken &&other) noexcept;
    /** Release only fixed scalar identity; never a protocol owner. */
    ~TickerToken();
    /** Compare complete origin, kind and activation.
     * @param other Token to compare; invalid tokens never match.
     * @return True only for the same valid selection.
     */
    [[nodiscard]] bool matches(const TickerToken &other) const noexcept;
    /** Inspect identity presence, not current owner membership.
     * @return False for a default or moved-from token.
     */
    [[nodiscard]] bool valid() const noexcept;
};

/** View token; owner operations additionally verify its view discriminator and live activation. */
using TickerViewToken = TickerToken;
/** Local epoch token; equal numeric generations in another collector are unrelated. */
using TickerEpochToken = TickerToken;

/** Required finite collector capacities; no cumulative feed quota is imposed. */
struct TickerLimits {
    std::size_t viewSlots;    ///< Fixed independent local desired-state slots.
    std::size_t cacheKeys;    ///< Shared distinct cached keys; each view reserves this many pending keys.
    std::size_t cachedFrames; ///< Peak unique whole-event owners, including old plus incoming candidate.
    std::size_t cacheBytes;   ///< Peak event State plus full arena capacities, including unused shared records.
    std::size_t
        residentBytes; ///< Collector fixed/schema backing plus peak cacheBytes; separate core/session scopes excluded.
    core::SubscriptionLimits viewSubscriptions; ///< Independently enforced desired-state scope for every view.
};

/** Explicit source-compatible retention and retrieval scheduling; no ambient property lookup. */
struct TickerOptions {
    bool storeEverything;         ///< False evicts after last interest; true retains unrequested source keys.
    std::size_t retrievalQuantum; ///< Positive initial/update credit quantum; locked Java default is 100.
};

/** Caller-delimited operation credits, including failed work; never refunded. */
struct TickerWork {
    std::size_t operations; ///< Remaining owner operation attempts.
    std::size_t visits;     ///< Remaining metadata, matching, equality, queue and disposal visits.
    std::size_t textUnits;  ///< Remaining conservatively examined UTF-16 units.
    std::size_t bytes;      ///< Remaining examined payload bytes and conservative event/snapshot release work.
};

/** Desired command failure retaining the original pure-core cause when applicable. */
struct TickerCommandError {
    TickerError error; ///< Collector precondition/work failure, or INVALID_CONFIGURATION for a core rejection.
    std::optional<core::SubscriptionError> subscription; ///< Exact core rejection; absent before core application.
};

/** One atomic whole-event cache admission; input ownership transfers only if a final value is retained. */
struct TickerStage {
    std::size_t changed; ///< Effective source changes, including intermediate changes later conflated.
    std::size_t equal;   ///< Interested/stored inputs equal to the preceding candidate data.
    std::size_t ignored; ///< Inputs outside every interest when storeEverything is false.
    bool retained;       ///< True only when this collector consumed the event owner.
};

/** Borrowed immutable source evidence with explicit freshness and original provenance.
 * Successful cache mutation/eviction, pop/offer replacement, close causing eviction, shared STALE,
 * reset, move or destruction invalidates this result. Failed/no-op operations and equal-only stage preserve it.
 */
struct TickerValue {
    const protocol::LiveRecordView *sourceRecord; ///< Raw source metadata/payload, owned by a retained whole event.
    TickerEpochToken epoch;          ///< Detached local epoch scope; it does not retain the borrowed record.
    std::uint64_t bindingGeneration; ///< Original binding generation within the pinned source epoch.
    bool current;                    ///< False for explicitly inspected stale cache evidence.
};

/** Prepared single delivery; pop acknowledges collector state, not execution of application code. */
struct TickerDelivery {
    TickerValue value;                       ///< Borrowed exact source values; raw source flags remain intact.
    core::SubscriptionMembership membership; ///< Current owning core activation token.
    std::uint64_t revision;                  ///< View revision at initial admission or latest effective update.
    TickerDeliveryKind kind;                 ///< First membership value or subsequent live update.
    std::int32_t deliveryFlags; ///< Selected default-field TICKER projection, zero independently of raw source flags.
};

/** Saturating cumulative local observations, never a quantified upstream-loss claim. */
struct TickerCounts {
    std::size_t changed = 0;   ///< Effective admitted source changes.
    std::size_t equal = 0;     ///< Admitted equal inputs.
    std::size_t ignored = 0;   ///< Admitted unrequested inputs.
    std::size_t delivered = 0; ///< Explicitly popped pending values across views.
    std::size_t withdrawn = 0; ///< Pending values retired by desired mutation or close.
    std::size_t discarded = 0; ///< Pending values discarded by reset.
    std::size_t evicted = 0;   ///< Cached keys evicted by interest retirement or reset.
    std::size_t resets = 0;    ///< Applied reset barriers.
    bool saturated = false;    ///< At least one cumulative diagnostic counter saturated.
};

/** Reserved inline shared failure; incoming frame is unconsumed and prior cache becomes stale. */
struct TickerDiagnostic {
    std::optional<TickerError> error; ///< First shared resource cause in this local generation.
    std::size_t incoming = 0;         ///< Known records in the rejected whole event.
    std::size_t cached = 0;           ///< Known cache keys retained as stale evidence.
};

/** Reserved coalesced reset notice; acknowledgment is a global owner barrier only. */
struct TickerResetNotice {
    std::uint64_t sequence;                   ///< Exact newest reset sequence required by acknowledgeReset.
    TickerEpochToken firstEpoch;              ///< Earliest affected local epoch generation.
    TickerEpochToken nextEpoch;               ///< New reserved local generation; not peer acknowledgment.
    std::size_t skipped;                      ///< Additional reset barriers coalesced before acknowledgment.
    std::size_t discarded;                    ///< Known pending view values discarded across the interval.
    std::size_t evicted;                      ///< Known cached keys removed across the interval.
    std::optional<TickerError> previousCause; ///< First shared failure retained through the reset interval.
    bool unknownLoss;                         ///< Always true: upstream gap size is not known.
    bool saturated;                           ///< A coalesced diagnostic count saturated.
};

/** Pure shared latest cache and bounded independent local views.
 * Every owner operation, view, move and destruction is serialized and nonreentrant with related
 * LiveSession/Event/Snapshot/Identity owners. There are no callbacks, I/O, command port or upstream MUX.
 * Views reserve cacheKeys pending slots and never retain old frames independently of the shared cache.
 * Fixed/schema/cache requested backing excludes separately enforced desired/session storage, allocator
 * bookkeeping and process RSS. Destructor/move-assignment fallback releases bounded owners without turn
 * credits and promises no bounded latency or successful delivery.
 */
class TickerCollector {
    struct Impl;
    std::unique_ptr<Impl> impl_; ///< Unique serialized owner; absent after move.
    /** Adopt complete preallocated ownership.
     * @param impl Prepared storage with no remaining fallible transfer.
     */
    explicit TickerCollector(std::unique_ptr<Impl> impl) noexcept;

    public:
    /** Transfer ownership and invalidate borrowed offers.
     * @param other Source, made unusable.
     */
    TickerCollector(TickerCollector &&other) noexcept;
    /** Release old ownership and transfer another collector without delivery.
     * @param other Source; self-move preserves state.
     * @return This collector.
     */
    TickerCollector &operator=(TickerCollector &&other) noexcept;
    /** Release bounded cache, views, schema and source epoch without dispatch. */
    ~TickerCollector();
    /** Preallocate fixed capacities before consuming immutable schema ownership.
     * @param schema Validated immutable schema, transferred only on success.
     * @param limits Explicit finite backing and per-view desired capacities.
     * @param options Explicit retention and positive retrieval quantum.
     * @param work Turn credits, charged before allocation/initialization.
     * @return Empty-view collector or typed failure preserving schema ownership.
     */
    [[nodiscard]] static std::expected<TickerCollector, TickerError>
    create(core::LocalSchema &&schema, TickerLimits limits, TickerOptions options, TickerWork &work);
    /** Open an empty independent TICKER desired state using reserved pending storage.
     * @param work Turn credits including schema capture and slot initialization.
     * @return Fresh origin-bound token, or rejection leaving other views unchanged.
     */
    [[nodiscard]] std::expected<TickerViewToken, TickerError> openView(TickerWork &work);
    /** Retire a view and evict keys no other interest retains in false mode.
     * @param view Exact live view token.
     * @param work Turn credits including conservative release cost.
     * @return Success after retirement or rejection preserving ownership and views.
     */
    [[nodiscard]] std::expected<void, TickerError> closeView(const TickerViewToken &view, TickerWork &work);
    /** Apply an atomic core add and initialize only new cached memberships.
     * @param view Exact live view token.
     * @param requests Borrowed exact inputs, copied by core on effective publication.
     * @param subscriptionWork Core command credits.
     * @param work Full collector maintenance/release preflight credits.
     * @return Core application barrier or typed rejection preserving prior map/offers.
     */
    [[nodiscard]] std::expected<core::SubscriptionChange, TickerCommandError>
    add(const TickerViewToken &view, std::span<const core::SubscriptionRequest> requests,
        core::SubscriptionWork &subscriptionWork, TickerWork &work);
    /** Apply atomic removal and retire pending activations.
     * @param view Exact live view token.
     * @param keys Borrowed exact keys.
     * @param subscriptionWork Core command credits.
     * @param work Full collector maintenance/release preflight credits.
     * @return Core application barrier or rejection preserving prior state.
     */
    [[nodiscard]] std::expected<core::SubscriptionChange, TickerCommandError>
    remove(const TickerViewToken &view, std::span<const core::SubscriptionKey> keys,
           core::SubscriptionWork &subscriptionWork, TickerWork &work);
    /** Atomically replace one view's desired map.
     * @param view Exact live view token.
     * @param requests Complete borrowed desired map, ordered for new initial memberships.
     * @param subscriptionWork Core command credits.
     * @param work Full collector maintenance/release preflight credits.
     * @return Core result or typed rejection; unchanged memberships retain queue position.
     */
    [[nodiscard]] std::expected<core::SubscriptionChange, TickerCommandError>
    replace(const TickerViewToken &view, std::span<const core::SubscriptionRequest> requests,
            core::SubscriptionWork &subscriptionWork, TickerWork &work);
    /** Retire every membership of one view.
     * @param view Exact live view token.
     * @param subscriptionWork Core command credits.
     * @param work Full collector maintenance/release preflight credits.
     * @return Core result or rejection preserving prior state.
     */
    [[nodiscard]] std::expected<core::SubscriptionChange, TickerCommandError>
    clear(const TickerViewToken &view, core::SubscriptionWork &subscriptionWork, TickerWork &work);
    /** Atomically apply a whole already-admitted TICKER_DATA frame.
     * @param event Immutable source owner; transferred only when a final cache value retains it.
     * @param work Full frame/candidate/equality/disposal credits, never refunded.
     * @return Counts or typed failure. RESOURCE_LIMIT alone rejects input while publishing shared STALE;
     * WORK_LIMIT and other rejection preserve state and offers. Foreign epoch requires reset first.
     */
    [[nodiscard]] std::expected<TickerStage, TickerError> stage(protocol::LiveEvent &&event, TickerWork &work);
    /** Prepare one borrowed latest pending value without acknowledging it.
     * @param view Exact live view token.
     * @param work Metadata/offer credits.
     * @return Empty or a prepared value; stale/reset states refuse fresh delivery.
     */
    [[nodiscard]] std::expected<std::optional<TickerDelivery>, TickerError> front(const TickerViewToken &view,
                                                                                  TickerWork &work);
    /** Acknowledge exactly the current successful nonempty offer.
     * @param view Exact live view token.
     * @param work Queue/offer credits.
     * @return Success or rejection preserving the offer; never a callback completion.
     */
    [[nodiscard]] std::expected<void, TickerError> pop(const TickerViewToken &view, TickerWork &work);
    /** Inspect explicitly current/stale cache evidence without creating an interest.
     * @param key Borrowed exact key in this collector's schema.
     * @param work Full lookup/text credits.
     * @return Absent or a borrowed source value; a present zero value remains present.
     */
    [[nodiscard]] std::expected<std::optional<TickerValue>, TickerError> latest(core::SubscriptionKey key,
                                                                                TickerWork &work);
    /** Discard old epoch cache/pending state and reserve/coalesce a reset notice.
     * @param work Complete disposal and metadata credits.
     * @return Current exact notice or failure preserving the previous barrier.
     */
    [[nodiscard]] std::expected<TickerResetNotice, TickerError> reset(TickerWork &work);
    /** Acknowledge one exact global owner notice, never every downstream consumer.
     * @param sequence Current reset notice sequence; stale values are rejected.
     * @param work Constant owner operation credits.
     * @return Success permitting a fresh source epoch, or rejection preserving the notice.
     */
    [[nodiscard]] std::expected<void, TickerError> acknowledgeReset(std::uint64_t sequence, TickerWork &work);
    /** Borrow owned immutable schema, invalidated by move/assignment/destruction.
     * @return Null after move; no stage or view mutation changes this schema.
     */
    [[nodiscard]] const core::LocalSchema *schema() const noexcept;
    /** Borrow one core owner, invalidated by effective mutation/close/move/destruction.
     * @param view Exact live token; failed/no-op commands preserve borrowed views.
     * @return Null for invalid, foreign or closed selections.
     */
    [[nodiscard]] const core::SubscriptionState *desired(const TickerViewToken &view) const noexcept;
    /** Inspect one constant-time pending-key count.
     * @param view Exact live token.
     * @return Empty for invalid tokens; one key is counted once across both queues.
     */
    [[nodiscard]] std::optional<std::size_t> pendingKeys(const TickerViewToken &view) const noexcept;
    /** Inspect shared cache cardinality.
     * @return Distinct resident keys; zero after move.
     */
    [[nodiscard]] std::size_t cachedKeys() const noexcept;
    /** Inspect unique retained whole-frame count, independent of stalled view duration.
     * @return Resident cache frame owners; zero after move.
     */
    [[nodiscard]] std::size_t cachedFrames() const noexcept;
    /** Inspect unique retained event State plus full arena capacity.
     * @return Cache event bytes, excluding separately bounded snapshot/epoch storage.
     */
    [[nodiscard]] std::size_t cacheBytes() const noexcept;
    /** Inspect fixed/schema/cache requested backing.
     * @return Logical bytes excluding independent desired/session scopes and allocator overhead.
     */
    [[nodiscard]] std::size_t residentBytes() const noexcept;
    /** Inspect current freshness without changing state.
     * @return EMPTY after move; no external socket completion is implied.
     */
    [[nodiscard]] TickerState state() const noexcept;
    /** Inspect saturating local cumulative counts.
     * @return Zero counts after move.
     */
    [[nodiscard]] TickerCounts counts() const noexcept;
    /** Inspect reserved first shared failure.
     * @return Empty diagnostic for a healthy/moved-from collector.
     */
    [[nodiscard]] TickerDiagnostic diagnostic() const noexcept;
    /** Copy the detached current reset notice.
     * @return Empty when no owner acknowledgment is pending.
     */
    [[nodiscard]] std::optional<TickerResetNotice> resetNotice() const noexcept;
};

} // namespace org::ttldtor::qdcxx::collectors
