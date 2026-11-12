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
struct HistoryReplayAccess;
}

/** Typed owner failures; no outcome implies callback, wire or peer completion. */
enum class HistoryError {
    INVALID_STATE,            ///< Moved-from owner or wrong lifecycle state.
    INVALID_CONFIGURATION,    ///< Unrepresentable fixed storage or zero scheduling quantum.
    INVALID_VIEW,             ///< Default, foreign, closed or reused token.
    INVALID_KEY,              ///< No current membership for the selected exact key.
    INVALID_CONTEXT,          ///< An actual local record belongs to another immutable schema.
    INVALID_EVENT,            ///< Not a complete admitted HISTORY_DATA event.
    INVALID_EPOCH,            ///< A foreign source epoch requires explicit reset.
    INVALID_INDEX,            ///< Relevant non-removal index is the reserved signed64 maximum.
    CORE_REJECTED,            ///< Independently typed desired-state rejection is available.
    RESOURCE_LIMIT,           ///< Shared staging failure marks stale; ordinary command rejection preserves state.
    WORK_LIMIT,               ///< Retryable turn exhaustion preserves input, state and offers.
    ALLOCATION_FAILURE,       ///< Fixed factory or desired-state allocation failed before publication.
    FAILED_VIEW,              ///< Local pending reservation overflowed; evidence remains until close.
    STALE,                    ///< Shared gap prevents fresh delivery until reset.
    RESET_PENDING,            ///< Exact owner reset acknowledgment is required before fresh admission.
    NO_OFFER,                 ///< No current nonempty front can be acknowledged.
    IDENTITY_EXHAUSTED,       ///< A non-reused scalar identity cannot advance.
    UNSUPPORTED_SELECTOR,     ///< Immutable selector profile is not supported by HISTORY.
    UNSUPPORTED_SYMBOL_DOMAIN ///< Canonical source key-domain rejection preserves input/state/offers.
};

/** Finite error without text allocation or retained operations. */
struct HistoryFailure {
    HistoryError error;                                       ///< Owning layer failure.
    std::optional<core::SubscriptionError> subscriptionError; ///< Present only for CORE_REJECTED.
};

/** Shared freshness, independent of sockets and external source-driver completion. */
enum class HistoryState {
    EMPTY,        ///< No source epoch pinned in this local generation.
    CURRENT,      ///< Current pinned source; individual ranges can still be incomplete.
    STALE,        ///< Shared source/storage failure; retained evidence is not fresh.
    RESET_PENDING ///< Old data discarded; owner notice awaits exact acknowledgment.
};

/** Local view lifecycle; a failed token remains queryable until explicit close. */
enum class HistoryViewState {
    INVALID, ///< Foreign, closed or stale token.
    ACTIVE,  ///< Desired interests contribute to local retention.
    FAILED   ///< Reservation overflow; no further delivery or desired mutation.
};

/** Source-range snapshot facts; transaction/view consistency is reported separately. */
enum class HistorySnapshotState {
    UNKNOWN,   ///< No proved snapshot protocol for this requested range.
    RECEIVING, ///< Source frontier has not completed the requested range.
    COMPLETE,  ///< Source covered the requested range; inspect transaction/view facts too.
    TRUNCATED  ///< Snapshot finished at a snip frontier above the requested bound.
};

/** Distinguish real source values from locally synthesized projections. */
enum class HistoryDeliveryKind {
    INITIAL, ///< Current cached value in descending initial history.
    UPDATE,  ///< Real admitted update in the local update FIFO.
    REMOVAL, ///< Locally defaulted removal, possibly caused by a snapshot sweep.
    BOUNDARY ///< Locally defaulted snapshot or virtual transaction boundary.
};

/** Detached fixed identity; never references protocol/cache state. */
struct HistoryIdentity;

/** Opaque view/epoch identity with fixed scalar backing.
 * Copies and final release of distinct token objects are thread-safe. They never release a protocol
 * lease, desired owner or callable. Collector access and related protocol owners remain serialized
 * and nonreentrant. Representable reference-count exhaustion terminates before wrap.
 */
class HistoryToken {
    HistoryIdentity *origin_ = nullptr; ///< Fixed atomic-reference marker.
    std::size_t slot_ = 0;              ///< View slot, or reserved epoch discriminator.
    std::uint64_t generation_ = 0;      ///< Never-reused selection generation.
    friend class HistoryCollector;
    /** Retain fixed identity.
     * @param origin Marker, or null for invalid.
     * @param slot View slot or epoch discriminator.
     * @param generation Nonzero generation.
     */
    HistoryToken(HistoryIdentity *origin, std::size_t slot, std::uint64_t generation) noexcept;

    public:
    /** Construct an invalid token without allocation. */
    HistoryToken() noexcept = default;
    /** Retain scalar identity.
     * @param other Source object, not concurrently mutated.
     */
    HistoryToken(const HistoryToken &other) noexcept;
    /** Transfer identity.
     * @param other Source, made invalid.
     */
    HistoryToken(HistoryToken &&other) noexcept;
    /** Replace scalar identity.
     * @param other Source object.
     * @return This token.
     */
    HistoryToken &operator=(const HistoryToken &other) noexcept;
    /** Transfer scalar identity; self-move preserves it.
     * @param other Source object.
     * @return This token.
     */
    HistoryToken &operator=(HistoryToken &&other) noexcept;
    /** Release only fixed scalar backing. */
    ~HistoryToken();
    /** Compare complete origin/kind/activation; invalid tokens never match.
     * @param other Selection to compare.
     * @return Same valid identity, not proof of current owner membership.
     */
    [[nodiscard]] bool matches(const HistoryToken &other) const noexcept;
    /** Inspect identity presence.
     * @return False for default/moved-from token.
     */
    [[nodiscard]] bool valid() const noexcept;
};

/** View token additionally validated against the live owner slot. */
using HistoryViewToken = HistoryToken;
/** Collector-local epoch token, distinct from protocol binding generation. */
using HistoryEpochToken = HistoryToken;

/** Required logical capacities; zero means zero, not unlimited. */
struct HistoryLimits {
    std::size_t partitions;              ///< Exact record/symbol slots, including empty gating metadata.
    std::size_t points;                  ///< Shared signed64 indexed values.
    std::size_t symbolUnitsPerPartition; ///< Fixed exact UTF16 storage per partition in each bank.
    std::size_t cacheFrames;             ///< Peak unique shared event owners, including old plus incoming.
    std::size_t cacheBytes;              ///< Peak shared event State+arena capacities, excluding session ledger.
    std::size_t viewSlots;               ///< Fixed independent local desired-state slots.
    std::size_t pendingItems;            ///< Reserved local FIFO items per view.
    std::size_t pendingFrames;           ///< Peak referenced owners per view, including old plus incoming replacement.
    std::size_t pendingBytes;            ///< Peak whole-event State+arena charge per view.
    std::size_t residentBytes; ///< Fixed known backing plus unique event peak; separate core/session scopes excluded.
    core::SubscriptionLimits viewSubscriptions; ///< Independently enforced desired-state capacity per view.
};

/** Explicit source profile, without ambient properties or name-based business policy. */
struct HistoryOptions {
    bool storeEverything; ///< Retain unrequested points; false keeps only requested ranges plus bounded empty metadata.
    std::size_t retrievalQuantum; ///< Positive global-snapshot/local-update balance, selected source default100.
    std::size_t partitionSnapshotQuantum; ///< Positive per-partition snapshot rotation, source default10000.
    core::SubscriptionSelectorPolicy selectorPolicy =
        core::SubscriptionSelectorPolicy::EXACT_ONLY; ///< Immutable symbol domain; no All.
    std::span<const std::int32_t> retransmitSnapshotRecords = {};
    ///< Exact indexed local IDs that restart snapshot views on every BEGIN or MODE. Borrowed through create only.
};

/** Explicit view projection; false emits legacy unflagged data while preserving raw provenance. */
struct HistoryViewOptions {
    bool historySnapshot; ///< Use selected snapshot/transaction projection flags.
};

/** Remaining caller-delimited credits, charged on attempts and never refunded. */
struct HistoryWork {
    std::size_t operations; ///< Owner operation attempts.
    std::size_t visits;     ///< Metadata/field/matching/queue/init/release visits.
    std::size_t textUnits;  ///< Conservatively examined/copied UTF16 units.
    std::size_t bytes;      ///< Examined/copied payload and conservative shared-disposal work allowance.
};

/** Borrowed projected field owned by retained source or this view's fixed offer scratch. */
struct HistoryFieldValueView {
    core::SerialFieldType type;                      ///< Exact full serial identity.
    std::int64_t raw;                                ///< Exact scalar bits; synthetic ordinary defaults are zero.
    std::optional<std::u16string_view> units;        ///< Exact null/empty/text distinction.
    std::optional<std::span<const std::byte>> bytes; ///< Exact null/empty/opaque bytes.
};

/** Borrowed projection, deliberately distinct from an admitted protocol record. */
struct HistoryProjectedRecordView {
    std::int32_t localId;                          ///< Local schema record identity.
    core::RecordContext context;                   ///< Owning local provenance only.
    std::u16string_view symbol;                    ///< Exact collector-owned symbol units.
    std::int64_t rawTime;                          ///< Full signed64 raw inner index; no business source decomposition.
    std::int32_t eventFlags;                       ///< Produced projection flags, not original source flags.
    std::span<const HistoryFieldValueView> fields; ///< Exact fields, valid until offer invalidation.
};

/** Exact detached scalar metadata of a real contributing source record. */
struct HistorySourceMetadata {
    std::int32_t senderId;                             ///< Actual source sender ID, never delivery identity.
    std::int32_t cipher;                               ///< Actual raw source cipher.
    std::int32_t flags;                                ///< Actual source flags before projection.
    std::int64_t rawTime;                              ///< Actual source indexed time.
    std::uint64_t bindingGeneration;                   ///< Actual immutable source binding generation.
    std::optional<protocol::FieldEventTime> fieldTime; ///< Actual independent field-prefix metadata.
    std::optional<std::int64_t> tapeMessageTimeMs{};   ///< Optional tape MESSAGE cursor time of the true source.
};

/** One borrowed prepared projection; pop acknowledges collector consumption only. */
struct HistoryDelivery {
    HistoryProjectedRecordView record; ///< Projected value, including explicitly synthesized defaults.
    HistoryDeliveryKind kind;          ///< Projection origin.
    std::optional<protocol::LiveRecordView>
        sourceRecord;                             ///< True retained admitted source, absent for synthetic output.
    std::optional<HistorySourceMetadata> trigger; ///< Real contributing scalar provenance, absent when none exists.
    core::SubscriptionMembership membership;      ///< Exact desired activation.
    std::uint64_t admissionRevision;              ///< Desired revision when this projection became eligible.
    HistoryEpochToken epoch;                      ///< Detached local epoch identity.
};

/** Detached AS-OF range status, not a self-updating freshness lease. */
struct HistoryRangeStatus {
    HistoryEpochToken epoch;                  ///< Local epoch at observation.
    core::SubscriptionMembership membership;  ///< Exact current activation at observation.
    std::uint64_t revision;                   ///< Desired owner revision at observation.
    std::uint64_t observationSerial;          ///< Non-reused owner publication serial.
    std::uint64_t partitionActivation;        ///< Non-reused partition identity, or zero before creation.
    std::int64_t requestedFrom;               ///< Inclusive signed64 bound.
    std::int64_t sourceFrontier;              ///< Lowest current descending snapshot frontier.
    std::int64_t knownFrontier;               ///< Lowest source coverage ever retained in this partition.
    std::optional<std::int64_t> snipFrontier; ///< Explicit truncation frontier, if any.
    HistorySnapshotState
        snapshot; ///< Source snapshot marker progress; a higher requested range can already be covered.
    bool sourceTransactionPending; ///< Explicit or implicit source transaction remains open.
    bool initialPending;           ///< Initial projection remains queued.
    bool viewTransactionDirty;     ///< This view has an inconsistent mixed projection.
    bool completeRequestedRange;   ///< Only true for covered, consistent, fresh source and view.
};

/** Reserved local failure evidence, detached from all operation/protocol owners. */
struct HistoryViewFailure {
    HistoryError cause;                ///< Local pending reservation failure.
    std::size_t sourceRecordOrdinal;   ///< Zero-based source record causing this failure.
    std::size_t pendingItemsDiscarded; ///< Exact materialized FIFO items discarded at failure commit.
    bool initialIncomplete;            ///< An unmaterialized initial projection was interrupted.
    bool unknownFollowingLoss;         ///< Later/upstream/application loss is not an invented zero.
};

/** Exact bounded outcome of one admitted whole source frame. */
struct HistoryStage {
    std::size_t admittedRecords;   ///< Actual admitted local records examined.
    std::size_t ignoredRecords;    ///< No relevant interest or snapshot gating.
    std::size_t changedPoints;     ///< Actual insert/update/remove effects.
    std::size_t syntheticRemovals; ///< Cache points swept/snipped.
    std::size_t localConflations;  ///< Most-recent same-index FIFO replacements.
    std::size_t failedViews;       ///< Newly failed views in this atomic commit.
};

/** Owner-only reset interval; not downstream acknowledgment or wire closure. */
struct HistoryResetNotice {
    std::uint64_t sequence;       ///< Exact latest sequence required by acknowledgeReset.
    HistoryEpochToken from;       ///< Earliest affected local epoch.
    HistoryEpochToken to;         ///< Newest empty local epoch.
    std::size_t discardedPoints;  ///< Known retained points discarded across this interval.
    std::size_t discardedPending; ///< Known materialized local FIFO items discarded.
    std::size_t skippedResets;    ///< Additional coalesced reset calls.
    bool unknownLoss;             ///< Always explicit for unobserved source/application continuity.
    bool countsSaturated;         ///< Diagnostic accumulation overflowed representable size_t.
};

/** Synchronous explicit empty-metadata release; no downstream ACK is needed. */
struct HistoryRetentionReport {
    std::uint64_t generation;        ///< Non-reused retention operation generation; unchanged for no-op.
    std::size_t forgottenPartitions; ///< Empty/unobserved partitions removed.
    bool continuityForgotten;        ///< Later add starts unknown, never revives prior partition identity.
};

/** Bounded pure HISTORY owner.
 * Every method and all related source-event destruction are caller-serialized and nonreentrant.
 * No background progress, callbacks, clocks, upstream union or transport completion is provided.
 * Successful publication/pop/reset/close/forget/move invalidates all borrowed offers before release.
 * Failed/no-op commands and WORK_LIMIT preserve offers; shared source/resource failure explicitly
 * invalidates fresh offers by marking STALE while retaining input. Destructor fallback releases finite
 * ownership without a work/latency guarantee. Java clock-driven empty-state expiry is excluded;
 * forgetUnobserved releases only empty, unobserved metadata without resetting healthy views.
 */
class HistoryCollector {
    friend struct detail::HistoryReplayAccess;
    struct Impl;
    std::unique_ptr<Impl> impl_; ///< Serialized complete state.
    /** Adopt complete fixed storage.
     * @param impl Fully initialized owner.
     */
    explicit HistoryCollector(std::unique_ptr<Impl> impl) noexcept;

    public:
    /** Release finite source owners on the caller's serialized owner domain. */
    ~HistoryCollector();
    /** Copying complete serialized ownership is disabled.
     * @param other Owner that cannot be copied.
     */
    HistoryCollector(const HistoryCollector &other) = delete;
    /** Copy assignment is disabled.
     * @param other Owner that cannot be copied.
     * @return Unavailable because copying is disabled.
     */
    HistoryCollector &operator=(const HistoryCollector &other) = delete;
    /** Transfer owner, invalidating prepared offers on both sides.
     * @param other Source made invalid.
     */
    HistoryCollector(HistoryCollector &&other) noexcept;
    /** Release old owner and transfer; self-move preserves state.
     * @param other Source made invalid.
     * @return This owner.
     */
    HistoryCollector &operator=(HistoryCollector &&other) noexcept;
    /** Allocate and validate fixed backing before consuming schema.
     * @param schema Owned immutable indexed/ordinary declarations; preserved on failure.
     * @param limits Finite logical capacities, including separately scoped core limits.
     * @param options Explicit retention, positive scheduling quanta and optional borrowed retransmission IDs.
     * @param work Factory initialization and schema examination credits.
     * @return Empty collector or typed rejection; no I/O or user code.
     */
    [[nodiscard]] static std::expected<HistoryCollector, HistoryFailure>
    create(core::LocalSchema &&schema, HistoryLimits limits, HistoryOptions options, HistoryWork &work);
    /** Open one empty desired owner with reserved local queue/control capacity.
     * @param options Explicit snapshot projection mode.
     * @param work Owner/schema initialization credits.
     * @return Detached live token or isolated rejection.
     */
    [[nodiscard]] std::expected<HistoryViewToken, HistoryFailure> openView(HistoryViewOptions options,
                                                                           HistoryWork &work);
    /** Retire exact view and release its local owners; no allocation or callback.
     * @param view Live or failed token.
     * @param work Full maintenance/disposal credits; WORK_LIMIT preserves the view.
     * @return Local barrier, not transport quiescence.
     */
    [[nodiscard]] std::expected<void, HistoryFailure> closeView(const HistoryViewToken &view, HistoryWork &work);
    /** Atomically upsert interests; changed bounds receive fresh membership/initial projection.
     * @param view Active view token.
     * @param requests Complete borrowed inputs, possibly aliasing current desired storage.
     * @param coreWork Independently bounded core command work.
     * @param work Collector candidate/maintenance/release credits.
     * @return Applied revision/no-op or typed rejection preserving state and offers.
     */
    [[nodiscard]] std::expected<core::SubscriptionChange, HistoryFailure>
    add(const HistoryViewToken &view, std::span<const core::SubscriptionRequest> requests,
        core::SubscriptionWork &coreWork, HistoryWork &work);
    /** Remove exact interests; absent keys are a no-op.
     * @param view Active view token.
     * @param keys Borrowed exact keys, possibly aliasing desired storage.
     * @param coreWork Independent command work.
     * @param work Collector maintenance/release credits.
     * @return Application barrier or rejection with rollback.
     */
    [[nodiscard]] std::expected<core::SubscriptionChange, HistoryFailure>
    remove(const HistoryViewToken &view, std::span<const core::SubscriptionKey> keys, core::SubscriptionWork &coreWork,
           HistoryWork &work);
    /** Publish exactly the validated desired map.
     * @param view Active view token.
     * @param requests Complete borrowed replacement inputs.
     * @param coreWork Independent command work.
     * @param work Collector maintenance/release credits.
     * @return Application barrier/no-op or rejection with rollback.
     */
    [[nodiscard]] std::expected<core::SubscriptionChange, HistoryFailure>
    replace(const HistoryViewToken &view, std::span<const core::SubscriptionRequest> requests,
            core::SubscriptionWork &coreWork, HistoryWork &work);
    /** Clear desired interests, retaining bounded empty source gating metadata.
     * @param view Active view token.
     * @param coreWork Independent command work.
     * @param work Collector maintenance/release credits.
     * @return Application barrier/no-op or rejection with rollback.
     */
    [[nodiscard]] std::expected<core::SubscriptionChange, HistoryFailure>
    clear(const HistoryViewToken &view, core::SubscriptionWork &coreWork, HistoryWork &work);
    /** Stage one complete HISTORY_DATA20 event atomically.
     * @param event Actual admitted source; consumed only on success, including ignored frames.
     * Canonical domain/work refusal precedes candidate activation and preserves the whole input and old offers;
     * this preflight is separate from shared RESOURCE_LIMIT transitions to STALE.
     * @param work Full candidate/peak/normalization/disposal credits.
     * @return Exact stage counts. Shared capacity/source failure marks STALE and leaves input owned;
     * WORK_LIMIT/context/family rejection preserves prior state/offers/input. Individual pending
     * failures publish only after whole shared admission succeeds. Foreign epoch needs reset.
     */
    [[nodiscard]] std::expected<HistoryStage, HistoryFailure> stage(protocol::LiveEvent &&event, HistoryWork &work);
    /** Prepare a read-only projection; full preflight precedes offer scratch writes.
     * @param view Active view token.
     * @param work Selection/default-field work credits.
     * @return Empty or borrowed offer; no semantic cursor/credit changes before pop.
     */
    [[nodiscard]] std::expected<std::optional<HistoryDelivery>, HistoryFailure> front(const HistoryViewToken &view,
                                                                                      HistoryWork &work);
    /** Commit exactly the current prepared single offer.
     * @param view Active view token.
     * @param work Complete commit/invalidation/disposal credits.
     * @return Local consumption barrier or rejection preserving the offer.
     */
    [[nodiscard]] std::expected<void, HistoryFailure> pop(const HistoryViewToken &view, HistoryWork &work);
    /** Observe one current desired range; result is detached AS-OF evidence.
     * @param view Active or failed view token.
     * @param key Exact current desired key.
     * @param work Bounded matching/status credits.
     * @return Scoped status or typed invalid-key/view error; no mutable lease.
     */
    [[nodiscard]] std::expected<HistoryRangeStatus, HistoryFailure>
    rangeStatus(const HistoryViewToken &view, const core::SubscriptionKey &key, HistoryWork &work) const;
    /** Explicitly forget empty metadata with no live interest or pending references.
     * @param work Full scan/release credits; no-op/failure preserves offers.
     * @return Synchronous continuity report with non-reused generation; no ACK or healthy reset.
     */
    [[nodiscard]] std::expected<HistoryRetentionReport, HistoryFailure> forgetUnobserved(HistoryWork &work);
    /** Discard old source/pending state and reserve/coalesce an incomplete interval.
     * @param work Full fixed-state/disposal credits.
     * @return Detached exact owner reset notice; desired interests remain.
     */
    [[nodiscard]] std::expected<HistoryResetNotice, HistoryFailure> reset(HistoryWork &work);
    /** Acknowledge only the exact current local reset interval.
     * @param sequence Current notice sequence.
     * @return Owner barrier or typed rejection; no peer/application acknowledgment.
     */
    [[nodiscard]] std::expected<void, HistoryFailure> acknowledgeReset(std::uint64_t sequence) noexcept;
    /** Borrow immutable schema.
     * @return Pointer until move/destruction, null after move.
     */
    [[nodiscard]] const core::LocalSchema *schema() const noexcept;
    /** Borrow desired evidence; failed view remains queryable.
     * @param view Current active/failed token.
     * @return Owner until effective desired mutation/close/move/destruction, or null.
     */
    [[nodiscard]] const core::SubscriptionState *desired(const HistoryViewToken &view) const noexcept;
    /** Inspect shared freshness.
     * @return EMPTY after move; not proof of range completeness.
     */
    [[nodiscard]] HistoryState state() const noexcept;
    /** Inspect exact view lifecycle.
     * @param view Detached token to validate.
     * @return INVALID for foreign/closed/reused selections.
     */
    [[nodiscard]] HistoryViewState viewState(const HistoryViewToken &view) const noexcept;
    /** Copy reserved local failure evidence without retaining operations.
     * @param view Current token.
     * @return Failure until explicit close, or empty.
     */
    [[nodiscard]] std::optional<HistoryViewFailure> viewFailure(const HistoryViewToken &view) const noexcept;
    /** Inspect stored partition metadata count.
     * @return Includes empty retained snapshot gating state.
     */
    [[nodiscard]] std::size_t partitionCount() const noexcept;
    /** Inspect shared indexed values.
     * @return Current retained point count.
     */
    [[nodiscard]] std::size_t pointCount() const noexcept;
    /** Inspect materialized local FIFO occupancy.
     * @param view Current token.
     * @return Exact queued items; zero for invalid/failed view.
     */
    [[nodiscard]] std::size_t pendingCount(const HistoryViewToken &view) const noexcept;
    /** Inspect unique whole-event owner count.
     * @return Cache/local aliases count once.
     */
    [[nodiscard]] std::size_t frameCount() const noexcept;
    /** Inspect unique event State+arena bytes.
     * @return Excludes separately enforced actual session snapshot/epoch scope.
     */
    [[nodiscard]] std::size_t eventBytes() const noexcept;
    /** Inspect known fixed/schema plus unique event backing.
     * @return Logical bytes, excluding independent desired/session scopes and allocator overhead.
     */
    [[nodiscard]] std::size_t residentBytes() const noexcept;
    /** Copy exact reserved owner reset interval.
     * @return Pending notice, or empty.
     */
    [[nodiscard]] std::optional<HistoryResetNotice> resetNotice() const noexcept;
};

} // namespace org::ttldtor::qdcxx::collectors
