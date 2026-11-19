// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/symbol_striper.hpp"
#include "qd/mux/queue.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace org::ttldtor::qdcxx::mux {
namespace detail {
struct StripePort;
struct StripeRoot;
} // namespace detail

/** Detached composition refusal; nested child errors retain their original categories. */
enum class StripeError {
    INVALID_STATE,         ///< Default or moved-from root has no execution state.
    INVALID_CONFIGURATION, ///< Root configuration or command shape is invalid.
    INVALID_CLIENT,        ///< Token does not name this root's complete registration.
    BUSY,                  ///< Another operation or unclaimed open result occupies the single slot.
    INVALID_OPERATION,     ///< Cancellation/take operation does not match the current identity or phase.
    CHILD_FAILURE,  ///< Actual child queue/collector refusal; original typed child or producer cause remains available.
    INVALID_SOURCE, ///< Input family/provenance, slicing or actual source action failed; nested causes remain exact.
    CLOSED,         ///< Reserved root/child closure fences new work.
    RESOURCE_LIMIT, ///< Independent explicit backing capacity or representable size is exceeded.
    WORK_LIMIT,     ///< Supplied remaining work cannot fund this action.
    ALLOCATION_FAILURE, ///< Allocation failed before publication of the attempted action.
    IDENTITY_EXHAUSTED, ///< Non-reused operation identity cannot advance.
    UNSUPPORTED_FAMILY, ///< A native inspection method does not apply to this immutable family.
    REENTRANT           ///< A recursive owner call was rejected while an operation or callback is executing.
};

/** Exact failure and optional child cause, never a fabricated transport or peer error. */
struct StripeFailure {
    StripeError error;                                  ///< Root failure classification.
    std::optional<std::size_t> stripe{};                ///< Actual child when failure belongs to a stripe.
    std::optional<QueueFailure> child{};                ///< Original queue/collector cause.
    std::optional<QueueError> submission{};             ///< Original immediate producer refusal.
    std::optional<protocol::LiveError> event{};         ///< Original slice admission refusal.
    std::optional<core::SymbolStripeError> partition{}; ///< Original pure partition refusal.
};

/** Required independent root metadata/operation capacities; child capacities remain separate. */
struct StripedLimits {
    std::size_t residentBytes;  ///< Root state, STL controls/backing and live ports; excludes children and caller-owned
                                ///< inspection copies.
    std::size_t operationBytes; ///< One copied command/registration or source-plus-slice operation, including overlap.
    std::size_t sliceMetadataBytes;        ///< Complete operational slice State/view/control/alignment bytes.
    std::size_t sliceCopiedPayloadBytes;   ///< Copied legacy symbol/text/opaque/scalar bytes in one slice.
    std::size_t sliceRetainedPayloadBytes; ///< Whole pure backing retained by one slice.
};

/** One child's actual STREAM configuration; no common minimum or implicit quota multiplication. */
struct StreamStripeOptions {
    MuxLimits interests;              ///< This stripe's authoritative multi-consumer demand bounds.
    core::SubscriptionLimits desired; ///< This stripe's native union desired state.
    collectors::StreamLimits source;  ///< This stripe's actual native collector bounds.
    QueueLimits queue;                ///< This stripe's detached delivery reservations.
    core::SubscriptionSelectorPolicy selectorPolicy = core::SubscriptionSelectorPolicy::EXACT_ONLY;
    ///< Explicit existing exact/STREAM-All profile.
};

/** One child's actual TICKER configuration. */
struct TickerStripeOptions {
    MuxLimits interests;               ///< Independent authoritative demand bounds.
    collectors::TickerLimits source;   ///< Independent cache/view bounds.
    collectors::TickerOptions options; ///< Existing retention/version/selector profile.
    QueueLimits queue;                 ///< Independent detached delivery reservations.
};

/** One child's actual HISTORY configuration. */
struct HistoryStripeOptions {
    MuxLimits interests;                ///< Independent authoritative demand bounds.
    collectors::HistoryLimits source;   ///< Independent indexed cache/view bounds.
    collectors::HistoryOptions options; ///< Existing retention/snapshot/retransmission/selector profile.
    QueueLimits queue;                  ///< Independent detached delivery reservations.
};

/** Explicit caller turn credits; child/slice allowances are never silently replenished. */
struct StripedWork {
    std::size_t operations;   ///< Root attempts, including unsuccessful attempts.
    std::size_t visits;       ///< Root input, stripe, control and observation visits.
    std::size_t textUnits;    ///< Root owned-command symbol copy/comparison units.
    std::size_t bytes;        ///< Root construction/copy/disposal bytes.
    QueueWork child;          ///< Actual selected child owner action credits.
    QueueSubmitWork submit;   ///< Actual selected child command producer credits.
    protocol::LiveWork event; ///< Actual immutable event selection credits.
};

/** Producer-safe finite control token; owns child ports, never queues, callbacks or protocol state.
 * Last release follows the existing child MuxHandle abort rule. Distinct copies support concurrent control.
 */
class StripeClient final {
    std::shared_ptr<detail::StripePort> port_;
    friend class StripedCollector;
    explicit StripeClient(std::shared_ptr<detail::StripePort> port) noexcept;

    public:
    /** Construct an invalid inert token. */
    StripeClient() noexcept = default;
    /** @param other Existing complete producer token, retained without allocation. */
    StripeClient(const StripeClient &other) noexcept;
    /** @param other Token to transfer, made invalid. */
    StripeClient(StripeClient &&other) noexcept;
    /** @param other Existing token.
     * @return This producer token; the old last token records abort before retirement.
     */
    StripeClient &operator=(const StripeClient &other) noexcept;
    /** @param other Token to transfer; self-move preserves it.
     * @return This producer token.
     */
    StripeClient &operator=(StripeClient &&other) noexcept;
    /** Last public logical token records existing child abort even while a root operation retains port metadata. */
    ~StripeClient();
    /** @return Whether a complete registration's child ports are retained. */
    [[nodiscard]] bool valid() const noexcept;
    /** @return Number of enabled child registrations, not the complete striper count. */
    [[nodiscard]] std::size_t stripeCount() const noexcept;
    /** Request the existing reserved stop in every enabled child; starts no callback or disposal.
     * @param mode Existing graceful or escalating abort intent.
     */
    void requestStop(StopMode mode) const noexcept;
    /** Request existing reserved resume in every enabled child; cannot undo stop/reset gates. */
    void requestResume() const noexcept;
    /** @param stripe Actual configured stripe index.
     * @param sequence Exact notice sequence from that child.
     * @return Existing admission, or CLOSED for an absent child; not reset completion.
     */
    [[nodiscard]] ControlAdmission acknowledgeReset(std::size_t stripe, std::uint64_t sequence) const noexcept;
    /** @param stripe Actual configured stripe index.
     * @return Existing child notice or no pending/enabled notice; starts no progress.
     */
    [[nodiscard]] std::optional<QueueResetNotice> resetNotice(std::size_t stripe) const;
    /** @param stripe Actual configured stripe index.
     * @return Existing child terminal or no completed/enabled terminal.
     */
    [[nodiscard]] std::optional<QueueTerminal> terminal(std::size_t stripe) const;
};

/** Exactly one current root operation; accepted child work is not a rollback transaction. */
enum class StripeOperation {
    NONE,    ///< No operation has been accepted.
    OPEN,    ///< Open every enabled child registration.
    ADD,     ///< Submit routed additions through existing child tickets.
    REMOVE,  ///< Submit routed removals through existing child tickets.
    REPLACE, ///< Replace each enabled child's desired demand.
    CLEAR,   ///< Clear each enabled child's desired demand.
    STAGE,   ///< Admit and stage exact per-stripe source input.
    RESET    ///< Commit actual source reset and applicable global ACK in each child.
};
/** Completion means all required children completed the selected local barrier. */
enum class StripePhase {
    IDLE,       ///< No operation has started.
    RUNNING,    ///< One accepted operation retains its exact current continuation.
    FAILED,     ///< Permanent current-action failure requires explicit cancellation or stop.
    CANCELLING, ///< Accepted current work resolves while later submissions remain fenced.
    COMPLETE,   ///< Every required child completed the requested local barrier.
    CANCELLED   ///< Current input/cancellation cleanup completed; completed child effects remain.
};

/** Per-child exact current operation evidence, stable until the next operation begins. */
struct StripeChildResult {
    std::size_t stripe = 0;          ///< Actual stable partition index.
    bool required = false;           ///< This operation needs this child's action.
    bool submitted = false;          ///< Child accepted a command, event or reset request.
    bool completed = false;          ///< Actual requested local child barrier completed.
    std::size_t attempts = 0;        ///< Actual root attempts at this child.
    std::size_t sourceRecords = 0;   ///< Original exact rows assigned to this stripe.
    std::size_t admittedRecords = 0; ///< Rows whose child input ownership admission succeeded.
    std::size_t stagedRecords =
        0; ///< Original rows in a successful child stage call, including rows ignored by collector eligibility.
    std::optional<MuxCommandResult>
        command{}; ///< Actual immutable authoritative result; APPLIED is not synchronization.
    std::optional<QueueAdmission> admission{}; ///< Actual event ownership barrier.
    std::optional<ResetAdmission> reset{};     ///< Actual reset request identity.
    std::optional<StripeFailure> failure{};    ///< Preserved current child refusal.
};

/** Borrowed operation cut; result views require this root and no later begin/advance. */
struct StripedProgress {
    std::uint64_t operationId;    ///< Non-reused root-local identity; zero before the first operation.
    StripeOperation operation;    ///< Current/most recently completed operation kind.
    StripePhase phase;            ///< Truthful current phase.
    std::size_t completedStripes; ///< Exact completed required children.
    std::size_t requiredStripes;  ///< Exact required children; independent of skipped stripes.
    std::span<const StripeChildResult>
        children; ///< Current root-owned evidence; empty if a callback retired the public owner.
    std::optional<StripeFailure> failure; ///< Root/current child refusal, if present.
    bool closing;                         ///< Reserved root stop intent has fenced new operations.
    bool closed;                          ///< Every actual child reached its terminal barrier.
};

/** Detached per-child current scope and final count availability. */
struct StripeObservation {
    std::size_t stripe;            ///< Stable partition index.
    QueueResources resources;      ///< Actual independently enforced current queue/collector scopes.
    QueueCapacity capacity;        ///< This child's exact independent configured quotas.
    QueueState state;              ///< Actual child lifecycle at its moment of observation.
    std::uint64_t desiredRevision; ///< Actual authoritative child demand revision; children are sampled in index order.
    std::size_t desiredEntries;    ///< Exact child desired aggregate size, including one physical All occurrence.
    ResetStatus reset;             ///< Actual child reset/ACK phase.
    std::optional<QueueTerminal> terminal; ///< Full counts/loss evidence exists only after actual terminal.
    std::size_t acceptedSourceRecords = 0; ///< Exact root-observed successful input admissions for this child.
    std::size_t stagedSourceRecords =
        0; ///< Exact root-observed rows in successful collector stage calls, including ignored rows.
    bool countsSaturated = false; ///< Root child admission/stage/attempt counters saturated.
};

/** Detached aggregate diagnostic cut; children are observed sequentially, never as one atomic source instant. */
struct StripedSummary {
    std::uint64_t operationId = 0;         ///< Last accepted root operation identity at observation.
    std::size_t stripes = 0;               ///< Configured physical children.
    std::size_t terminalStripes = 0;       ///< Children with actual final outcomes.
    std::size_t desiredEntries = 0;        ///< Sum of physical child demand entries; All is counted per selected child.
    std::size_t acceptedSourceRecords = 0; ///< Sum of actual child ownership admissions.
    std::size_t stagedSourceRecords = 0;   ///< Sum of actual successful collector stage inputs, including ignored rows.
    std::size_t unsubmittedSourceRecords =
        0; ///< Root-owned rows excluded by explicit cancellation before child admission.
    std::optional<QueueCounts> finalCounts; ///< Available only when every child has actual terminal counts.
    bool countsSaturated = false;           ///< Any root or summed count overflowed.
    bool hadGap = false;             ///< Any already-terminal child or explicit unsubmitted root discard reports a gap.
    bool unknownEligibility = false; ///< Actual child terminal evidence; live-child eligibility is not invented.
    bool unknownUpstreamLoss = true; ///< Upstream continuity is not established by this local composition.
};

/** One real immutable-family composition of existing child MuxQueues and their native collectors.
 * Callers serialize owner operations/moves/destruction. Child queues keep existing multi-consumer
 * demand, delivery, callback and retained-lease rules. No transport, clock, worker or extra data queue exists.
 * Per-child effects are atomic under the child contract. Completed child effects survive cancellation;
 * an accepted pending command resolves before cancellation completes. No later child is submitted after cutoff.
 * Root control is request-only and is rechecked after callbacks; detached clients retain no operational owner.
 */
class StripedCollector final {
    std::shared_ptr<detail::StripeRoot> state_;
    explicit StripedCollector(std::shared_ptr<detail::StripeRoot> state) noexcept;
    [[nodiscard]] std::expected<std::uint64_t, StripeFailure>
    beginCommand(StripeOperation operation, const StripeClient &client,
                 std::span<const core::SubscriptionRequest> requests, std::span<const core::SubscriptionKey> keys,
                 StripedWork &work);

    public:
    /** Transfer the same serialized root; the source becomes invalid.
     * @param other Existing root to transfer.
     */
    StripedCollector(StripedCollector &&other) noexcept;
    /** Cut off the old root and transfer another; self-move preserves it.
     * @param other Root to transfer.
     * @return This handle.
     */
    StripedCollector &operator=(StripedCollector &&other) noexcept;
    /** Request abort and release this public owner; a currently executing callback lease retires on return. */
    ~StripedCollector();
    /** Create actual independently configured STREAM children before publication.
     * @param schema Existing immutable provenance, copied within child and root byte bounds.
     * @param striper Owned immutable physical-symbol partition.
     * @param options Exactly one independently configured option per stripe, borrowed through creation only.
     * @param limits Required root metadata/operation grants; no new stripe/client count ceiling.
     * @param work Actual initialization credits across every child.
     * @return Complete root or atomic factory refusal; no input owner or callback is adopted on failure.
     */
    [[nodiscard]] static std::expected<StripedCollector, StripeFailure>
    createStream(const core::LocalSchema &schema, core::SymbolStriper striper,
                 std::span<const StreamStripeOptions> options, StripedLimits limits, StripedWork &work);
    /** @param schema Existing indexed/ordinary provenance.
     * @param striper Immutable physical partition.
     * @param options Exact independent per-stripe TICKER settings.
     * @param limits Explicit root bounds.
     * @param work Actual child/root initialization credits.
     * @return Complete TICKER composition or atomic factory refusal.
     */
    [[nodiscard]] static std::expected<StripedCollector, StripeFailure>
    createTicker(const core::LocalSchema &schema, core::SymbolStriper striper,
                 std::span<const TickerStripeOptions> options, StripedLimits limits, StripedWork &work);
    /** @param schema Existing indexed/ordinary provenance.
     * @param striper Immutable physical partition.
     * @param options Exact independent per-stripe HISTORY settings.
     * @param limits Explicit root bounds.
     * @param work Actual child/root initialization credits.
     * @return Complete HISTORY composition or atomic factory refusal.
     */
    [[nodiscard]] static std::expected<StripedCollector, StripeFailure>
    createHistory(const core::LocalSchema &schema, core::SymbolStriper striper,
                  std::span<const HistoryStripeOptions> options, StripedLimits limits, StripedWork &work);
    /** Begin one registration across proven intersecting stripes; no child callback starts inline.
     * @param limits Exactly one independent ClientLimits value per complete striper stripe.
     * @param consumer Existing owning Mux consumer; captures remain outside root/child byte scopes.
     * @param stripeHint Owned static stripe hint for candidate-child selection; the independent hard FeedFilter stays
     * active at its upper boundary.
     * @param work Actual copy/reservation credits.
     * @return Operation identity; takeClient is available only after every required child opened.
     */
    [[nodiscard]] std::expected<std::uint64_t, StripeFailure>
    beginOpen(std::span<const ClientLimits> limits, std::shared_ptr<MuxConsumer> consumer,
              std::optional<core::SymbolStripeFilter> stripeHint, StripedWork &work);
    /** @return Complete new client once, or a typed incomplete/wrong-operation refusal. */
    [[nodiscard]] std::expected<StripeClient, StripeFailure> takeClient();
    /** @param client Complete registration from this root.
     * @param requests Borrowed exact/All demands, copied before acceptance; All fans out to every enabled stripe.
     * @param work Actual root copy/child submit credits.
     * @return Accepted single operation identity, or refusal with no child submission.
     */
    [[nodiscard]] std::expected<std::uint64_t, StripeFailure>
    beginAdd(const StripeClient &client, std::span<const core::SubscriptionRequest> requests, StripedWork &work);
    /** @param client Complete registration.
     * @param keys Borrowed exact/All keys to remove, copied before acceptance.
     * @param work Actual root copy/child credits.
     * @return Accepted identity or refusal before child work.
     */
    [[nodiscard]] std::expected<std::uint64_t, StripeFailure>
    beginRemove(const StripeClient &client, std::span<const core::SubscriptionKey> keys, StripedWork &work);
    /** @param client Complete registration.
     * @param requests Complete desired replacement, copied before acceptance.
     * @param work Actual root copy/child credits.
     * @return Accepted identity; each child replacement remains individually atomic.
     */
    [[nodiscard]] std::expected<std::uint64_t, StripeFailure>
    beginReplace(const StripeClient &client, std::span<const core::SubscriptionRequest> requests, StripedWork &work);
    /** @param client Complete registration.
     * @param work Actual root copy/child credits.
     * @return Accepted clear operation identity, or refusal before child work.
     */
    [[nodiscard]] std::expected<std::uint64_t, StripeFailure> beginClear(const StripeClient &client, StripedWork &work);
    /** @param event Complete existing-family DATA event, moved only after full root/slice preflight succeeds.
     * @param work Actual validation/index/copy credits; successful charges are never refunded.
     * @return Accepted operation identity, or refusal leaving input and completed child effects unchanged.
     */
    [[nodiscard]] std::expected<std::uint64_t, StripeFailure> beginStage(protocol::LiveEvent &&event,
                                                                         StripedWork &work);
    /** @param work Actual reserved reset-request and continuing owner credits.
     * @return Accepted reset operation; complete means every child committed its source reset/global ACK,
     * while client-specific notices still require their exact independent acknowledgments.
     */
    [[nodiscard]] std::expected<std::uint64_t, StripeFailure> beginReset(StripedWork &work);
    /** Record request-only cancellation of the exact current operation.
     * @param operationId Current non-reused operation identity.
     * @return Success/cutoff only; already accepted child work is resolved by advance and never rolled back.
     */
    [[nodiscard]] std::expected<void, StripeFailure> requestCancel(std::uint64_t operationId) noexcept;
    /** Advance at most one bounded root/child step, or one fair existing delivery action while idle.
     * May invoke an existing consumer outside locks; the public root can request stop or be destroyed there.
     * @param work Caller turn credits, retained across retries without replenishment.
     * @return Current truthful operation/close cut, or a detached root precondition/work refusal.
     */
    [[nodiscard]] std::expected<StripedProgress, StripeFailure> advance(StripedWork &work);
    /** @return Current borrowed operation/close cut without starting work. */
    [[nodiscard]] StripedProgress progress() const noexcept;
    /** Record reserved context-wide stop; abort escalates graceful and starts no child call inline.
     * @param mode Existing honest drain or abort policy.
     */
    void requestStop(StopMode mode) noexcept;
    /** @return Fixed native delivery family; invalid roots return no family. */
    [[nodiscard]] std::optional<core::SubscriptionFamily> family() const noexcept;
    /** @return Configured physical stripe count, or zero after move. */
    [[nodiscard]] std::size_t stripeCount() const noexcept;
    /** @return Actual charged root data/control backing; child scopes and the pure counter control are separate. */
    [[nodiscard]] std::size_t residentBytes() const noexcept;
    /** Read every actual child scope without advancing delivery.
     * @param work Actual observation/copy credits.
     * @return Detached per-stripe scope/reset/final-count availability; no loss count is invented for live children.
     */
    [[nodiscard]] std::expected<std::vector<StripeObservation>, StripeFailure> observations(StripedWork &work) const;
    /** Sum root admissions and actual child demand/final evidence without progressing delivery.
     * @param work Explicit finite observation credits; children are sampled sequentially in stripe order.
     * @return Detached aggregate cut. Live children keep finalCounts unavailable; root cancellation stays separate.
     */
    [[nodiscard]] std::expected<StripedSummary, StripeFailure> summary(StripedWork &work) const;
    /** Read every enabled child's exact desired aggregate, preserving stripe identity and duplicate All occurrences.
     * @param stripe Stable configured partition index.
     * @return Borrowed authoritative desired entries until effective child mutation, or an empty invalid-stripe view.
     */
    [[nodiscard]] std::span<const core::SubscriptionRequest> aggregateEntries(std::size_t stripe) const noexcept;

    /** One owning per-stripe HISTORY key/status observation, retaining no operational owner. */
    struct RangeObservation {
        std::size_t stripe;          ///< Actual child partition.
        QueueRangeObservation value; ///< Existing owning exact key and AS-OF coverage/status evidence.
    };

    /** Read current HISTORY request/source diagnostics across every enabled stripe.
     * @param client Existing complete client from this root.
     * @param work Actual root and child observation/copy credits.
     * @return Owning stripe/key/status rows, or explicit unsupported-family/child refusal; no partial result escapes.
     */

    [[nodiscard]] std::expected<std::vector<RangeObservation>, StripeFailure> rangeSnapshot(const StripeClient &client,
                                                                                            StripedWork &work) const;
    /** Dispatch an exact retained HISTORY interval to its physical child.
     * @param client Existing registration.
     * @param query Exact physical key and bounded inclusive interval.
     * @param work Actual root and native query credits.
     * @return Existing borrowed source cut, or explicit family/client/partition/child refusal.
     */
    [[nodiscard]] std::expected<collectors::HistoryRetainedRange, StripeFailure>
    retainedHistoryRange(const StripeClient &client, const collectors::HistoryRetainedQuery &query,
                         StripedWork &work) const;
    /** Visit one current exact TICKER cache value through the existing admission boundary.
     * @param client Existing registration.
     * @param key Exact physical key, not an event-source or channel identity.
     * @param work Actual root and native lookup credits.
     * @param context Caller-owned state borrowed during the synchronous callback.
     * @param visitor Existing nonthrowing borrowed-value visitor; retain no input and do not reenter the root.
     * @return Existing disposition or explicit family/client/partition/child refusal.
     */
    [[nodiscard]] std::expected<TickerLatestObservation, StripeFailure>
    visitTickerLatest(const StripeClient &client, core::SubscriptionKey key, StripedWork &work, void *context,
                      void (*visitor)(void *, const collectors::TickerValue &) noexcept) const;
};
} // namespace org::ttldtor::qdcxx::mux
