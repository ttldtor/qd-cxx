// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/collectors/ticker.hpp"

#include "../core/subscription_state_access.hpp"

namespace org::ttldtor::qdcxx::collectors {

/** Own a prepared desired replacement and reserve its collector's preflighted cache/order state. */
class TickerCollector::PreparedReplace {
    using CoreReplace = core::detail::SubscriptionStateAccess::PreparedReplace;

    TickerViewToken view_;
    std::optional<CoreReplace> desired_;
    std::size_t ordered_;

    friend class TickerCollector;
    friend struct detail::TickerCollectorAccess;

    /** Reserve the existing cache state and input-order scratch until commit or abandonment.
     * @param view Live selection in a collector with no outstanding candidate.
     * @param desired Owned core replacement for that exact view.
     * @param ordered Number of valid preplanned cache indices.
     */
    PreparedReplace(const TickerViewToken &view, CoreReplace &&desired, std::size_t ordered) noexcept;

    public:
    /** Move the replacement and leave the source without a collector reservation.
     * @param other The candidate whose ownership transfers.
     */
    PreparedReplace(PreparedReplace &&other) noexcept = default;
    /** Release the desired candidate and cache reservation without changing published state. */
    ~PreparedReplace();
};

namespace detail {

/** Serialized owner-only access to a staged replacement and its cache maintenance. */
struct TickerCollectorAccess {
    using PreparedReplace = TickerCollector::PreparedReplace; ///< Owning transaction tied to one collector/view.

    /** Precharge cache maintenance and prepare one complete desired replacement.
     * @param owner Serialized collector with no outstanding candidate.
     * @param view Exact local view.
     * @param requests Borrowed inputs, copied as needed before return.
     * @param subscriptionWork Actual remaining desired command credits.
     * @param work Actual remaining collector maintenance and release credits.
     * @return Candidate or typed failure preserving desired state, cache, queues and offers.
     * Mutating collector calls report INVALID_STATE while a candidate owns the reservation.
     */
    [[nodiscard]] static std::expected<PreparedReplace, TickerCommandError>
    prepareReplace(TickerCollector &owner, const TickerViewToken &view,
                   std::span<const core::SubscriptionRequest> requests, core::SubscriptionWork &subscriptionWork,
                   TickerWork &work) {
        return owner.prepareReplace(view, requests, subscriptionWork, work);
    }

    /** Borrow staged desired entries solely for unpublished collector/queue planning.
     * @param owner The logical collector that issued the candidate.
     * @param candidate The outstanding replacement.
     * @return Staged entries, or an empty view for an invalid candidate. Do not retain the view through owner
     * movement or candidate consumption. Destroy copied provisional tokens before abandoning the candidate.
     */
    [[nodiscard]] static std::span<const core::SubscriptionEntry> entries(const TickerCollector &owner,
                                                                          const PreparedReplace &candidate) noexcept {
        const auto *desired = owner.desired(candidate.view_);

        return desired ? core::detail::SubscriptionStateAccess::entries(*desired, *candidate.desired_)
                       : std::span<const core::SubscriptionEntry>{};
    }

    /** Inspect the revision that a valid replacement will publish.
     * @param owner The logical collector that issued the candidate.
     * @param candidate The outstanding replacement.
     * @return The resulting revision, or none when ownership or the desired revision does not match.
     */
    [[nodiscard]] static std::optional<std::uint64_t> revision(const TickerCollector &owner,
                                                               const PreparedReplace &candidate) noexcept {
        const auto *desired = owner.desired(candidate.view_);

        return desired ? core::detail::SubscriptionStateAccess::revision(*desired, *candidate.desired_) : std::nullopt;
    }

    /** Consume the candidate and publish precharged changes using nonthrowing operations.
     * @param owner The logical collector that issued the candidate.
     * @param candidate The complete replacement, discarded even when owner verification fails.
     * @return Desired application barrier or a typed rejection with no publication.
     */
    [[nodiscard]] static std::expected<core::SubscriptionChange, TickerCommandError>
    commitReplace(TickerCollector &owner, PreparedReplace candidate) noexcept {
        return owner.publishReplace(candidate);
    }
};

} // namespace detail
} // namespace org::ttldtor::qdcxx::collectors
