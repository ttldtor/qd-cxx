// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/subscription.hpp"

namespace org::ttldtor::qdcxx::mux {

/** Fixed per-bank capacities, including exact text owned independently of the desired map. */
struct DemandLimits {
    std::size_t entries;     ///< Maximum distinct exact keys.
    std::size_t symbolUnits; ///< Maximum total UTF-16 units; zero permits only empty symbols.
};

/** Caller-turn credits; completed charges are kept on rejection. */
struct DemandWork {
    std::size_t operations;  ///< Remaining polls and resets.
    std::size_t visits;      ///< Remaining entry inspections, comparisons and fixed metadata writes.
    std::size_t symbolUnits; ///< Remaining examined/copied UTF-16 units.
};

/** Local errors leave the admitted and pending demand unchanged. */
enum class DemandError {
    INVALID_STATE,         ///< Moved-from owner.
    INVALID_CONFIGURATION, ///< Unsupported immutable family.
    INVALID_DESIRED,       ///< A desired entry is invalid for this exact subscription family.
    INVALID_SEQUENCE,      ///< Acknowledgment does not name the outstanding change.
    RESOURCE_LIMIT,        ///< Explicit backing limits or representable allocation size exceeded.
    WORK_LIMIT,            ///< Caller-turn credit exhausted.
    IDENTITY_EXHAUSTED,    ///< Non-reused change sequence cannot advance.
    ALLOCATION_FAILURE     ///< Fixed-bank allocation failed before creation finished.
};

/** A change to the source subscription state. */
enum class DemandAction {
    ADD,   ///< First interest, or a changed HISTORY bound for an existing key.
    REMOVE ///< Last interest withdrawn; no bound is sent.
};

/** One pending upstream change. The request's text borrows the tracker. */
struct DemandChange {
    std::uint64_t sequence;            ///< Nonzero local sequence, never reused by this owner.
    DemandAction action;               ///< Selected change to the acknowledged map.
    core::SubscriptionRequest request; ///< Exact key and optional raw HISTORY ADD bound.
};

/** Reconcile one family's exact desired union with an acknowledged upstream subscription map.
 * Serialize all calls. Polling, acknowledgment and reset do not allocate. A pending
 * change always takes precedence over later desired input, so an in-flight frame can
 * complete against its original key. Reset abandons a pending change and all previous
 * acknowledgments on source generation loss. The next poll then advertises current demand.
 */
class DemandTracker {
    struct State;
    std::unique_ptr<State> state_;

    explicit DemandTracker(std::unique_ptr<State> state) noexcept;

    public:
    /** Release the banks and invalidate every borrowed change. */
    ~DemandTracker();
    /** Transfer source state and invalidate outstanding borrowed change views. */
    DemandTracker(DemandTracker &&other) noexcept;
    /** Replace source state and invalidate outstanding borrowed change views. */
    DemandTracker &operator=(DemandTracker &&other) noexcept;

    /** Reserve both complete banks before any change can be issued.
     * @param family Immutable exact subscription family.
     * @param limits Fixed capacities for each bank.
     * @return Empty tracker or typed refusal.
     */
    [[nodiscard]] static std::expected<DemandTracker, DemandError> create(core::SubscriptionFamily family,
                                                                          DemandLimits limits);

    /** Poll one outstanding change or find a change against the acknowledged map.
     * @param desired Complete exact union from the serialized MuxState or MuxQueue. Duplicate keys are refused.
     * @param work Actual poll, comparison and copy credits.
     * @return Borrowed change until acknowledgment, reset, move or destruction. Empty means converged.
     */
    [[nodiscard]] std::expected<std::optional<DemandChange>, DemandError>
    next(std::span<const core::SubscriptionRequest> desired, DemandWork &work);

    /** Commit one change after the upstream output has admitted the entire command.
     * @param sequence Exact sequence issued by this tracker.
     * @return Success or stale/invalid refusal. This is a local barrier, not a remote receipt.
     */
    [[nodiscard]] std::expected<void, DemandError> acknowledge(std::uint64_t sequence) noexcept;

    /** Forget all previous source-generation state without reusing a sequence.
     * @param work Credits for bounded retirement of both banks.
     * @return Success or typed refusal preserving the old generation state.
     */
    [[nodiscard]] std::expected<void, DemandError> reset(DemandWork &work);
};

} // namespace org::ttldtor::qdcxx::mux
