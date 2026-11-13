// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/core/subscription.hpp"

namespace org::ttldtor::qdcxx::core {

/** Own one prepared replacement and its share of the owner's simultaneous backing allowance. */
class SubscriptionState::Candidate {
    std::shared_ptr<const SubscriptionMembership::Identity> identity_;
    std::unique_ptr<SubscriptionEntry[]> entries_;
    std::unique_ptr<char16_t[]> symbols_;
    std::uint64_t baseRevision_;
    std::uint64_t lastActivation_ = 0;
    std::size_t size_ = 0;
    std::size_t symbolUnits_ = 0;
    bool changed_ = false;

    friend class SubscriptionState;
    friend struct detail::SubscriptionStateAccess;

    /** Hold the sole candidate reservation for the current owner revision.
     * @param owner Serialized owner with no other outstanding candidate.
     */
    explicit Candidate(const SubscriptionState &owner) noexcept;

    public:
    /** Move the backing and reservation without invalidating the prepared input.
     * @param other Candidate to leave without an owner reservation.
     */
    Candidate(Candidate &&other) noexcept = default;
    /** Release candidate or retired published backing and the owner reservation. */
    ~Candidate();
};

namespace detail {

/** Prepare and publish desired-state replacements on one serialized owner thread. */
struct SubscriptionStateAccess {
    using PreparedReplace = SubscriptionState::Candidate; ///< Sole owning candidate; keep on the serialized thread.

    /** Stage a complete replacement without publishing entries, memberships or revisions.
     * @param owner Current desired-state owner, which may later move with its reservation.
     * @param requests Borrowed inputs, copied as needed before return.
     * @param work Remaining turn credits, including disposal work for either outcome.
     * @return One owning candidate, or typed rejection with the current state preserved. Mutations on the same
     * owner report INVALID_STATE while the candidate is outstanding. Reads remain available.
     */
    [[nodiscard]] static std::expected<PreparedReplace, SubscriptionError>
    prepareReplace(SubscriptionState &owner, std::span<const SubscriptionRequest> requests, SubscriptionWork &work) {
        return owner.prepare(SubscriptionState::Command::REPLACE, requests, {}, work);
    }

    /** Borrow staged entries for private collector planning until the next owner move or candidate consumption.
     * @param owner The logical owner at the prepared revision.
     * @param candidate The outstanding replacement.
     * @return Staged entries, or an empty view for an invalid candidate. Keep copied provisional memberships
     * in unpublished scratch and destroy them before abandoning the candidate.
     */
    [[nodiscard]] static std::span<const SubscriptionEntry> entries(const SubscriptionState &owner,
                                                                    const PreparedReplace &candidate) noexcept;

    /** Inspect the revision that a valid replacement will publish.
     * @param owner The logical owner at the prepared revision.
     * @param candidate The outstanding replacement.
     * @return The resulting revision, or none when ownership or base revision does not match.
     */
    [[nodiscard]] static std::optional<std::uint64_t> revision(const SubscriptionState &owner,
                                                               const PreparedReplace &candidate) noexcept;

    /** Check membership against staged desired state, with the same scan bound as the published owner.
     * @param owner The logical owner at the prepared revision.
     * @param candidate The outstanding replacement.
     * @param membership A detached membership tested by precharged collector planning.
     * @return True exactly when the candidate is valid and retains this membership.
     */
    [[nodiscard]] static bool isCurrent(const SubscriptionState &owner, const PreparedReplace &candidate,
                                        const SubscriptionMembership &membership) noexcept;

    /** Consume the candidate and publish using only nonthrowing operations.
     * @param owner The same logical owner at the prepared revision.
     * @param candidate Complete owned replacement, discarded even when ownership or revision does not match.
     * @return Published revision or INVALID_STATE with no change to the supplied owner.
     */
    [[nodiscard]] static std::expected<SubscriptionChange, SubscriptionError>
    commitReplace(SubscriptionState &owner, PreparedReplace candidate) noexcept;
};

} // namespace detail
} // namespace org::ttldtor::qdcxx::core
